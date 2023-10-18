cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
};

Texture2D<float4> gNextPosWTex;

RWTexture2D<uint> gTempDepthTex;
RWTexture2D<float> gDepthTex;
RWTexture2D<float2> gForwardMotionTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float2 curCrd = pixelCoord / gFrameDim;

    float4 nextPosW = gNextPosWTex[pixelCoord];
    float4 nextPosH = mul(curViewProjMat, nextPosW);
    float2 nextCrd = nextPosH.xy / nextPosH.w;
    float nextZ = nextPosH.z / nextPosH.w;

#ifdef FALCOR_FLIP_Y
    nextCrd *= float2(0.5, 0.5);
#else
    nextCrd *= float2(0.5, -0.5);
#endif
    nextCrd += 0.5;

    gForwardMotionTex[pixelCoord] = nextCrd - curCrd;
    gDepthTex[pixelCoord] = nextZ;


    uint2 nextCoord = uint2(nextCrd * gFrameDim);

    uint depth = uint(nextZ * 262144.0f);

    InterlockedMin(gTempDepthTex[nextCoord], depth);

    return;
}
