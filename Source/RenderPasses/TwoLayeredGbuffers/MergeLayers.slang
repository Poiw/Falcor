import Scene.Raster;
import GBufferHelpers;


Texture2D<uint> gFirstLayerDepthTest;
Texture2D<float4> gFirstLayerNormWS;
Texture2D<float4> gFirstLayerDiffOpacity;

Texture2D<uint> gSecondLayerDepthTest;
Texture2D<float4> gSecondLayerNormWS;
Texture2D<float4> gSecondLayerDiffOpacity;



RWTexture2D<float4> gNormWS;
RWTexture2D<float4> gDiffOpacity;
RWTexture2D<float4> gMask;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    int gNearestFilter;
};

struct Gbuf {
    float4 normWS;
    float4 diffOpacity;
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

    if (gFirstLayerDepthTest[pixelCoord] == uint(-1)) {

        float minDist = 1000000.0;

        for (int i = -gNearestFilter; i <= gNearestFilter; i++) {
            for (int j = -gNearestFilter; j <= gNearestFilter; j++) {
                uint2 tarCrd = pixelCoord + uint2(i, j);
                if (tarCrd.x >= 0 && tarCrd.y >= 0 && tarCrd.x < gFrameDim.x && tarCrd.y < gFrameDim.y &&
                        gFirstLayerDepthTest[tarCrd] != uint(-1) &&
                        Dist(i, j) < minDist) {

                    minDist = Dist(i, j);
                    layer1.normWS = gFirstLayerNormWS[tarCrd];
                    layer1.diffOpacity = gFirstLayerDiffOpacity[tarCrd];
                    layer1.z = gFirstLayerDepthTest[tarCrd];
                }
            }
        }

    }
    else {
        layer1.normWS = gFirstLayerNormWS[pixelCoord];
        layer1.diffOpacity = gFirstLayerDiffOpacity[pixelCoord];
        layer1.z = gFirstLayerDepthTest[pixelCoord];
    }

    // Second Layer

    if (gSecondLayerDepthTest[pixelCoord] == uint(-1)) {

        float minDist = 1000000.0;

        for (int i = -gNearestFilter; i <= gNearestFilter; i++) {
            for (int j = -gNearestFilter; j <= gNearestFilter; j++) {
                uint2 tarCrd = pixelCoord + uint2(i, j);
                if (tarCrd.x >= 0 && tarCrd.y >= 0 && tarCrd.x < gFrameDim.x && tarCrd.y < gFrameDim.y &&
                        gSecondLayerDepthTest[tarCrd] != uint(-1) &&
                        Dist(i, j) < minDist) {

                    minDist = Dist(i, j);
                    layer2.normWS = gSecondLayerNormWS[tarCrd];
                    layer2.diffOpacity = gSecondLayerDiffOpacity[tarCrd];
                    layer2.z = gSecondLayerDepthTest[tarCrd];
                }
            }
        }

    }
    else {
        layer2.normWS = gSecondLayerNormWS[pixelCoord];
        layer2.diffOpacity = gSecondLayerDiffOpacity[pixelCoord];
        layer2.z = gSecondLayerDepthTest[pixelCoord];
    }

    if (layer1.z < layer2.z) {
        gNormWS[pixelCoord] = layer1.normWS;
        gDiffOpacity[pixelCoord] = layer1.diffOpacity;
        gMask[pixelCoord] = float4(1.0, 0.0, 0.0, 1.0);
    }
    else {
        gNormWS[pixelCoord] = layer2.normWS;
        gDiffOpacity[pixelCoord] = layer2.diffOpacity;
        gMask[pixelCoord] = float4(0.0, 0.0, 1.0, 1.0);
    }


    return;
}
