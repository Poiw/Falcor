cbuffer PerFrameCB {
    uint2 gFrameDim;
    bool mUseBGCollection;
};

Texture2D<float4> gRenderTex;
Texture2D<uint> gDepthTex;
Texture2D<float2> gForwardMotionTex;

Texture2D<float4> gBackgroundWarpedTex;
Texture2D<uint> gBackgroundWarpedDepthTex;

RWTexture2D<uint> gTempDepthTex;

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

    if (mUseBGCollection) {

        if (gTempDepthTex[pixelCoord] == uint(-1)) {
            gTempWarpTex[pixelCoord] = gBackgroundWarpedTex[pixelCoord];
            gTempDepthTex[pixelCoord] = gBackgroundWarpedDepthTex[pixelCoord];
            gTempMotionVectorTex[pixelCoord] = float2(0, 0);
        }

    }

    return;
}
