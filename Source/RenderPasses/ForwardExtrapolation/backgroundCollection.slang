cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
    uint mDepthScale;
};

Texture2D<float4> gBackgroundPosWTex;
Texture2D<float4> gCurPosWTex;
Texture2D<float4> gBackgroundColorTex;
Texture2D<float4> gCurColorTex;
Texture2D<uint> gTestedDepthTex;

RWTexture2D<float4> gTempOutputTex;
RWTexture2D<float4> gTempOutputPosWTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float4 backgroundPosW = float4(gBackgroundPosWTex[pixelCoord].xyz, 1);
    float4 curPosW = float4(gCurPosWTex[pixelCoord].xyz, 1);

    float4 backgroundPosH = mul(curViewProjMat, backgroundPosW);
    float4 curPosH = mul(curViewProjMat, curPosW);

    float2 backgroundCrd = backgroundPosH.xy / backgroundPosH.w;
    float2 curCrd = curPosH.xy / curPosH.w;

    uint backgroundDepth = uint(backgroundPosH.w * float(mDepthScale));
    uint curDepth = uint(curPosH.w * float(mDepthScale));

    backgroundDepth = backgroundDepth * 2;
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

    if (gTestedDepthTex[backgroundCoord] == backgroundDepth) {
        gTempOutputTex[backgroundCoord] = gBackgroundColorTex[pixelCoord];
        gTempOutputPosWTex[backgroundCoord] = backgroundPosW;
    }

    if (gTestedDepthTex[curCoord] == curDepth) {
        gTempOutputTex[curCoord] = gCurColorTex[pixelCoord];
        gTempOutputPosWTex[curCoord] = curPosW;
    }


    return;
}
