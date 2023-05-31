Texture2D<uint> gFrameCount;

Texture2D<float4> gCurDiffOpacity;
Texture2D<float4> gCurNormWS;
Texture2D<float4> gCurPosWS;
Texture2D<uint2>  gCurPrevCoord;

Texture2D<float4> gCenterDiffOpacity;
Texture2D<float4> gCenterNormWS;
Texture2D<float4> gCenterPosWS;
Texture2D<float4>  gCenterRender;

RWTexture2D<float4> gRender;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    uint2 gTarFrameDim;
    float gAlbedoSigma;
    float gPosWSigma;
    float gNormSigma;
    float gCoordSigma;
};

struct Prop {
    float2 coord;
    float3 norm;
    float3 albedo;
    float3 posW;
};

float gaussian(float x, float sigma) {
    return exp(-x * x / (2 * sigma * sigma)) / (sqrt(2 * 3.1415926) * sigma);
}

float gaussian3(float3 a, float sigma) {
    return gaussian(a.x, sigma) * gaussian(a.y, sigma) * gaussian(a.z, sigma);
}

float gaussian2(float2 a, float sigma) {
    return gaussian(a.x, sigma) * gaussian(a.y, sigma);
}

float similarity(const Prop cur, const Prop near)
{
    // float2 coord_dif = (cur.coord - near.coord);
    // float3 pos_dif = (cur.posW - near.posW) * 5;
    // float3 pos_max = max(cur.posW, near.posW);
    // float3 pos_min = min(cur.posW, near.posW);
    // // pos_dif = (pos_dif - pos_min) / (pos_max - pos_min);
    // float3 norm_dif = (cur.norm - near.norm) / 2;
    // float3 albedo_dif = (cur.albedo - near.albedo);

    // return gaussian3(albedo_dif) * gaussian3(pos_dif) * gaussian3(norm_dif);

    // // float3 albedo_dif = abs(cur.albedo - near.albedo);
    // // return albedo_dif.x + albedo_dif.y + albedo_dif.z;

    float coord_w = gaussian(length(cur.coord - near.coord), gCoordSigma);
    float pos_w = gaussian(length(cur.posW - near.posW), gPosWSigma);
    // float norm_w = gaussian3(cur.norm - near.norm, gNormSigma);
    float norm_w = gaussian(1 - dot(cur.norm, near.norm), gNormSigma);
    float albedo_w = gaussian(length(cur.albedo - near.albedo), gAlbedoSigma);

    return max(coord_w * norm_w * albedo_w, 1e-10);
}

float3 similarityW(const Prop cur, const Prop near)
{
    // float2 coord_dif = (cur.coord - near.coord);
    // float3 pos_dif = (cur.posW - near.posW) * 5;
    // float3 pos_max = max(cur.posW, near.posW);
    // float3 pos_min = min(cur.posW, near.posW);
    // // pos_dif = (pos_dif - pos_min) / (pos_max - pos_min);
    // float3 norm_dif = (cur.norm - near.norm) / 2;
    // float3 albedo_dif = (cur.albedo - near.albedo);

    // return gaussian3(albedo_dif) * gaussian3(pos_dif) * gaussian3(norm_dif);

    // // float3 albedo_dif = abs(cur.albedo - near.albedo);
    // // return albedo_dif.x + albedo_dif.y + albedo_dif.z;

    float coord_w = gaussian(length(cur.coord - near.coord), gCoordSigma);
    float pos_w = gaussian(length(cur.posW - near.posW), gPosWSigma);
    // float norm_w = gaussian3(cur.norm - near.norm, gNormSigma);
    float norm_w = gaussian(1 - dot(cur.norm, near.norm), gNormSigma);
    float albedo_w = gaussian3(cur.albedo - near.albedo, gAlbedoSigma);

    // return float3(length(cur.posW - near.posW), 1 - dot(cur.norm, near.norm), albedo_w);
    return near.posW;
}

// float albedoDiff(const Prop cur, const Prop near)
// {
//     float3 albedo_dif = abs(cur.albedo - near.albedo);
//     return albedo_dif.x + albedo_dif.y + albedo_dif.z;
// }

