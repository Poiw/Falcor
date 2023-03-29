cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 gCenterViewProjMat;
};

Texture2D<float4> gPosWS;
Texture2D<float4> gNormWS;
Texture2D<float4> gDiffOpacity;
Texture2D<uint> gInstanceID;
Texture2D<float4> gPosL;

Texture2D<uint> gProjDepthBuf;

RWTexture2D<float4> gProjPosWS;
RWTexture2D<float4> gProjNormWS;
RWTexture2D<float4> gProjDiffOpacity;
RWTexture2D<uint> gProjInstanceID;
RWTexture2D<float4> gProjPosL;

[numthreads(1, 1, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float3 posWS = gPosWS[pixelCoord].xyz;

    float4 tarPosH = mul(gCenterViewProjMat, float4(posWS, 1.0f));
    float2 tarCrd = tarPosH.xy / tarPosH.w;
    float z = tarPosH.z / tarPosH.w;

#ifdef FALCOR_FLIP_Y
    tarCrd *= float2(0.5, 0.5);
#else
    tarCrd *= float2(0.5, -0.5);
#endif
    tarCrd += 0.5;

    uint2 tarIpos = uint2(tarCrd * gFrameDim);

    if (asuint(z) == gProjDepthBuf[tarIpos]) {
        gProjNormWS[tarIpos] = gNormWS[pixelCoord];
        gProjDiffOpacity[tarIpos] = gDiffOpacity[pixelCoord];
        gProjPosWS[tarIpos] = float4(posWS, 1.0f);
        gProjInstanceID[tarIpos] = gInstanceID[pixelCoord];
        gProjPosL[tarIpos] = gPosL[pixelCoord];
    }

    return;
}
