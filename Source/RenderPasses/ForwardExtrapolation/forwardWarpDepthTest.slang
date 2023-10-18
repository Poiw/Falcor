cbuffer PerFrameCB {
    uint2 gFrameDim;
};


Texture2D<float> gDepthTex;
Texture2D<float2> gForwardMotionTex;

RWTexture2D<uint> gTempDepthTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float2 curCrd = pixelCoord / gFrameDim;
    float2 nextCrd = curCrd + gForwardMotionTex[pixelCoord];

    uint2 nextCoord = uint2(nextCrd * gFrameDim);

    uint depth = uint(gDepthTex[pixelCoord] * 262144.0f);

    InterlockedMin(gTempDepthTex[nextCoord], depth);

    return;
}
