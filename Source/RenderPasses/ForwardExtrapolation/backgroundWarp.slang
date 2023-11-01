cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
    uint mDepthScale;
};

Texture2D<float4> gBackgroundPosWTex;
Texture2D<float4> gBackgroundColorTex;
Texture2D<uint> gBackgroundWarpedDepthTex;

RWTexture2D<float4> gBackgroundWarpedTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float4 backgroundPosW = float4(gBackgroundPosWTex[pixelCoord].xyz, 1);

    float4 backgroundPosH = mul(curViewProjMat, backgroundPosW);

    float2 backgroundCrd = backgroundPosH.xy / backgroundPosH.w;

    uint backgroundDepth = uint(backgroundPosH.w * float(mDepthScale));


#ifdef FALCOR_FLIP_Y
    backgroundCrd *= float2(0.5, 0.5);
#else
    backgroundCrd *= float2(0.5, -0.5);
#endif

    backgroundCrd += float2(0.5, 0.5);

    uint2 backgroundCoord = uint2(backgroundCrd * gFrameDim);

    if (gBackgroundWarpedDepthTex[backgroundCoord] == backgroundDepth) {
        gBackgroundWarpedTex[backgroundCoord] = gBackgroundColorTex[pixelCoord];
    }

    return;
}
