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
};

struct Prop {
    float2 coord;
    float3 norm;
    float3 albedo;
    float3 posW;
};

float gaussian(float x) {
    return exp(-x * x) / sqrt(2 * 3.14159265358979323846) * 3;
}

float gaussian3(float3 a) {
    return gaussian(a.x) * gaussian(a.y) * gaussian(a.z);
}

float gaussian2(float2 a) {
    return gaussian(a.x) * gaussian(a.y);
}

float similarity(const Prop cur, const Prop near)
{
    float2 coord_dif = (cur.coord - near.coord);
    float3 pos_dif = (cur.posW - near.posW) * 5;
    float3 pos_max = max(cur.posW, near.posW);
    float3 pos_min = min(cur.posW, near.posW);
    // pos_dif = (pos_dif - pos_min) / (pos_max - pos_min);
    float3 norm_dif = (cur.norm - near.norm) / 2;
    float3 albedo_dif = (cur.albedo - near.albedo);

    return gaussian3(albedo_dif) * gaussian3(pos_dif) * gaussian3(norm_dif);

    // float3 albedo_dif = abs(cur.albedo - near.albedo);
    // return albedo_dif.x + albedo_dif.y + albedo_dif.z;

}

float albedoDiff(const Prop cur, const Prop near)
{
    float3 albedo_dif = abs(cur.albedo - near.albedo);
    return albedo_dif.x + albedo_dif.y + albedo_dif.z;
}

[numthreads(16, 16, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;


    uint frameCount = gFrameCount[uint2(0, 0)];

    if (frameCount == 0) {
        gRender[pixelCoord] = gCenterRender[pixelCoord];
    }
    else {
        uint2 prevCoord = gCurPrevCoord[pixelCoord].xy;

        float curSim = -1.;
        float3 shading = 0;

        Prop cur, near;
        cur.coord = float2(prevCoord) / gFrameDim;
        cur.norm = gCurNormWS[pixelCoord].xyz;
        cur.albedo = gCurDiffOpacity[pixelCoord].xyz;
        cur.posW = gCurPosWS[pixelCoord].xyz;

        for (int step = 1; step <= 9; step += 4) {
            for (int i = -2; i <= 2; ++i) {
                for (int j = -2; j <= 2; ++j) {

                    int2 nearCoord = (int2)prevCoord + int2(i * step, j * step);
                    if (nearCoord.x >= 0 && nearCoord.x < gFrameDim.x && nearCoord.y >= 0 && nearCoord.y < gFrameDim.y) {
                        near.coord = float2(nearCoord) / gFrameDim;
                        near.norm = gCenterNormWS[nearCoord].xyz;
                        near.albedo = gCenterDiffOpacity[nearCoord].xyz;
                        near.posW = gCenterPosWS[nearCoord].xyz;
                        float nearSim = similarity(cur, near);
                        if (nearSim > curSim) {
                            curSim = nearSim;
                            // gRender[pixelCoord] = nearSim;
                            shading = gCenterRender[nearCoord].xyz / max(gCenterDiffOpacity[nearCoord].xyz, float3(1e-2, 1e-2, 1e-2));
                            // shading = gCenterRender[nearCoord].xyz;
                        }
                    }
                }
            }
        }

        // gRender[pixelCoord] = float4(shading, 1.);

        gRender[pixelCoord] = float4(shading * max(gCurDiffOpacity[pixelCoord].xyz, float3(1e-2, 1e-2, 1e-2)), 1.);

        // if (prevCoord.x >= 0 && prevCoord.x < gFrameDim.x && prevCoord.y >= 0 && prevCoord.y < gFrameDim.y) {

        //     float3 prevShading = gCenterRender[prevCoord].xyz / max(gCenterDiffOpacity[prevCoord].xyz, float3(1e-2, 1e-2, 1e-2));
        //     float3 curShading = max(gCurDiffOpacity[pixelCoord].xyz, float3(1e-2, 1e-2, 1e-2)) * prevShading;
        //     gRender[pixelCoord] = float4(curShading, 1.);
        // }
    }

    return;
}
