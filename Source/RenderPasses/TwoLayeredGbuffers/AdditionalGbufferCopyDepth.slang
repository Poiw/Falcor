import Scene.Raster;
import GBufferHelpers;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 gCenterViewProjMat;
};

Texture2D<float4> gPosWS;
Texture2D<float2> gProjFirstLinearZ;

RWTexture2D<uint> gProjDepthBuf;

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

    if (z * tarPosH.w > gProjFirstLinearZ[tarIpos][0])
        InterlockedMax(gProjDepthBuf[tarIpos], asuint(z), gProjDepthBuf[tarIpos]);

    return;
}
