

Texture2D<float4> gCurNormal;
Texture2D<float4> gCurBaseColor;
Texture2D<float4> gCurRender;
Texture2D<float2> gCurMotionVector;

Texture2D<float4> gPrevNormal;
Texture2D<float4> gPrevBaseColor;
Texture2D<float4> gPrevRender;

RWTexture2D<float4> gWarped;

cbuffer PerFrameCB {
    uint2 gFrameDim;
    float coord_sigma;
    float feature_sigma;
};

struct Prop {
    float2 coord;
    float3 norm;
    float3 albedo;
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

    float coord_w = gaussian2(cur.coord - near.coord, coord_sigma);
    // float norm_w = gaussian3(cur.norm - near.norm, gNormSigma);
    float norm_w = gaussian3(cur.norm - near.norm, feature_sigma);
    float albedo_w = gaussian3(cur.albedo - near.albedo, feature_sigma);

    return max(coord_w * norm_w * albedo_w, 1e-10);
}


// float albedoDiff(const Prop cur, const Prop near)
// {
//     float3 albedo_dif = abs(cur.albedo - near.albedo);
//     return albedo_dif.x + albedo_dif.y + albedo_dif.z;
// }

[numthreads(32, 32, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    float2 prevCoord = (float2)pixelCoord - gCurMotionVector[pixelCoord].xy;

    Prop cur, near;
    cur.coord = prevCoord;
    cur.norm = gCurNormal[pixelCoord].xyz;
    cur.albedo = gCurBaseColor[pixelCoord].xyz;

    int2 stepList[] = { int2(1, 1), int2(3, 3), int2(5, 5) , int2(9, 9)};
    // int2 stepList[] = { int2(1, 1)};
    // int stepNumList[] = { 0};

    float weight[] = {0., 0., 0., 0.};
    float3 shadingList[4];
    shadingList[0] = float3(0., 0., 0.);
    shadingList[1] = float3(0., 0., 0.);
    shadingList[2] = float3(0., 0., 0.);
    shadingList[3] = float3(0., 0., 0.);

    for (int idx = 0; idx < 4; ++idx) {
        int2 step = stepList[idx];

        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {

                int2 nearCoord = (int2)(prevCoord + float2(0.5, 0.5)) + int2(i, j) * step;
                if (nearCoord.x >= 0 && nearCoord.x < gFrameDim.x && nearCoord.y >= 0 && nearCoord.y < gFrameDim.y) {

                    near.coord = float2(nearCoord);
                    near.norm = gPrevNormal[nearCoord].xyz;
                    near.albedo = gPrevBaseColor[nearCoord].xyz;

                    float nearSim = similarity(cur, near);

                    // if (nearSim > curSim) {
                    //     curSim = nearSim;
                    //     // gRender[pixelCoord] = nearSim;
                    //     shading = gCenterRender[nearCoord].xyz / max(gCenterDiffOpacity[nearCoord].xyz, float3(1e-2, 1e-2, 1e-2));
                    //     // shading = gCenterRender[nearCoord].xyz;
                    // }


                    float minv = 1e10;
                    int minIdx = -1;
                    for (int k = 0; k < 4; ++k) {
                        if (weight[k] < minv) {
                            minv = weight[k];
                            minIdx = k;
                        }
                    }

                    if (nearSim > weight[minIdx]) {
                        weight[minIdx] = nearSim;
                        shadingList[minIdx] = gPrevRender[nearCoord].xyz / max(gPrevBaseColor[nearCoord].xyz, float3(1e-2, 1e-2, 1e-2));
                        // shadingList[minIdx] = gPrevRender[nearCoord].xyz;
                    }
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

    gWarped[pixelCoord] = float4(shading * max(gCurBaseColor[pixelCoord].xyz, float3(1e-2, 1e-2, 1e-2)), 1.);
    // gWarped[pixelCoord] = float4(shading, 1.);

    return;
}
