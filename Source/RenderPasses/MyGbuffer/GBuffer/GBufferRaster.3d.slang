/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "Scene/VertexAttrib.slangh"

import Scene.Raster;
import Utils.Math.MathHelpers;
import GBufferHelpers;
import Rendering.Materials.TexLODHelpers;

#ifndef INTERPOLATION_MODE
#define INTERPOLATION_MODE linear
#endif

cbuffer PerFrameCB
{
    uint2 gFrameDim;
};

// GBuffer channels
struct GBufferPSOut
{
    float4 posW         : SV_TARGET0;
    float4 normW        : SV_TARGET1;
    float4 tangentW     : SV_TARGET2;
    float4 faceNormalW  : SV_TARGET3;
    float2 texC         : SV_TARGET4;
    float4 texGrads     : SV_TARGET5;
    float2 mvec         : SV_TARGET6;
    uint4  mtlData      : SV_TARGET7;
};

// GBufferRaster channels
RasterizerOrderedTexture2D<PackedHitInfo> gVBuffer;
RasterizerOrderedTexture2D<float4> gDiffOpacity;
RasterizerOrderedTexture2D<float4> gSpecRough;
RasterizerOrderedTexture2D<float4> gEmissive;
RasterizerOrderedTexture2D<float4> gViewW;
RasterizerOrderedTexture2D<float2> gPosNormalFwidth;
RasterizerOrderedTexture2D<float2> gLinearZAndDeriv;
RasterizerOrderedTexture2D<uint> gRawInstanceID;
RasterizerOrderedTexture2D<float4> gPosL;

#define is_valid(name) (is_valid_##name != 0)

struct MyVSOut
{
    INTERPOLATION_MODE float3 normalW : NORMAL;    ///< Shading normal in world space (not normalized!).
    INTERPOLATION_MODE float4 tangentW : TANGENT;  ///< Shading tangent in world space (not normalized!).
    INTERPOLATION_MODE float2 texC : TEXCRD;       ///< Texture coordinate.
    INTERPOLATION_MODE float3 posW : POSW;         ///< Position in world space.
    INTERPOLATION_MODE float4 prevPosH : PREVPOSH; ///< Position in clip space for the previous frame.
    INTERPOLATION_MODE float4 posL : POSL;         ///< Position in original model space.

    // Per-triangle data
    nointerpolation GeometryInstanceID instanceID : INSTANCE_ID; ///< Geometry instance ID.
    nointerpolation uint rawInstanceID : RAW_INSTANCE_ID;        ///< Raw geometry instance ID.
    nointerpolation uint materialID : MATERIAL_ID;               ///< Material ID.

    float4 posH : SV_POSITION;
};

MyVSOut vsMain(VSIn vIn)
{
    MyVSOut vOut;
    const GeometryInstanceID instanceID = { vIn.instanceID };

    float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    float3 posW = mul(worldMat, float4(vIn.pos, 1.f)).xyz;
    vOut.posW = posW;
    vOut.posH = mul(gScene.camera.getViewProj(), float4(posW, 1.f));

    vOut.instanceID = instanceID;
    vOut.materialID = gScene.getMaterialID(instanceID);

    vOut.texC = vIn.texC;
    vOut.normalW = mul(gScene.getInverseTransposeWorldMatrix(instanceID), vIn.unpack().normal);
    float4 tangent = vIn.unpack().tangent;
    vOut.tangentW = float4(mul((float3x3)gScene.getWorldMatrix(instanceID), tangent.xyz), tangent.w);

    // Compute the vertex position in the previous frame.
    float3 prevPos = vIn.pos;
    GeometryInstanceData instance = gScene.getGeometryInstance(instanceID);
    if (instance.isDynamic())
    {
        uint prevVertexIndex = gScene.meshes[instance.geometryIndex].prevVbOffset + vIn.vertexID;
        prevPos = gScene.prevVertices[prevVertexIndex].position;
    }
    float3 prevPosW = mul(gScene.getPrevWorldMatrix(instanceID), float4(prevPos, 1.f)).xyz;
    vOut.prevPosH = mul(gScene.camera.data.prevViewProjMatNoJitter, float4(prevPosW, 1.f));

    vOut.posL = float4(vIn.pos, 1.f);
    vOut.rawInstanceID = vIn.instanceID;

    return vOut;
}

