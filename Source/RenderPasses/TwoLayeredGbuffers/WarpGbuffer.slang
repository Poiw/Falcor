import Scene.Raster;
import GBufferHelpers;

Texture2D<float4> gPrevPosWSBuffer;
// RasterizerOrderedTexture2D<float4> gPosWSBufferTemp;

cbuffer PerFrameCB
{
    uint2 gFrameDim;
    float4x4 gCenterMatrix;
};

// Default Vertex Shader
VSOut vsMain(VSIn vIn) {

    return defaultVS(vIn);

}


#define is_valid(name) (is_valid_##name != 0)

struct PSOut {
    float4 gDebug : SV_TARGET0;
    float4 gMotionVector : SV_TARGET1;
    float4 gPosWS : SV_TARGET2;
    // float4 gDiffOpacity : SV_TARGET2;
    // float4 tangentWS : SV_TARGET2;
    // float4 posWS : SV_TARGET3;
};

PSOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) : SV_TARGET {

    int2 ipos = int2(vsOut.posH.xy);
    float2 prevCrd = vsOut.prevPosH.xy / vsOut.prevPosH.w;

#ifdef FALCOR_FLIP_Y
    prevCrd *= float2(0.5, 0.5);
#else
    prevCrd *= float2(0.5, -0.5);
#endif
    prevCrd += 0.5;

    int2 prevIpos = int2(prevCrd * (float2)gFrameDim);


    // Not a valid second layer gbuffer
    // float linearZ = (vsOut.posH.z * vsOut.posH.w - gEps);
    // if(linearZ <= gFirstLinearZBuffer[ipos][0] - 0.00001)
    //     discard;

    PSOut psOut = {};

    // float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    // VertexData v = prepareVertexData(vsOut, faceNormal);
    // let lod = ImplicitLodTextureSampler();

    // const float3 viewDir = normalize(gScene.camera.getPosition() - v.posW);
    // ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

    // // Create BSDF instance.
    // let bsdf = gScene.materials.getBSDF(sd, lod);

    // const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    psOut.gDebug = gPrevPosWSBuffer[prevIpos];
    // psOut.gDebug = float4(vsOut.prevPosH.xy, 0, 0);
    psOut.gMotionVector = float4(prevIpos, 0, 0);
    psOut.gPosWS = gPrevPosWSBuffer[prevIpos];

    return psOut;
}
