Texture2D<float4> gMotionVector;
Texture2D<float4> gColor;
Texture2D<float4> gDepth;

RWTexture2D<float2> gMotionVectorOut;
RWTexture2D<float4> gColorOut;
RWTexture2D<float2> gDepthOut;

cbuffer PerFrameCB {
    uint2 gFrameDim;
};


[numthreads(1, 1, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    uint2 posI = dispatchThreadId.xy;

    gMotionVectorOut[posI] = gMotionVector[posI].xy;
    gColorOut[posI] = gColor[posI];
    gDepthOut[posI] = float2(gDepth[posI].x, 1.0 / max(1e-3, gDepth[posI].y));

    return;
}
