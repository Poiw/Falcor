cbuffer PerFrameCB {
    uint2 gFrameDim;
    uint gKernelSize;
};

Texture2D<float4> gTempWarpTex;

RWTexture2D<float4> targetRenderTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    targetRenderTex[pixelCoord] = gTempWarpTex[pixelCoord];

    return;
}
