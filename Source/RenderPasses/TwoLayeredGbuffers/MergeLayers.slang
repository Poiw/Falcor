Texture2D<uint> gFirstLayerDepthTest0;
Texture2D<float4> gFirstLayerNormWS0;
Texture2D<float4> gFirstLayerDiffOpacity0;
Texture2D<float4> gFirstLayerPosWS0;
Texture2D<uint2>  gFirstLayerPrevCoord0;

Texture2D<uint> gFirstLayerDepthTest1;
Texture2D<float4> gFirstLayerNormWS1;
Texture2D<float4> gFirstLayerDiffOpacity1;
Texture2D<float4> gFirstLayerPosWS1;
Texture2D<uint2>  gFirstLayerPrevCoord1;

Texture2D<uint> gFirstLayerDepthTest2;
Texture2D<float4> gFirstLayerNormWS2;
Texture2D<float4> gFirstLayerDiffOpacity2;
Texture2D<float4> gFirstLayerPosWS2;
Texture2D<uint2>  gFirstLayerPrevCoord2;

Texture2D<uint> gFirstLayerDepthTest3;
Texture2D<float4> gFirstLayerNormWS3;
Texture2D<float4> gFirstLayerDiffOpacity3;
Texture2D<float4> gFirstLayerPosWS3;
Texture2D<uint2>  gFirstLayerPrevCoord3;

Texture2D<uint> gSecondLayerDepthTest0;
Texture2D<float4> gSecondLayerNormWS0;
Texture2D<float4> gSecondLayerDiffOpacity0;
Texture2D<float4> gSecondLayerPosWS0;
Texture2D<uint2>  gSecondLayerPrevCoord0;

Texture2D<uint> gSecondLayerDepthTest1;
Texture2D<float4> gSecondLayerNormWS1;
Texture2D<float4> gSecondLayerDiffOpacity1;
Texture2D<float4> gSecondLayerPosWS1;
Texture2D<uint2>  gSecondLayerPrevCoord1;

Texture2D<uint> gSecondLayerDepthTest2;
Texture2D<float4> gSecondLayerNormWS2;
Texture2D<float4> gSecondLayerDiffOpacity2;
Texture2D<float4> gSecondLayerPosWS2;
Texture2D<uint2>  gSecondLayerPrevCoord2;

Texture2D<uint> gSecondLayerDepthTest3;
Texture2D<float4> gSecondLayerNormWS3;
Texture2D<float4> gSecondLayerDiffOpacity3;
Texture2D<float4> gSecondLayerPosWS3;
Texture2D<uint2>  gSecondLayerPrevCoord3;



RWTexture2D<float4> gNormWS;
RWTexture2D<float4> gDiffOpacity;
RWTexture2D<float4> gPosWS;
RWTexture2D<uint2>  gPrevCoord;
RWTexture2D<float4> gMask;

cbuffer PerFrameCB {
    // uint2 gFrameDim;
    int gNearestFilter;
    uint gUsedMipLevel;
};

struct Gbuf {
    float4 normWS;
    float4 diffOpacity;
    float4 posWS;
    uint2 prevCoord;
    uint z;
};

float Dist(int x, int y)
{
    return sqrt(x * x + y * y);
}

