import Scene.Raster;
import GBufferHelpers;

#ifndef INTERPOLATION_MODE
#define INTERPOLATION_MODE linear
#endif

Texture2D<float2> gFirstLinearZBuffer;
Texture2D<float4> gFirstNormWSBuffer;
RasterizerOrderedTexture2D<float4> gLinearZThresholdBuffer; // 0: threshold, 1: max


cbuffer PerFrameCB {
    float gEps;
    float gNormalThreshold;
    bool maxConstraint;
    bool normalConstraint;
    uint2 curDim;
    float renderScale;
};

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
    nointerpolation uint rawInstanceID : RAW_INSTANCE_ID;       ///< Raw geometry instance ID.
    nointerpolation uint materialID : MATERIAL_ID;               ///< Material ID.

    float4 posH : SV_POSITION;
};

// Default Vertex Shader
MyVSOut vsMain(VSIn vIn) {
    // VSOut vOut;
    // const GeometryInstanceID instanceID = { vIn.instanceID };

    // float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    // float3 posW = mul(worldMat, float4(vIn.pos, 1.f)).xyz;
    // vOut.posW = posW;
    // vOut.posH = mul(gViewProjMat, float4(posW, 1.f));
    // return vOut;

    // return defaultVS(vIn);

    MyVSOut vOut;
    const GeometryInstanceID instanceID = { vIn.instanceID };

    float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    float3 posW = mul(worldMat, float4(vIn.pos, 1.f)).xyz;
    vOut.posW = posW;
    vOut.posH = mul(gScene.camera.getViewProj(), float4(posW, 1.f));
    vOut.posH.xy /= renderScale;

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

#define is_valid(name) (is_valid_##name != 0)

struct PSOut {
    float4 gDebug : SV_TARGET0;
    float2 gLinearZ : SV_TARGET1;
    // float4 gMask : SV_TARGET1;
    float4 gNormWS : SV_TARGET2;
    float4 gDiffOpacity : SV_TARGET3;
    float4 gPosWS : SV_TARGET4;
    float  gDepth : SV_TARGET5;
    uint   gInstanceID : SV_TARGET6;
    float4 gPosL   : SV_TARGET7;
    // float4 tangentWS : SV_TARGET2;
    // float4 posWS : SV_TARGET3;
};

PSOut psMain(MyVSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) : SV_TARGET {

    int2 ipos = int2(vsOut.posH.xy) - (int2)curDim / 2;
    ipos = int2(ipos * renderScale) + (int2)curDim / 2;

    VSOut rawVSOut = CopyToVSOut(vsOut);


    // Not a valid second layer gbuffer
    float linearZ = vsOut.posH.z * vsOut.posH.w;

    float threshold;
    if (maxConstraint) {
        gLinearZThresholdBuffer[ipos][1] = max(gLinearZThresholdBuffer[ipos][1], linearZ);
        threshold = min(gFirstLinearZBuffer[ipos][0] - 0.00001 + gEps, gLinearZThresholdBuffer[ipos][1]);
    }
    else {
        threshold = gFirstLinearZBuffer[ipos][0] - 0.00001 + gEps;
    }

    gLinearZThresholdBuffer[ipos][0] = (threshold + gFirstLinearZBuffer[ipos][0]) * 0.5;

    if (ipos.x >= 0 && ipos.x <= curDim.x && ipos.y >= 0 && ipos.y <= curDim.y && linearZ < threshold) // pecision err
        discard;

    PSOut psOut = {};

    float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    VertexData v = prepareVertexData(rawVSOut, faceNormal);
    let lod = ImplicitLodTextureSampler();

    if (gScene.materials.alphaTest(v, vsOut.materialID, lod)) discard;

    const float3 viewDir = normalize(gScene.camera.getPosition() - v.posW);
    ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

    // Create BSDF instance.
    let bsdf = gScene.materials.getBSDF(sd, lod);

    const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    psOut.gDebug = float4(vsOut.posH.z * vsOut.posH.w, 0, 0, 0);
    psOut.gNormWS = gbuf.normW;
    psOut.gDiffOpacity = gbuf.diffuseOpacity;
    // psOut.gMask = float4(1., 0., 0., 0.);
    psOut.gPosWS = gbuf.posW;
    psOut.gDepth = vsOut.posH.z;
    psOut.gInstanceID = vsOut.rawInstanceID;
    psOut.gPosL = vsOut.posL;
    psOut.gLinearZ = float2(linearZ, max(abs(ddx(linearZ)), abs(ddy(linearZ))));

    if (normalConstraint) {
        if (dot(gbuf.normW.xyz, normalize(gbuf.posW.xyz - gScene.camera.getPosition())) > gNormalThreshold)
            discard;
    }

    return psOut;
}


// Old Codes
    // gTargetPosWSBuffer[ipos] = float4(gPosWSBuffer[ipos].xyz, 0);

    // // Not a valid second layer gbuffer
    // // float linearZ = (vsOut.posH.z * vsOut.posH.w - gEps);
    // // if(linearZ <= gFirstLinearZBuffer[ipos][0] - 0.00001)
    // //     discard;

    // PSOut psOut = {};

    // // float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    // // VertexData v = prepareVertexData(vsOut, faceNormal);
    // // let lod = ImplicitLodTextureSampler();

    // // const float3 viewDir = normalize(gScene.camera.getPosition() - v.posW);
    // // ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

    // // // Create BSDF instance.
    // // let bsdf = gScene.materials.getBSDF(sd, lod);

    // // const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    // psOut.gDebug = float4(vsOut.posW, 0);
    // // psOut.gNormW = float4(vsOut.normalW, 0);
    // // psOut.gDiffOpacity = gbuf.diffuseOpacity;
