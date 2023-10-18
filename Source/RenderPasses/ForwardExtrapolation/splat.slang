cbuffer PerFrameCB {
    uint2 gFrameDim;
    uint gKernelSize;
    float gSplatSigma;
    float gSplatDistSigma;
    uint gStrideNum;
};

Texture2D<float4> gTempWarpTex;
Texture2D<uint> gTempDepthTex;

RWTexture2D<float4> targetRenderTex;

float softmax_exp(float x) {
    return exp(x * gSplatSigma);
}

float4 tonemap(float4 x, float mu) {
    return log(1 + mu * x) / log(1 + mu);
}

float4 detonemap(float4 x, float mu) {
    return (exp(x * log(1 + mu)) - 1) / mu;
}

[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    int2 pixelCoord = dispatchThreadId.xy;

    float total_weight = 0.0f;

    int size = (gKernelSize - 1) / 2;

    float mu = 8.;

    float4 targetRender = 0.;
    // targetRenderTex[pixelCoord] = 0.;
    for (int stride = 1; stride <= gStrideNum * 4 - 3; stride += 4) {
        for (int ux = -size; ux <= size; ux++) {
            for (int uy = -size; uy <= size; uy++) {

                int2 neighborCoord = pixelCoord + int2(ux, uy) * stride;

                if (neighborCoord.x >= 0 && neighborCoord.x < gFrameDim.x && neighborCoord.y >= 0 && neighborCoord.y < gFrameDim.y && gTempDepthTex[neighborCoord] != uint(-1)) {
                    float depth = (float)gTempDepthTex[neighborCoord] / 262144.0f;
                    // float weight = softmax_exp(depth) * abs(gKernelSize + 1 - ux) * abs(gKernelSize + 1 - uy) / ((gKernelSize+1) * (gKernelSize+1));
                    float weight = softmax_exp(depth) * exp(-abs(gSplatDistSigma * ux)) * exp(-abs(gSplatDistSigma * uy));
                    total_weight += weight;
                    targetRender += tonemap(gTempWarpTex[neighborCoord], mu) * weight;
                }
            }
        }
    }

    if (total_weight > 0.0f) {
        targetRender /= total_weight;
    }

    targetRender = detonemap(targetRender, mu);
    targetRenderTex[pixelCoord] = targetRender;

    // targetRenderTex[pixelCoord] = 1;

    // targetRenderTex[pixelCoord] = gTempWarpTex[pixelCoord];

    return;
}