[numthreads(16, 16, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    Gbuf layer1, layer2;

    layer1.diffOpacity = float4(0.0, 0.0, 0.0, 0.0);
    layer1.normWS = float4(0.0, 0.0, 0.0, 0.0);
    layer1.z = uint(-1);

    layer2.diffOpacity = float4(0.0, 0.0, 0.0, 0.0);
    layer2.normWS = float4(0.0, 0.0, 0.0, 0.0);
    layer2.z = uint(-1);

    uint eps = 128;

    // if (gFirstLayerDepthTest[pixelCoord] == uint(-1)) {

    //     float minDist = 1000000.0;

    //     for (int i = -gNearestFilter; i <= gNearestFilter; i++) {
    //         for (int j = -gNearestFilter; j <= gNearestFilter; j++) {
    //             uint2 tarCrd = pixelCoord + uint2(i, j);
    //             if (tarCrd.x >= 0 && tarCrd.y >= 0 && tarCrd.x < gFrameDim.x && tarCrd.y < gFrameDim.y &&
    //                     gFirstLayerDepthTest[tarCrd] != uint(-1) &&
    //                     Dist(i, j) < minDist) {

    //                 minDist = Dist(i, j);
    //                 layer1.normWS = gFirstLayerNormWS[tarCrd];
    //                 layer1.diffOpacity = gFirstLayerDiffOpacity[tarCrd];
    //                 layer1.z = gFirstLayerDepthTest[tarCrd];
    //             }
    //         }
    //     }

    // }
    // else {
    //     layer1.normWS = gFirstLayerNormWS[pixelCoord];
    //     layer1.diffOpacity = gFirstLayerDiffOpacity[pixelCoord];
    //     layer1.z = gFirstLayerDepthTest[pixelCoord];
    // }

    {

        if (gFirstLayerDepthTest0[pixelCoord] < layer1.z) {
            layer1.normWS = gFirstLayerNormWS0[pixelCoord];
            layer1.diffOpacity = gFirstLayerDiffOpacity0[pixelCoord];
            layer1.posWS = gFirstLayerPosWS0[pixelCoord];
            layer1.prevCoord = gFirstLayerPrevCoord0[pixelCoord];
            layer1.z = gFirstLayerDepthTest0[pixelCoord];
        }
        if (gUsedMipLevel >= 1 && gFirstLayerDepthTest1[pixelCoord / 2] + eps < layer1.z) {
            layer1.normWS = gFirstLayerNormWS1[pixelCoord / 2];
            layer1.diffOpacity = gFirstLayerDiffOpacity1[pixelCoord / 2];
            layer1.posWS = gFirstLayerPosWS1[pixelCoord / 2];
            layer1.prevCoord = gFirstLayerPrevCoord1[pixelCoord / 2];
            layer1.z = gFirstLayerDepthTest1[pixelCoord / 2];
        }
        if (gUsedMipLevel >= 2 && gFirstLayerDepthTest2[pixelCoord / 4] + eps * 2 < layer1.z) {
            layer1.normWS = gFirstLayerNormWS2[pixelCoord / 4];
            layer1.diffOpacity = gFirstLayerDiffOpacity2[pixelCoord / 4];
            layer1.posWS = gFirstLayerPosWS2[pixelCoord / 4];
            layer1.prevCoord = gFirstLayerPrevCoord2[pixelCoord / 4];
            layer1.z = gFirstLayerDepthTest2[pixelCoord / 4];
        }
        if (gUsedMipLevel >= 3 && gFirstLayerDepthTest3[pixelCoord / 8] + eps * 4 < layer1.z) {
            layer1.normWS = gFirstLayerNormWS3[pixelCoord / 8];
            layer1.diffOpacity = gFirstLayerDiffOpacity3[pixelCoord / 8];
            layer1.posWS = gFirstLayerPosWS3[pixelCoord / 8];
            layer1.prevCoord = gFirstLayerPrevCoord3[pixelCoord / 8];
            layer1.z = gFirstLayerDepthTest3[pixelCoord / 8];
        }



        // if (gFirstLayerDepthTest0[pixelCoord] != uint(-1)) {
        //     layer1.normWS = gFirstLayerNormWS0[pixelCoord];
        //     layer1.diffOpacity = gFirstLayerDiffOpacity0[pixelCoord];
        //     layer1.z = gFirstLayerDepthTest0[pixelCoord];
        // }
        // else if (gFirstLayerDepthTest1[pixelCoord / 4] != uint(-1)) {
        //     layer1.normWS = gFirstLayerNormWS1[pixelCoord / 4];
        //     layer1.diffOpacity = gFirstLayerDiffOpacity1[pixelCoord / 4];
        //     layer1.z = gFirstLayerDepthTest1[pixelCoord / 4];
        // }
        // else if (gFirstLayerDepthTest2[pixelCoord / 16] != uint(-1)) {
        //     layer1.normWS = gFirstLayerNormWS2[pixelCoord / 16];
        //     layer1.diffOpacity = gFirstLayerDiffOpacity2[pixelCoord / 16];
        //     layer1.z = gFirstLayerDepthTest2[pixelCoord / 16];
        // }
        // else if (gFirstLayerDepthTest3[pixelCoord / 64] != uint(-1)) {
        //     layer1.normWS = gFirstLayerNormWS3[pixelCoord / 64];
        //     layer1.diffOpacity = gFirstLayerDiffOpacity3[pixelCoord / 64];
        //     layer1.z = gFirstLayerDepthTest3[pixelCoord / 64];
        // }
    }

    // Second Layer

    // if (gSecondLayerDepthTest[pixelCoord] == uint(-1)) {

    //     float minDist = 1000000.0;

    //     for (int i = -gNearestFilter; i <= gNearestFilter; i++) {
    //         for (int j = -gNearestFilter; j <= gNearestFilter; j++) {
    //             uint2 tarCrd = pixelCoord + uint2(i, j);
    //             if (tarCrd.x >= 0 && tarCrd.y >= 0 && tarCrd.x < gFrameDim.x && tarCrd.y < gFrameDim.y &&
    //                     gSecondLayerDepthTest[tarCrd] != uint(-1) &&
    //                     Dist(i, j) < minDist) {

    //                 minDist = Dist(i, j);
    //                 layer2.normWS = gSecondLayerNormWS[tarCrd];
    //                 layer2.diffOpacity = gSecondLayerDiffOpacity[tarCrd];
    //                 layer2.z = gSecondLayerDepthTest[tarCrd];
    //             }
    //         }
    //     }

    // }
    // else {
    //     layer2.normWS = gSecondLayerNormWS[pixelCoord];
    //     layer2.diffOpacity = gSecondLayerDiffOpacity[pixelCoord];
    //     layer2.z = gSecondLayerDepthTest[pixelCoord];
    // }

    {
        if (gSecondLayerDepthTest0[pixelCoord] < layer2.z) {
            layer2.normWS = gSecondLayerNormWS0[pixelCoord];
            layer2.diffOpacity = gSecondLayerDiffOpacity0[pixelCoord];
            layer2.posWS = gSecondLayerPosWS0[pixelCoord];
            layer2.prevCoord = gSecondLayerPrevCoord0[pixelCoord];
            layer2.z = gSecondLayerDepthTest0[pixelCoord];
        }
        if (gUsedMipLevel >= 1 && gSecondLayerDepthTest1[pixelCoord / 2] + eps < layer2.z) {
            layer2.normWS = gSecondLayerNormWS1[pixelCoord / 2];
            layer2.diffOpacity = gSecondLayerDiffOpacity1[pixelCoord / 2];
            layer2.posWS = gSecondLayerPosWS1[pixelCoord / 2];
            layer2.prevCoord = gSecondLayerPrevCoord1[pixelCoord / 2];
            layer2.z = gSecondLayerDepthTest1[pixelCoord / 2];
        }
        if (gUsedMipLevel >= 2 && gSecondLayerDepthTest2[pixelCoord / 4] + eps * 2 < layer2.z) {
            layer2.normWS = gSecondLayerNormWS2[pixelCoord / 4];
            layer2.diffOpacity = gSecondLayerDiffOpacity2[pixelCoord / 4];
            layer2.posWS = gSecondLayerPosWS2[pixelCoord / 4];
            layer2.prevCoord = gSecondLayerPrevCoord2[pixelCoord / 4];
            layer2.z = gSecondLayerDepthTest2[pixelCoord / 4];
        }
        if (gUsedMipLevel >= 3 && gSecondLayerDepthTest3[pixelCoord / 8] + eps * 4 < layer2.z) {
            layer2.normWS = gSecondLayerNormWS3[pixelCoord / 8];
            layer2.diffOpacity = gSecondLayerDiffOpacity3[pixelCoord / 8];
            layer2.posWS = gSecondLayerPosWS3[pixelCoord / 8];
            layer2.prevCoord = gSecondLayerPrevCoord3[pixelCoord / 8];
            layer2.z = gSecondLayerDepthTest3[pixelCoord / 8];
        }

        // if (gSecondLayerDepthTest0[pixelCoord] != uint(-1)) {
        //     layer2.normWS = gSecondLayerNormWS0[pixelCoord];
        //     layer2.diffOpacity = gSecondLayerDiffOpacity0[pixelCoord];
        //     layer2.z = gSecondLayerDepthTest0[pixelCoord];
        // }
        // else if (gSecondLayerDepthTest1[pixelCoord / 2] != uint(-1)) {
        //     layer2.normWS = gSecondLayerNormWS1[pixelCoord / 2];
        //     layer2.diffOpacity = gSecondLayerDiffOpacity1[pixelCoord / 2];
        //     layer2.z = gSecondLayerDepthTest1[pixelCoord / 2];
        // }
        // else if (gSecondLayerDepthTest2[pixelCoord / 16] != uint(-1)) {
        //     layer2.normWS = gSecondLayerNormWS2[pixelCoord / 16];
        //     layer2.diffOpacity = gSecondLayerDiffOpacity2[pixelCoord / 16];
        //     layer2.z = gSecondLayerDepthTest2[pixelCoord / 16];
        // }
        // else if (gSecondLayerDepthTest3[pixelCoord / 64] != uint(-1)) {
        //     layer2.normWS = gSecondLayerNormWS3[pixelCoord / 64];
        //     layer2.diffOpacity = gSecondLayerDiffOpacity3[pixelCoord / 64];
        //     layer2.z = gSecondLayerDepthTest3[pixelCoord / 64];
        // }
    }

    // gNormWS[pixelCoord] = layer1.normWS;
    // gDiffOpacity[pixelCoord] = layer1.diffOpacity;
    // gMask[pixelCoord] = float4(1.0, 0.0, 0.0, 1.0);
    if (layer1.z < uint(-1)) {
        gNormWS[pixelCoord] = layer1.normWS;
        gDiffOpacity[pixelCoord] = layer1.diffOpacity;
        gPosWS[pixelCoord] = layer1.posWS;
        gPrevCoord[pixelCoord] = layer1.prevCoord;
        gMask[pixelCoord] = float4(1.0, 0.0, 0.0, 1.0);
    }
    else {
        gNormWS[pixelCoord] = layer2.normWS;
        gDiffOpacity[pixelCoord] = layer2.diffOpacity;
        gPosWS[pixelCoord] = layer2.posWS;
        gPrevCoord[pixelCoord] = layer2.prevCoord;
        gMask[pixelCoord] = float4(0.0, 0.0, 1.0, 1.0);
    }


    return;
}
