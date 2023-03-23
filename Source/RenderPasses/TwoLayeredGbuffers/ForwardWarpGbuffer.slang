import Scene.Raster;
import GBufferHelpers;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    float4x4 gCenterViewProjMat;
    float4x4 gCenterViewProjMatInv;
    float4x4 gCurViewProjMat;
    uint subSampleNum;
    bool gEnableSubPixel;
};


Texture2D<float4> gFirstLayerNormWS;
Texture2D<float4> gFirstLayerPosWS;
Texture2D<float4> gFirstLayerDiffOpacity;
Texture2D<float4> gSecondLayerNormWS;
Texture2D<float4> gSecondLayerPosWS;
Texture2D<float4> gSecondLayerDiffOpacity;

Texture2D<float> gFirstDepth;
Texture2D<float> gSecondDepth;

Texture2D<uint> gProjFirstLayerDepthTest0;
Texture2D<uint> gProjSecondLayerDepthTest0;
Texture2D<uint> gProjFirstLayerDepthTest1;
Texture2D<uint> gProjSecondLayerDepthTest1;
Texture2D<uint> gProjFirstLayerDepthTest2;
Texture2D<uint> gProjSecondLayerDepthTest2;
Texture2D<uint> gProjFirstLayerDepthTest3;
Texture2D<uint> gProjSecondLayerDepthTest3;

RWTexture2D<float4> gProjFirstLayerNormWS0;
RWTexture2D<float4> gProjFirstLayerDiffOpacity0;
RWTexture2D<float4> gProjFirstLayerNormWS1;
RWTexture2D<float4> gProjFirstLayerDiffOpacity1;
RWTexture2D<float4> gProjFirstLayerNormWS2;
RWTexture2D<float4> gProjFirstLayerDiffOpacity2;
RWTexture2D<float4> gProjFirstLayerNormWS3;
RWTexture2D<float4> gProjFirstLayerDiffOpacity3;

RWTexture2D<float4> gProjSecondLayerNormWS0;
RWTexture2D<float4> gProjSecondLayerDiffOpacity0;
RWTexture2D<float4> gProjSecondLayerNormWS1;
RWTexture2D<float4> gProjSecondLayerDiffOpacity1;
RWTexture2D<float4> gProjSecondLayerNormWS2;
RWTexture2D<float4> gProjSecondLayerDiffOpacity2;
RWTexture2D<float4> gProjSecondLayerNormWS3;
RWTexture2D<float4> gProjSecondLayerDiffOpacity3;



