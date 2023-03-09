import Scene.Raster;
import GBufferHelpers;

Texture2D<float4> gLinearZ;
Texture2D<float4> gFirstLayerNormWS;
Texture2D<float4> gFirstLayerDiffOpacity;
Texture2D<float4> gSecondaLayerNormWS;
Texture2D<float4> gSecondLayerDiffOpacity;
// RasterizerOrderedTexture2D<float> gPosWSBufferTemp;

cbuffer PerFrameCB
{
    uint2 gFrameDim;
    float4x4 gCenterViewProjMatNoJitter;
    // float gCurEps;
};

// Default Vertex Shader
VSOut vsMain(VSIn vIn) {

    VSOut vOut = defaultVS(vIn);

    float3 posW = vOut.posW; // Assume no dynamic object, no world matrix changes

    vOut.prevPosH = mul(gCenterViewProjMatNoJitter, float4(posW, 1.f));

    return vOut;


}


#define is_valid(name) (is_valid_##name != 0)

struct PSOut {
    float4 gDebug : SV_TARGET0;
    float4 gMask : SV_TARGET1;
    float4 gNormWS : SV_TARGET2;
    float4 gDiffOpacity : SV_TARGET3;
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
    float projLinearZ = vsOut.prevPosH.w;

    PSOut psOut = {};

    if (projLinearZ < gLinearZ[prevIpos][0]) {
        psOut.gNormWS = gFirstLayerNormWS[prevIpos];
        psOut.gDiffOpacity = gFirstLayerDiffOpacity[prevIpos];
        psOut.gMask = float4(1, 0, 0, 0);
    }
    else {
        psOut.gNormWS = gSecondaLayerNormWS[prevIpos];
        psOut.gDiffOpacity = gSecondLayerDiffOpacity[prevIpos];
        psOut.gMask = float4(0, 0, 1, 0);
    }

    return psOut;
}
