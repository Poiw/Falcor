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

Texture2D<float4> gFirstLayerPosWS;
Texture2D<float4> gSecondLayerPosWS;

Texture2D<float> gFirstDepth;
Texture2D<float> gSecondDepth;

RWTexture2D<uint> gProjFirstLayerDepthTest0;
RWTexture2D<uint> gProjSecondLayerDepthTest0;
RWTexture2D<uint> gProjFirstLayerDepthTest1;
RWTexture2D<uint> gProjSecondLayerDepthTest1;
RWTexture2D<uint> gProjFirstLayerDepthTest2;
RWTexture2D<uint> gProjSecondLayerDepthTest2;
RWTexture2D<uint> gProjFirstLayerDepthTest3;
RWTexture2D<uint> gProjSecondLayerDepthTest3;



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

                uint2 tarIpos0 = uint2(tarCrd * gFrameDim);
                uint2 tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
                uint2 tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
                uint2 tarIpos3 = uint2(tarCrd * gFrameDim) / 8;

                InterlockedMin(gProjFirstLayerDepthTest0[tarIpos0], asuint(z));
                InterlockedMin(gProjFirstLayerDepthTest1[tarIpos1], asuint(z));
                InterlockedMin(gProjFirstLayerDepthTest2[tarIpos2], asuint(z));
                InterlockedMin(gProjFirstLayerDepthTest3[tarIpos3], asuint(z));
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

                uint2 tarIpos0 = uint2(tarCrd * gFrameDim);
                uint2 tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
                uint2 tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
                uint2 tarIpos3 = uint2(tarCrd * gFrameDim) / 8;

                InterlockedMin(gProjSecondLayerDepthTest0[tarIpos0], asuint(z));
                InterlockedMin(gProjSecondLayerDepthTest1[tarIpos1], asuint(z));
                InterlockedMin(gProjSecondLayerDepthTest2[tarIpos2], asuint(z));
                InterlockedMin(gProjSecondLayerDepthTest3[tarIpos3], asuint(z));
            }
        }
    }

    else {

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

        // InterlockedMin(gProjFirstLayerDepthTest[tarIpos], asuint(z));
        uint2 tarIpos0 = uint2(tarCrd * gFrameDim);
        uint2 tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
        uint2 tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
        uint2 tarIpos3 = uint2(tarCrd * gFrameDim) / 8;

        InterlockedMin(gProjFirstLayerDepthTest0[tarIpos0], asuint(z));
        InterlockedMin(gProjFirstLayerDepthTest1[tarIpos1], asuint(z));
        InterlockedMin(gProjFirstLayerDepthTest2[tarIpos2], asuint(z));
        InterlockedMin(gProjFirstLayerDepthTest3[tarIpos3], asuint(z));


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

        // InterlockedMin(gProjSecondLayerDepthTest[tarIpos], asuint(z));
        tarIpos0 = uint2(tarCrd * gFrameDim);
        tarIpos1 = uint2(tarCrd * gFrameDim) / 2;
        tarIpos2 = uint2(tarCrd * gFrameDim) / 4;
        tarIpos3 = uint2(tarCrd * gFrameDim) / 8;

        InterlockedMin(gProjSecondLayerDepthTest0[tarIpos0], asuint(z));
        InterlockedMin(gProjSecondLayerDepthTest1[tarIpos1], asuint(z));
        InterlockedMin(gProjSecondLayerDepthTest2[tarIpos2], asuint(z));
        InterlockedMin(gProjSecondLayerDepthTest3[tarIpos3], asuint(z));

    }

    return;
}
