cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4 gCurRec;
    float4 gPrevRec;
};

Texture2D<float4> gCurColorTex;
Texture2D<float2> gCurMotionVectorTex;
Texture2D<float2> gPrevMotionVectorTex;
Texture2D<float2> gForwardMotionTex;

Texture2D<float2> gCurLinearZTex;

RWTexture2D<float2> gMotionVectorOut;
RWTexture2D<float2> gLinearZOut;
RWTexture2D<float4> gColorOut;



[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 sourcePixelCoord = ((float2)dispatchThreadId.xy / (float2)gFrameDim * (gCurRec.zw - gCurRec.xy) + gCurRec.xy) * (float2)gFrameDim;

    uint2 pixelCoord = dispatchThreadId.xy;


    gLinearZOut[pixelCoord] = float2(gCurLinearZTex[sourcePixelCoord].x, 0.0f);
    gColorOut[pixelCoord] = gCurColorTex[sourcePixelCoord];


    // Todo: need to fix motion vector
    float2 Crd = (pixelCoord + float2(0.5, 0.5)) / gFrameDim;
    float2 SourceCrd = (sourcePixelCoord + float2(0.5, 0.5)) / gFrameDim;

    float2 PrevCrd = SourceCrd + gCurMotionVectorTex[sourcePixelCoord];
    PrevCrd = PrevCrd * (gPrevRec.zw - gPrevRec.xy) + gPrevRec.xy;
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