[numthreads(1, 1, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;


    if (gEnableSubPixel) {
        float st = 0.5 / subSampleNum;
        float step = 1.0 / subSampleNum;

        for (float ix = st; ix < 1.0; ix += step) {
            for (float iy = st; iy < 1.0; iy += step) {

                float2 uv = float2(pixelCoord) / gFrameDim + float2(ix, iy) / gFrameDim;

                uv -= 0.5;
                #ifdef FALCOR_FLIP_Y
                    uv *= float2(2, 2);
                #else
                    uv *= float2(2, -2);
                #endif

                // float4 posWS = mul(gCenterViewProjMatInv, float4(uv, gFirstDepth[pixelCoord], 1));

                // float4 tarPosH = mul(gCurViewProjMat, posWS);
                float4 tarPosH = mul(mul(gCurViewProjMat, gCenterViewProjMatInv), float4(uv, gFirstDepth[pixelCoord], 1));
                float2 tarCrd = tarPosH.xy / tarPosH.w;
                float z = tarPosH.z / tarPosH.w;

                #ifdef FALCOR_FLIP_Y
                    tarCrd *= float2(0.5, 0.5);
                #else
                    tarCrd *= float2(0.5, -0.5);
                #endif
                    tarCrd += 0.5;

                float4 normWS = gFirstLayerNormWS[pixelCoord];
                float4 diffOpacity = gFirstLayerDiffOpacity[pixelCoord];

                uint2 tarIpos0 = uint2(tarCrd * gFrameDim);
                if (asuint(z) == gProjFirstLayerDepthTest0[tarIpos0]) {
                    gProjFirstLayerNormWS0[tarIpos0] = normWS;
                    gProjFirstLayerDiffOpacity0[tarIpos0] = diffOpacity;
                }

                uint2 tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
                if (asuint(z) == gProjFirstLayerDepthTest1[tarIpos1]) {
                    gProjFirstLayerNormWS1[tarIpos1] = normWS;
                    gProjFirstLayerDiffOpacity1[tarIpos1] = diffOpacity;
                }

                uint2 tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
                if (asuint(z) == gProjFirstLayerDepthTest2[tarIpos2]) {
                    gProjFirstLayerNormWS2[tarIpos2] = normWS;
                    gProjFirstLayerDiffOpacity2[tarIpos2] = diffOpacity;
                }

                uint2 tarIpos3 = uint2(tarCrd * gFrameDim) / 8;
                if (asuint(z) == gProjFirstLayerDepthTest3[tarIpos3]) {
                    gProjFirstLayerNormWS3[tarIpos3] = normWS;
                    gProjFirstLayerDiffOpacity3[tarIpos3] = diffOpacity;
                }

            }
        }

        // Second Layer

        for (float ix = st; ix < 1.0; ix += step) {
            for (float iy = st; iy < 1.0; iy += step) {

                float2 uv = float2(pixelCoord) / gFrameDim + float2(ix, iy) / gFrameDim;

                uv -= 0.5;
                #ifdef FALCOR_FLIP_Y
                    uv *= float2(2, 2);
                #else
                    uv *= float2(2, -2);
                #endif

                // float4 posWS = mul(gCenterViewProjMatInv, float4(uv, gSecondDepth[pixelCoord], 1));

                // float4 tarPosH = mul(gCurViewProjMat, posWS);
                float4 tarPosH = mul(mul(gCurViewProjMat, gCenterViewProjMatInv), float4(uv, gSecondDepth[pixelCoord], 1));
                float2 tarCrd = tarPosH.xy / tarPosH.w;
                float z = tarPosH.z / tarPosH.w;

                #ifdef FALCOR_FLIP_Y
                    tarCrd *= float2(0.5, 0.5);
                #else
                    tarCrd *= float2(0.5, -0.5);
                #endif
                    tarCrd += 0.5;

                // uint2 tarIpos = uint2(tarCrd * gFrameDim);

                // if (asuint(z) == gProjSecondLayerDepthTest[tarIpos]) {
                //     gProjSecondLayerNormWS[tarIpos] = gSecondLayerNormWS[pixelCoord];
                //     // gProjSecondLayerNormWS[tarIpos] = float4(posWS.xyz - gSecondLayerPosWS[pixelCoord].xyz, 1);
                //     gProjSecondLayerDiffOpacity[tarIpos] = gSecondLayerDiffOpacity[pixelCoord];
                // }

                float4 normWS = gSecondLayerNormWS[pixelCoord];
                float4 diffOpacity = gSecondLayerDiffOpacity[pixelCoord];

                uint2 tarIpos0 = uint2(tarCrd * gFrameDim);
                if (asuint(z) == gProjSecondLayerDepthTest0[tarIpos0]) {
                    gProjSecondLayerNormWS0[tarIpos0] = normWS;
                    gProjSecondLayerDiffOpacity0[tarIpos0] = diffOpacity;
                }

                uint2 tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
                if (asuint(z) == gProjSecondLayerDepthTest1[tarIpos1]) {
                    gProjSecondLayerNormWS1[tarIpos1] = normWS;
                    gProjSecondLayerDiffOpacity1[tarIpos1] = diffOpacity;
                }

                uint2 tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
                if (asuint(z) == gProjSecondLayerDepthTest2[tarIpos2]) {
                    gProjSecondLayerNormWS2[tarIpos2] = normWS;
                    gProjSecondLayerDiffOpacity2[tarIpos2] = diffOpacity;
                }

                uint2 tarIpos3 = uint2(tarCrd * gFrameDim) / 8;
                if (asuint(z) == gProjSecondLayerDepthTest3[tarIpos3]) {
                    gProjSecondLayerNormWS3[tarIpos3] = normWS;
                    gProjSecondLayerDiffOpacity3[tarIpos3] = diffOpacity;
                }

            }
        }

    } else {

        float3 posWS = gFirstLayerPosWS[pixelCoord].xyz;

        float4 tarPosH = mul(gCurViewProjMat, float4(posWS, 1.0f));
        float2 tarCrd = tarPosH.xy / tarPosH.w;
        float z = tarPosH.z / tarPosH.w;

    #ifdef FALCOR_FLIP_Y
        tarCrd *= float2(0.5, 0.5);
    #else
        tarCrd *= float2(0.5, -0.5);
    #endif
        tarCrd += 0.5;

        // uint2 tarIpos = uint2(tarCrd * gFrameDim);

        // if (asuint(z) == gProjFirstLayerDepthTest[tarIpos]) {
        //     gProjFirstLayerNormWS[tarIpos] = gFirstLayerNormWS[pixelCoord];
        //     gProjFirstLayerDiffOpacity[tarIpos] = gFirstLayerDiffOpacity[pixelCoord];
        // }

        float4 normWS = gFirstLayerNormWS[pixelCoord];
        float4 diffOpacity = gFirstLayerDiffOpacity[pixelCoord];

        uint2 tarIpos0 = uint2(tarCrd * gFrameDim);
        if (asuint(z) == gProjFirstLayerDepthTest0[tarIpos0]) {
            gProjFirstLayerNormWS0[tarIpos0] = normWS;
            gProjFirstLayerDiffOpacity0[tarIpos0] = diffOpacity;
        }

        uint2 tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
        if (asuint(z) == gProjFirstLayerDepthTest1[tarIpos1]) {
            gProjFirstLayerNormWS1[tarIpos1] = normWS;
            gProjFirstLayerDiffOpacity1[tarIpos1] = diffOpacity;
        }

        uint2 tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
        if (asuint(z) == gProjFirstLayerDepthTest2[tarIpos2]) {
            gProjFirstLayerNormWS2[tarIpos2] = normWS;
            gProjFirstLayerDiffOpacity2[tarIpos2] = diffOpacity;
        }

        uint2 tarIpos3 = uint2(tarCrd * gFrameDim) / 8;
        if (asuint(z) == gProjFirstLayerDepthTest3[tarIpos3]) {
            gProjFirstLayerNormWS3[tarIpos3] = normWS;
            gProjFirstLayerDiffOpacity3[tarIpos3] = diffOpacity;
        }


        // Second Layer
        posWS = gSecondLayerPosWS[pixelCoord].xyz;

        tarPosH = mul(gCurViewProjMat, float4(posWS, 1.0f));
        tarCrd = tarPosH.xy / tarPosH.w;
        z = tarPosH.z / tarPosH.w;

    #ifdef FALCOR_FLIP_Y
        tarCrd *= float2(0.5, 0.5);
    #else
        tarCrd *= float2(0.5, -0.5);
    #endif
        tarCrd += 0.5;

        // tarIpos = uint2(tarCrd * gFrameDim);

        // if (asuint(z) == gProjSecondLayerDepthTest[tarIpos]) {
        //     gProjSecondLayerNormWS[tarIpos] = gSecondLayerNormWS[pixelCoord];
        //     gProjSecondLayerDiffOpacity[tarIpos] = gSecondLayerDiffOpacity[pixelCoord];
        // }

        normWS = gSecondLayerNormWS[pixelCoord];
        diffOpacity = gSecondLayerDiffOpacity[pixelCoord];

        tarIpos0 = uint2(tarCrd * gFrameDim);
        if (asuint(z) == gProjSecondLayerDepthTest0[tarIpos0]) {
            gProjSecondLayerNormWS0[tarIpos0] = normWS;
            gProjSecondLayerDiffOpacity0[tarIpos0] = diffOpacity;
        }

        tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
        if (asuint(z) == gProjSecondLayerDepthTest1[tarIpos1]) {
            gProjSecondLayerNormWS1[tarIpos1] = normWS;
            gProjSecondLayerDiffOpacity1[tarIpos1] = diffOpacity;
        }

        tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
        if (asuint(z) == gProjSecondLayerDepthTest2[tarIpos2]) {
            gProjSecondLayerNormWS2[tarIpos2] = normWS;
            gProjSecondLayerDiffOpacity2[tarIpos2] = diffOpacity;
        }

        tarIpos3 = uint2(tarCrd * gFrameDim) / 16;
        if (asuint(z) == gProjSecondLayerDepthTest3[tarIpos3]) {
            gProjSecondLayerNormWS3[tarIpos3] = normWS;
            gProjSecondLayerDiffOpacity3[tarIpos3] = diffOpacity;
        }

    }


    return;
}
