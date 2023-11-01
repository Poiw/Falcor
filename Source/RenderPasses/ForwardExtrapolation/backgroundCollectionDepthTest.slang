cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
    uint mDepthScale;
};

Texture2D<float4> gBackgroundPosWTex;
Texture2D<float4> gCurPosWTex;

RWTexture2D<uint> gTempDepthTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float4 backgroundPosW = gBackgroundPosWTex[pixelCoord];
    float4 curPosW = gCurPosWTex[pixelCoord];

    float4 backgroundPosH = mul(curViewProjMat, backgroundPosW);
    float4 curPosH = mul(curViewProjMat, curPosW);

    float2 backgroundCrd = backgroundPosH.xy / backgroundPosH.w;
    float2 curCrd = curPosH.xy / curPosH.w;

    uint backgroundDepth = uint(backgroundPosH.w * float(mDepthScale));
    uint curDepth = uint(curPosH.w * float(mDepthScale));

    backgroundCrd = backgroundCrd * 2;
    curDepth = curDepth * 2 + 1;



#ifdef FALCOR_FLIP_Y
    backgroundCrd *= float2(0.5, 0.5);
    curCrd *= float2(0.5, 0.5);
#else
    backgroundCrd *= float2(0.5, -0.5);
    curCrd *= float2(0.5, -0.5);
#endif

    backgroundCrd += float2(0.5, 0.5);
    curCrd += float2(0.5, 0.5);

    uint2 backgroundCoord = uint2(backgroundCrd * gFrameDim);
    uint2 curCoord = uint2(curCrd * gFrameDim);

    InterlockedMax(gTempDepthTex[backgroundCoord], backgroundDepth);
    InterlockedMax(gTempDepthTex[curCoord], curDepth);

    return;
}