VSOut CopyToVSOut(MyVSOut vsOut) {
    VSOut vOut;
    vOut.posH = vsOut.posH;
    vOut.posW = vsOut.posW;
    vOut.normalW = vsOut.normalW;
    vOut.tangentW = vsOut.tangentW;
    vOut.texC = vsOut.texC;
    vOut.instanceID = vsOut.instanceID;
    vOut.materialID = vsOut.materialID;
    vOut.prevPosH = vsOut.prevPosH;
    return vOut;
}

float2 computeMotionVector(const VSOut vsOut, const int2 ipos)
{
    float2 pixelPos = ipos + float2(0.5, 0.5); // Current sample in pixel coords.
    float4 prevPosH = vsOut.prevPosH; // Sample in previous frame in clip space coords, no jittering applied.
    return calcMotionVector(pixelPos, prevPosH, gFrameDim) + float2(gScene.camera.data.jitterX, -gScene.camera.data.jitterY); // Remove camera jitter from motion vector
}

[earlydepthstencil]
GBufferPSOut psMain(MyVSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics)
{
    // Using vOut.posH.xy as pixel coordinate since it has the SV_Position semantic.
    int2 ipos = int2(vsOut.posH.xy);

    VSOut rawVSOut = CopyToVSOut(vsOut);

    float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    VertexData v = prepareVertexData(rawVSOut, faceNormal);
    let lod = ImplicitLodTextureSampler();

#if USE_ALPHA_TEST
    if (gScene.materials.alphaTest(v, vsOut.materialID, lod)) discard;
#endif
    const float3 viewDir = normalize(gScene.camera.getPosition() - v.posW);
    ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

#if ADJUST_SHADING_NORMALS
    adjustShadingNormal(sd, v);
#endif

    // Create BSDF instance.
    let bsdf = gScene.materials.getBSDF(sd, lod);

    const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    const float4 texGrads = float4(ddx(sd.uv), ddy(sd.uv));
    const float2 mvec = computeMotionVector(rawVSOut, ipos);

    GBufferPSOut psOut = {};

    // Store render target outputs.
    if (is_valid(gPosW))            psOut.posW          = gbuf.posW;
    if (is_valid(gNormW))           psOut.normW         = gbuf.normW;
    if (is_valid(gTangentW))        psOut.tangentW      = gbuf.tangentW;
    if (is_valid(gFaceNormalW))     psOut.faceNormalW   = gbuf.faceNormalW;
    if (is_valid(gTexC))            psOut.texC          = gbuf.texC;
    if (is_valid(gTexGrads))        psOut.texGrads      = texGrads;
    if (is_valid(gMotionVector))    psOut.mvec          = mvec;
    if (is_valid(gMaterialData))    psOut.mtlData       = gbuf.mtlData;

    // Store UAV outputs.
    if (is_valid(gViewW))           gViewW[ipos]        = float4(sd.V, 0.f);
    if (is_valid(gDiffOpacity))     gDiffOpacity[ipos]  = gbuf.diffuseOpacity;
    if (is_valid(gSpecRough))       gSpecRough[ipos]    = gbuf.specRough;
    if (is_valid(gEmissive))        gEmissive[ipos]     = gbuf.emissive;
    if (is_valid(gRawInstanceID))   gRawInstanceID[ipos] = vsOut.rawInstanceID;
    if (is_valid(gPosL))            gPosL[ipos] = vsOut.posL;

    // Length of derivatives of position and normal
    if (is_valid(gPosNormalFwidth))
    {
        gPosNormalFwidth[ipos] = float2(length(fwidth(sd.posW)), length(fwidth(sd.N)));
    }

    // Linear z and its derivative
    if (is_valid(gLinearZAndDeriv))
    {
        const float linearZ = vsOut.posH.z * vsOut.posH.w;
        gLinearZAndDeriv[ipos] = float2(linearZ, max(abs(ddx(linearZ)), abs(ddy(linearZ))));
    }

    // Note on barycentrics:
    // The barycentric weights provided to pixel shader correspond to vertices A, B, C of the rasterized triangle.
    // For triangle strips, every odd primitive has the order for vertices B and C flipped. We don't handle triangle
    // strips as DXR does not support them and Falcor uses triangle lists exclusively in its scene representation.
    // DXR intersection attributes store barycentric weights in a float2 for triangle vertices B and C.
    // This is what we store in the hit info.

    // Store hit information.
    if (is_valid(gVBuffer))
    {
        TriangleHit triangleHit;
        triangleHit.instanceID = vsOut.instanceID;
        triangleHit.primitiveIndex = triangleIndex;
        triangleHit.barycentrics = barycentrics.yz;
        gVBuffer[ipos] = triangleHit.pack();
    }

    return psOut;
}
