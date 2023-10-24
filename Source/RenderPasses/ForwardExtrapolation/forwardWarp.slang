cbuffer PerFrameCB {
    uint2 gFrameDim;
};

Texture2D<float4> gRenderTex;
Texture2D<uint> gDepthTex;
Texture2D<uint> gTempDepthTex;
Texture2D<float2> gForwardMotionTex;

RWTexture2D<float4> gTempWarpTex;
RWTexture2D<float2> gTempMotionVectorTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float2 curCrd = (pixelCoord + float2(0.5, 0.5)) / gFrameDim;
    float2 nextCrd = curCrd + gForwardMotionTex[pixelCoord];

    uint2 nextCoord = uint2(nextCrd * gFrameDim);

    uint depth = gDepthTex[pixelCoord];

    if (depth == gTempDepthTex[nextCoord]) {
        gTempWarpTex[nextCoord] = gRenderTex[pixelCoord];
        gTempMotionVectorTex[nextCoord] = curCrd - nextCrd;
    }

    return;
}
