cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
    uint mDepthScale;
    float mBackgroundScreenScaleInv;
};

Texture2D<float4> gBackgroundPosWTex;
Texture2D<float4> gCurPosWTex;
Texture2D<float4> gBackgroundColorTex;
Texture2D<float4> gCurColorTex;
Texture2D<uint> gTestedDepthTex;
Texture2D<float4> gNextPosWTex; // for dynamic test

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

    backgroundCrd *= mBackgroundScreenScaleInv;
    curCrd *= mBackgroundScreenScaleInv;

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


    // uint2 backgroundCoord_tmp = uint2(backgroundCrd * gFrameDim);
    // for (int i = -1; i <= 1; ++i)
    //     for (int j = -1; j <= 1; ++j) {
    //         uint2 backgroundCoord = backgroundCoord_tmp + uint2(i, j);
    //         if (backgroundCoord.x >= 0 && backgroundCoord.x < gFrameDim.x && backgroundCoord.y >= 0 && backgroundCoord.y < gFrameDim.y &&
    //             gTestedDepthTex[backgroundCoord] == backgroundDepth) {
    //             gTempOutputTex[backgroundCoord] = gBackgroundColorTex[pixelCoord];
    //             gTempOutputPosWTex[backgroundCoord] = backgroundPosW;
    //         }
    //     }

    uint2 backgroundCoord = uint2(backgroundCrd * gFrameDim);
    if (backgroundCoord.x >= 0 && backgroundCoord.x < gFrameDim.x && backgroundCoord.y >= 0 && backgroundCoord.y < gFrameDim.y &&
        gTestedDepthTex[backgroundCoord] == backgroundDepth) {
        gTempOutputTex[backgroundCoord] = gBackgroundColorTex[pixelCoord];
        gTempOutputPosWTex[backgroundCoord] = backgroundPosW;
    }


    if (gNextPosWTex[pixelCoord].w < 0.5) { // dynamic objects
        return;
    }

    uint2 curCoord = uint2(curCrd * gFrameDim);
    if (curCoord.x >= 0 && curCoord.x < gFrameDim.x && curCoord.y >= 0 && curCoord.y < gFrameDim.y &&
        gTestedDepthTex[curCoord] == curDepth) {
        gTempOutputTex[curCoord] = gCurColorTex[pixelCoord];
        gTempOutputPosWTex[curCoord] = curPosW;
    }


    return;
}
