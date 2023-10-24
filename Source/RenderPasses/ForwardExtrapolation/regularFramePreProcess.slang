cbuffer PerFrameCB {
    uint2 gFrameDim;
};

Texture2D<float2> gCurMotionVectorTex;
Texture2D<float2> gPrevMotionVectorTex;
Texture2D<float2> gForwardMotionTex;

Texture2D<float2> gCurLinearZTex;

RWTexture2D<float2> gMotionVectorOut;
RWTexture2D<float2> gLinearZOut;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;


    gLinearZOut[pixelCoord] = float2(gCurLinearZTex[pixelCoord].x, 0.0f);

    float2 Crd = (pixelCoord + float2(0.5, 0.5)) / gFrameDim;

    float2 PrevCrd = Crd + gCurMotionVectorTex[pixelCoord];
    int2 prevCoord = int2(PrevCrd * gFrameDim);
    prevCoord.x = min(max(prevCoord.x, 0), gFrameDim.x - 1);
    prevCoord.y = min(max(prevCoord.y, 0), gFrameDim.y - 1);

    float2 PrevPrevCrd = PrevCrd + gPrevMotionVectorTex[prevCoord];
    int2 prevPrevCoord = int2(PrevPrevCrd * gFrameDim);
    prevPrevCoord.x = min(max(prevPrevCoord.x, 0), gFrameDim.x - 1);
    prevPrevCoord.y = min(max(prevPrevCoord.y, 0), gFrameDim.y - 1);

    float2 EstPrevCrd = PrevPrevCrd + gForwardMotionTex[prevPrevCoord];

    gMotionVectorOut[pixelCoord] = EstPrevCrd - Crd;

    return;
}
