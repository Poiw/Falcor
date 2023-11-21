cbuffer PerFrameCB {
    float4x4 gViewProjMat;
    uint2 gCurDim;
    uint gLevelNum;
    float gLinearZScale;
    float gNearRatio;
    uint gSubpixelNum;
};

Texture2DArray<float4> gPosW;
Texture2DArray<float4> gNormal;
Texture2DArray<float4> gAlbedo;
Texture2D<uint> gDepth;

RWTexture2D<float4> gNormalOut;
RWTexture2D<float4> gAlbedoOut;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;

    uint layer = dispatchThreadId.z;
    float skip = 1.0 / float(gSubpixelNum);

    float3 var = 0;
    int count = 0;

    float3 centerPosW = gPosW[int3(pixelCoord, layer)].xyz;

    for (int ux = max(0, pixelCoord.x-1); ux <= min(gCurDim.x-1, pixelCoord.x+1); ++ux)
        for (int uy = max(0, pixelCoord.y-1); uy <= min(gCurDim.y-1, pixelCoord.y+1); ++uy) {

            int2 tmpCoord = int2(ux, uy);

            float3 normal = gNormal[int3(tmpCoord, layer)].xyz;

            if (length(normal) > 0.5) {
                var += pow(gPosW[int3(tmpCoord, layer)].xyz - centerPosW, 2);
                count++;
            }

        }

    var /= (count - 1);
    var = sqrt(var);

    for (float ux = skip * 0.5; ux < 1.0; ux += skip)
        for (float uy = skip * 0.5; uy < 1.0; uy += skip) {

            float2 curCoord = (pixelCoord + float2(ux, uy) - float2(0.5, 0.5));

            int2 lt = int2(floor(curCoord));
            int2 rb = int2(ceil(curCoord));

            lt.x = clamp(lt.x, 0, gCurDim.x - 1);
            lt.y = clamp(lt.y, 0, gCurDim.y - 1);
            rb.x = clamp(rb.x, 0, gCurDim.x - 1);
            rb.y = clamp(rb.y, 0, gCurDim.y - 1);

            int2 lb = int2(lt.x, rb.y);
            int2 rt = int2(rb.x, lt.y);


            float3 ltNormal = gNormal[int3(lt, layer)].xyz;
            float3 rbNormal = gNormal[int3(rb, layer)].xyz;
            float3 lbNormal = gNormal[int3(lb, layer)].xyz;
            float3 rtNormal = gNormal[int3(rt, layer)].xyz;

            float weight = 0, w;
            float3 posW = 0, cPosW;

            if (length(ltNormal) > 0.5) {

                cPosW = gPosW[int3(lt, layer)].xyz;
                cPosW = clamp(cPosW, centerPosW - var * gNearRatio, centerPosW + var * gNearRatio);

                w = (1 - abs(curCoord.x - lt.x)) * (1 - abs(curCoord.y - lt.y));
                posW += cPosW * w;
                weight += w;
            }

            if (length(rbNormal) > 0.5) {

                cPosW = gPosW[int3(rb, layer)].xyz;
                cPosW = clamp(cPosW, centerPosW - var * gNearRatio, centerPosW + var * gNearRatio);

                w = (1 - abs(curCoord.x - rb.x)) * (1 - abs(curCoord.y - rb.y));
                posW += cPosW * w;
                weight += w;
            }

            if (length(lbNormal) > 0.5) {

                cPosW = gPosW[int3(lb, layer)].xyz;
                cPosW = clamp(cPosW, centerPosW - var * gNearRatio, centerPosW + var * gNearRatio);

                w = (1 - abs(curCoord.x - lb.x)) * (1 - abs(curCoord.y - lb.y));
                posW += cPosW * w;
                weight += w;
            }

            if (length(rtNormal) > 0.5) {

                cPosW = gPosW[int3(rt, layer)].xyz;
                cPosW = clamp(cPosW, centerPosW - var * gNearRatio, centerPosW + var * gNearRatio);

                w = (1 - abs(curCoord.x - rt.x)) * (1 - abs(curCoord.y - rt.y));
                posW += cPosW * w;
                weight += w;
            }

            if (weight > 0) {
                posW /= weight;
            }
            else {
                continue;
            }

            // float4 posW = lerp(lerp(ltPosW, rtPosW, float4(curCoord.x - lt.x)), lerp(lbPosW, rbPosW, float4(curCoord.x - lt.x)), float4(curCoord.y - lt.y));

            // float4 posW = float4(lerp(lerp(ltPosW, rtPosW, curCoord.x - lt.x), lerp(lbPosW, rbPosW, curCoord.x - lt.x), curCoord.y - lt.y), 1);
            // float4 posW = float4(rtPosW, 1);

            float4 posH = mul(gViewProjMat, float4(posW, 1));

            float2 Crd = posH.xy / posH.w;

            uint depth = uint(posH.w * gLinearZScale);

            #ifdef FALCOR_FLIP_Y
                Crd *= float2(0.5, 0.5);
            #else
                Crd *= float2(0.5, -0.5);
            #endif

            Crd += 0.5;

            uint2 coord = uint2(Crd * gCurDim);

            if (gDepth[coord] == depth) {
                gNormalOut[coord] = gNormal[int3(pixelCoord, layer)];
                // gAlbedoOut[coord] = gAlbedo[int3(pixelCoord, layer)];
                gAlbedoOut[coord] = (gLevelNum - 1 - layer) * float4(1, 0, 0, 1) / (gLevelNum-1) + (layer) * float4(0, 0, 1, 1) / (gLevelNum-1);
            }


        }


    return;
}
