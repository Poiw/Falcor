cbuffer PerFrameCB {
    uint2 gFrameDim;
    uint mDepthScale;
};

Texture2D<float4> gBackgroundWarpedTex;
Texture2D<uint> gBackgroundWarpedDepthTex;
Texture2D<float4> gWarpedTex;
Texture2D<uint> gWarpedDepthTex;
Texture2D<float2> gMotionVectorTex;

RWTexture2D<float4> targetRenderTex;
RWTexture2D<float2> targetDepthTex;
RWTexture2D<float2> targetMotionVectorTex;
RWTexture2D<float4> gBackgroundMask;



[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    uint2 pixelCoord = dispatchThreadId.xy;

    if (gWarpedDepthTex[pixelCoord] != uint(-1)) {
        targetRenderTex[pixelCoord] = gWarpedTex[pixelCoord];
        targetDepthTex[pixelCoord] = float2((float)gWarpedDepthTex[pixelCoord] / float(mDepthScale), 0.);
        targetMotionVectorTex[pixelCoord] = gMotionVectorTex[pixelCoord];
        gBackgroundMask[pixelCoord] = float4(1., 0., 0., 1.);
        return;
    }
    else {
        targetRenderTex[pixelCoord] = gBackgroundWarpedTex[pixelCoord];
        targetDepthTex[pixelCoord] = float2((float)gBackgroundWarpedDepthTex[pixelCoord] / float(mDepthScale), 0.);
        targetMotionVectorTex[pixelCoord] = float2(0., 0.);
        gBackgroundMask[pixelCoord] = float4(0., 0., 1., 1.);
    }

    return;
}


