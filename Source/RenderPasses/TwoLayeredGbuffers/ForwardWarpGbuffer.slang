import Scene.Raster;
import GBufferHelpers;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 gCenterViewProjMatNoJitter;
    float4x4 gCurViewProjMat;
};


Texture2D<float4> gFirstLayerNormWS;
Texture2D<float4> gFirstLayerPosWS;
Texture2D<float4> gFirstLayerDiffOpacity;
Texture2D<float4> gSecondLayerNormWS;
Texture2D<float4> gSecondLayerPosWS;
Texture2D<float4> gSecondLayerDiffOpacity;

RWTexture2D<float> gDepthTest;
RWTexture2D<float4> gNormWS;
RWTexture2D<float4> gDiffOpacity;
RWTexture2D<float4> gMask;



[numthreads(16, 16, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float3 posWS = gFirstLayerPosWS[pixelCoord].xyz;

    float4 tarPosH = mul(gCurViewProjMat, float4(posWS, 1.0f));
    float2 tarCrd = tarPosH.xy / tarPosH.w;
    float z = tarPosH.z / tarPosH.w;

#ifdef FALCOR_FLIP_Y
    tarCrd *= float2(0.5, 0.5);
#else
    tarCrd *= float2(0.5, -0.5);
#endif
    tarCrd += 0.5;

    uint2 tarIpos = uint2(tarCrd * gFrameDim);

    if (z < gDepthTest[tarIpos]) {
        gDepthTest[tarIpos] = z;
        gNormWS[tarIpos] = gFirstLayerNormWS[pixelCoord];
        gDiffOpacity[tarIpos] = gFirstLayerDiffOpacity[pixelCoord];
        gMask[tarIpos] = float4(1.0f, 0.0f, 0.0f, 1.0f);
    }


    // Second Layer
    posWS = gSecondLayerPosWS[pixelCoord].xyz;

    tarPosH = mul(gCurViewProjMat, float4(posWS, 1.0f));
    tarCrd = tarPosH.xy / tarPosH.w;
    z = tarPosH.z / tarPosH.w;

#ifdef FALCOR_FLIP_Y
    tarCrd *= float2(0.5, 0.5);
#else
    tarCrd *= float2(0.5, -0.5);
#endif
    tarCrd += 0.5;

    tarIpos = uint2(tarCrd * gFrameDim);

    if (z < gDepthTest[tarIpos]) {
        gDepthTest[tarIpos] = z;
        gNormWS[tarIpos] = gSecondLayerNormWS[pixelCoord];
        gDiffOpacity[tarIpos] = gSecondLayerDiffOpacity[pixelCoord];
        gMask[tarIpos] = float4(0.0f, 0.0f, 1.0f, 1.0f);
    }


    return;
}