[numthreads(16, 16, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;


    uint frameCount = gFrameCount[uint2(0, 0)];

    // if (frameCount == 0) {
    //     gRender[pixelCoord] = gCenterRender[pixelCoord];
    // }
    // else {

    uint2 prevCoord;
    // if (frameCount == 0) {
    //     prevCoord = uint2(((float2)pixelCoord) / ((float2)gTarFrameDim) * ((float2)gFrameDim));
    // }
    // else {
        prevCoord = gCurPrevCoord[pixelCoord].xy;
    // }

    Prop cur, near;
    cur.coord = float2(prevCoord) / float2(gFrameDim);
    cur.norm = gCurNormWS[pixelCoord].xyz;
    cur.albedo = gCurDiffOpacity[pixelCoord].xyz;
    cur.posW = gCurPosWS[pixelCoord].xyz;

    int2 stepList[] = { int2(1, 1), int2(0.005 * gFrameDim), int2(0.05 * gFrameDim) };
    int stepNumList[] = { 3, 2, 1 };
    // int2 stepList[] = { int2(1, 1)};
    // int stepNumList[] = { 0};

    float weight[] = {0., 0., 0., 0.};
    float3 shadingList[4];
    shadingList[0] = float3(0., 0., 0.);
    shadingList[1] = float3(0., 0., 0.);
    shadingList[2] = float3(0., 0., 0.);
    shadingList[3] = float3(0., 0., 0.);

    for (int idx = 0; idx < 3; ++idx) {
        int2 step = stepList[idx];
        int begin = -stepNumList[idx];
        int end = stepNumList[idx];

        for (int i = begin; i <= end; ++i) {
            for (int j = begin; j <= end; ++j) {

                int2 nearCoord = (int2)prevCoord + int2(i, j) * step;
                if (nearCoord.x >= 0 && nearCoord.x < gFrameDim.x && nearCoord.y >= 0 && nearCoord.y < gFrameDim.y) {

                    near.coord = float2(nearCoord) / float2(gFrameDim);
                    near.norm = gCenterNormWS[nearCoord].xyz;
                    near.albedo = gCenterDiffOpacity[nearCoord].xyz;
                    near.posW = gCenterPosWS[nearCoord].xyz;

                    float nearSim = similarity(cur, near);

                    // if (nearSim > curSim) {
                    //     curSim = nearSim;
                    //     // gRender[pixelCoord] = nearSim;
                    //     shading = gCenterRender[nearCoord].xyz / max(gCenterDiffOpacity[nearCoord].xyz, float3(1e-2, 1e-2, 1e-2));
                    //     // shading = gCenterRender[nearCoord].xyz;
                    // }

                    float minw = weight[0];
                    int minidx = 0;
                    for (int k = 1; k < 4; ++k) {
                        if (weight[k] < minw) {
                            minw = weight[k];
                            minidx = k;
                        }
                    }

                    if (nearSim > minw) {
                        weight[minidx] = nearSim;
                        shadingList[minidx] = gCenterRender[nearCoord].xyz / max(gCenterDiffOpacity[nearCoord].xyz, float3(1e-2, 1e-2, 1e-2));
                        // shadingList[minidx] = gCenterDiffOpacity[nearCoord].xyz;
                        // shadingList[minidx] = w;
                    }

                    // for (int k = 0; k < 4; ++k) {
                    //     if (nearSim > weight[k]) {
                    //         weight[k] = nearSim;
                    //         shadingList[k] = gCenterRender[nearCoord].xyz / max(gCenterDiffOpacity[nearCoord].xyz, float3(1e-2, 1e-2, 1e-2));
                    //         // shadingList[k] = gCenterDiffOpacity[nearCoord].xyz;
                    //         // shadingList[k] = w;
                    //         break;
                    //     }
                    // }

                }
            }
        }
    }


    float3 shading = 0;
    for (int k = 0; k < 4; ++k) {
        shading += shadingList[k] * weight[k];
    }
    float weightSum = weight[0] + weight[1] + weight[2] + weight[3];
    if (weightSum > 0) {
        shading /= weightSum;
    }

    // if (weightSum > 0) weightSum = 1.;

    gRender[pixelCoord] = float4(shading * max(gCurDiffOpacity[pixelCoord].xyz, float3(1e-2, 1e-2, 1e-2)), 1.);
    // gRender[pixelCoord] = float4(shading, 1.);
    // gRender[pixelCoord] = float4(weightSum, 0, 0, 1);

    // if (prevCoord.x >= 0 && prevCoord.x < gFrameDim.x && prevCoord.y >= 0 && prevCoord.y < gFrameDim.y) {

    //     float3 prevShading = gCenterRender[prevCoord].xyz / max(gCenterDiffOpacity[prevCoord].xyz, float3(1e-2, 1e-2, 1e-2));
    //     float3 curShading = max(gCurDiffOpacity[pixelCoord].xyz, float3(1e-2, 1e-2, 1e-2)) * prevShading;
    //     gRender[pixelCoord] = float4(curShading, 1.);
    // }
    // }

    return;
}
