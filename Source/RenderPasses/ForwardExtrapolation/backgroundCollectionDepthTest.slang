cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
    uint mDepthScale;
    float mBackgroundScreenScaleInv;
    int passNum;
};

Texture2D<float4> gBackgroundPosWTex;
Texture2D<float4> gCurPosWTex;
Texture2D<float4> gNextPosWTex; // for dynamic test

RWTexture2D<uint> gTempDepthTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    if (passNum == 1) {

        if (gNextPosWTex[pixelCoord].w < 0.9) { // dynamic objects
            return;
        }
        float4 curPosW = float4(gCurPosWTex[pixelCoord].xyz, 1);
        float4 curPosH = mul(curViewProjMat, curPosW);
        float2 curCrd = curPosH.xy / curPosH.w;
        curCrd *= mBackgroundScreenScaleInv;
        uint curDepth = uint(curPosH.w * float(mDepthScale));
        curDepth = curDepth * 2 + 1;

    #ifdef FALCOR_FLIP_Y
        curCrd *= float2(0.5, 0.5);
    #else
        curCrd *= float2(0.5, -0.5);
    #endif

        curCrd += float2(0.5, 0.5);


        uint2 curCoord = uint2(curCrd * gFrameDim);
        if (curCoord.x >= 0 && curCoord.x < gFrameDim.x && curCoord.y >= 0 && curCoord.y < gFrameDim.y) {
            InterlockedMin(gTempDepthTex[curCoord], curDepth);
        }
    }

    else {
        float4 backgroundPosW = float4(gBackgroundPosWTex[pixelCoord].xyz, 1);
        float4 backgroundPosH = mul(curViewProjMat, backgroundPosW);
        float2 backgroundCrd = backgroundPosH.xy / backgroundPosH.w;
        backgroundCrd *= mBackgroundScreenScaleInv;
        uint backgroundDepth = uint(backgroundPosH.w * float(mDepthScale));
        backgroundDepth = backgroundDepth * 2;

    #ifdef FALCOR_FLIP_Y
        backgroundCrd *= float2(0.5, 0.5);
    #else
        backgroundCrd *= float2(0.5, -0.5);
    #endif

        backgroundCrd += float2(0.5, 0.5);


        // uint2 backgroundCoord_tmp = uint2(backgroundCrd * gFrameDim);
        // for (int i = -1; i <= 1; ++i)
        //     for (int j = -1; j <= 1; ++j) {
        //         uint2 backgroundCoord = backgroundCoord_tmp + uint2(i, j);
        //         if (backgroundCoord.x >= 0 && backgroundCoord.x < gFrameDim.x && backgroundCoord.y >= 0 && backgroundCoord.y < gFrameDim.y) {
        //             InterlockedMax(gTempDepthTex[backgroundCoord], backgroundDepth);
        //         }
        //     }

        uint2 backgroundCoord = uint2(backgroundCrd * gFrameDim);

        if (gTempDepthTex[backgroundCoord] < uint(-1)) {
            return;
        }

        if (backgroundCoord.x >= 0 && backgroundCoord.x < gFrameDim.x && backgroundCoord.y >= 0 && backgroundCoord.y < gFrameDim.y) {
            InterlockedMin(gTempDepthTex[backgroundCoord], backgroundDepth);
        }
    }

    return;
}
