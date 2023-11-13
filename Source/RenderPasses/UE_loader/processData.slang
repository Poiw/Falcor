cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 curViewProjMat;
    float4x4 curViewProjMatInv;
    float4x4 curViewMatInv;
    float tan2FovY;
    float tan2FovX;
    float inputLinearZScale;
    bool rescaleScene;
    float sceneMin;
    float sceneMax;
};

Texture2D<float4> gCurPosWTex;
Texture2D<float4> gCurColorTex;
Texture2D<float4> gCurMotionVectorTex;
Texture2D<float4> gCurLinearZTex;
Texture2D<float4> gPrevPosWTex;

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

    // Color
    gColorTex[flipX_Coord] = gCurColorTex[pixelCoord];

    // Motion Vector
    float2 mv = gCurMotionVectorTex[pixelCoord].xy;

    mv = mv / gFrameDim;

    gMotionVectorTex[flipX_Coord] = mv;

    // LinearZ
    float zScale = inputLinearZScale;

    if (rescaleScene) {
        zScale = zScale / (sceneMax - sceneMin) * 2;
    }

    float linearZ = gCurLinearZTex[pixelCoord].x;
    linearZ *= zScale;
    gLinearZTex[flipX_Coord] = float2(linearZ, 1);

    // Position

    float2 ndc_xy;
    ndc_xy.x = (pixelCoord.x + 0.5) / gFrameDim.x;
    ndc_xy.y = (pixelCoord.y + 0.5) / gFrameDim.y;

    ndc_xy -= float2(0.5, 0.5);

#ifdef FALCOR_FLIP_Y
    ndc_xy /= float2(0.5, 0.5);
#else
    ndc_xy /= float2(0.5, -0.5);
#endif

    float4 curPosH;
    curPosH.x = ndc_xy.x * tan2FovX * linearZ;
    curPosH.y = ndc_xy.y * tan2FovY * linearZ;

    curPosH.z = linearZ;
    curPosH.w = 1;

    float4 curPosW = mul(curViewMatInv, curPosH);

    gPosWTex[flipX_Coord] = curPosW;

    float2 prevCrd = (flipX_Coord + float2(0.5, 0.5)) / gFrameDim + mv;

    prevCrd = clamp(prevCrd, float2(0, 0), float2(1, 1));

    uint2 prevCoord = uint2(prevCrd * gFrameDim);

    float3 prevPosW = gPrevPosWTex[prevCoord].xyz;

    float3 nextPosW = curPosW.xyz * 2 - prevPosW;

    gNextPosWTex[flipX_Coord] = float4(nextPosW, 1);
    // gNextPosWTex[flipX_Coord] = float4(tmpCurPosW.xyz, 1);

    return;
}



















// ----------------------- Old Code ----------------------
// void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

//     // First Layer
//     uint2 pixelCoord = dispatchThreadId.xy;

//     uint2 flipX_Coord = uint2(gFrameDim.x - 1 - pixelCoord.x, pixelCoord.y);

//     // Color
//     gColorTex[flipX_Coord] = gCurColorTex[pixelCoord];

//     // gNextPosWTex[flipX_Coord] = float4(gCurPosWTex[pixelCoord].xyz, 1);
//     // Motion Vector
//     float2 mv = gCurMotionVectorTex[pixelCoord].xy;

//     mv = mv / gFrameDim;

//     gMotionVectorTex[flipX_Coord] = mv;

//     // Position
//     float3 tmpCurPosW = gCurPosWTex[pixelCoord].xyz;

//     if (rescaleScene) {
//         tmpCurPosW = (tmpCurPosW - sceneMin) / (sceneMax - sceneMin) * 2 - 1;
//     }

//     float4 curPosH = mul(curViewProjMat, float4(tmpCurPosW, 1));

//     float2 curCrd = curPosH.xy / curPosH.w;

//     // Linear Z
//     float linearZ = curPosH.w;
//     gLinearZTex[flipX_Coord] = float2(linearZ, 1);

//     // Position

//     float2 gtXY = flipX_Coord + float2(0.5, 0.5);
//     gtXY = gtXY / gFrameDim;
//     gtXY -= float2(0.5, 0.5);

// #ifdef FALCOR_FLIP_Y
//     gtXY /= float2(0.5, 0.5);
// #else
//     gtXY /= float2(0.5, -0.5);
// #endif

//     curPosH.xy = gtXY * curPosH.w;
//     float4 fixedCurPosW = mul(curViewProjMatInv, curPosH);
//     fixedCurPosW.xyz /= fixedCurPosW.w;
//     gPosWTex[flipX_Coord] = fixedCurPosW;
//     // gPosWTex[flipX_Coord] = float4(curPosH.xyz/curPosH.w, 1);


// // #ifdef FALCOR_FLIP_Y
// //     curCrd *= float2(0.5, 0.5);
// // #else
// //     curCrd *= float2(0.5, -0.5);
// // #endif

//     // curCrd += float2(0.5, 0.5);

//     // float2 curCoord = curCrd * gFrameDim;

//     // gNextPosWTex[flipX_Coord] = float4(curCoord, linearZ, 1);

// //     curPosH = mul(curViewProjMat, float4(fixedCurPosW.xyz, 1));
// //     curCrd = curPosH.xy / curPosH.w;

// // #ifdef FALCOR_FLIP_Y
// //     curCrd *= float2(0.5, 0.5);
// // #else
// //     curCrd *= float2(0.5, -0.5);
// // #endif

// //     curCrd += float2(0.5, 0.5);
// //     curCrd = clamp(curCrd, float2(0, 0), float2(1, 1));
// //     curCrd *= gFrameDim;
// //     gNextPosWTex[flipX_Coord] = float4(curCrd, linearZ, 1);
//     // gNextPosWTex[flipX_Coord] = float4(gCurPosWTex[pixelCoord].xyz, 1);
//     // gNextPosWTex[flipX_Coord] = float4(gCurPosWTex[pixelCoord].xyz, 1);



//     float2 prevCrd = (flipX_Coord + float2(0.5, 0.5)) / gFrameDim + mv;

//     prevCrd = clamp(prevCrd, float2(0, 0), float2(1, 1));

//     uint2 prevCoord = uint2(prevCrd * gFrameDim);

//     float3 prevPosW = gPrevPosWTex[prevCoord].xyz;
//     // float3 curPosW = gCurPosWTex[pixelCoord].xyz;
//     float3 curPosW = fixedCurPosW.xyz;

//     float3 nextPosW = curPosW.xyz * 2 - prevPosW;

//     gNextPosWTex[flipX_Coord] = float4(nextPosW, 1);
//     // gNextPosWTex[flipX_Coord] = float4(tmpCurPosW.xyz, 1);

//     return;
// }
