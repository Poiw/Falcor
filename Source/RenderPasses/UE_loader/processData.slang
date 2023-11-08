cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
};

Texture2D<float4> gCurPosWTex;
Texture2D<float4> gCurColorTex;
Texture2D<float4> gCurMotionVectorTex;

RWTexture2D<float4> gNextPosWTex;
RWTexture2D<float4> gPosWTex;
RWTexture2D<float4> gColorTex;
RWTexture2D<float2> gMotionVectorTex;
RWTexture2D<float2> gLinearZTex;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    uint2 flipX_Coord = uint2(gFrameDim.x - 1 - pixelCoord.x, pixelCoord.y);

    gPosWTex[flipX_Coord] = gCurPosWTex[pixelCoord];
    gColorTex[flipX_Coord] = gCurColorTex[pixelCoord];


    float2 mv = gCurMotionVectorTex[pixelCoord].xy;
    mv.x = -mv.x;
    mv.y = -mv.y;

    mv = mv / gFrameDim;

    gMotionVectorTex[flipX_Coord] = mv;





    float4 curPosW = float4(gCurPosWTex[pixelCoord].xyz, 1);
    // float4 curPosW = float4(-gCurPosWTex[pixelCoord].x, gCurPosWTex[pixelCoord].y, -gCurPosWTex[pixelCoord].z, 1);
    float4 curPosH = mul(curViewProjMat, curPosW);

    float2 curCrd = curPosH.xy / curPosH.w;
    float linearZ = curPosH.w;


#ifdef FALCOR_FLIP_Y
    curCrd *= float2(0.5, 0.5);
#else
    curCrd *= float2(0.5, -0.5);
#endif

    curCrd += float2(0.5, 0.5);

    float2 curCoord = curCrd * gFrameDim;

    gNextPosWTex[flipX_Coord] = float4(curCoord, linearZ, 1);

    gLinearZTex[flipX_Coord] = float2(linearZ, 1);

    return;
}
