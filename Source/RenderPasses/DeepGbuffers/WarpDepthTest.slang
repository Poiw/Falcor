cbuffer PerFrameCB {
    float4x4 gViewProjMat;
    uint2 gCurDim;
    uint gLevelNum;
    float gLinearZScale;
    uint gSubpixelNum;
};

Texture2DArray<float4> gPosW;
Texture2DArray<float4> gNormal;

RWTexture2D<uint> gDepth;


[numthreads(8, 8, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;
    uint layer = dispatchThreadId.z;
    float skip = 1.0 / float(gSubpixelNum);

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
            float3 posW = 0;

            if (length(ltNormal) > 0.5) {
                w = (1 - abs(curCoord.x - lt.x)) * (1 - abs(curCoord.y - lt.y));
                posW += gPosW[int3(lt, layer)].xyz * w;
                weight += w;
            }

            if (length(rbNormal) > 0.5) {
                w = (1 - abs(curCoord.x - rb.x)) * (1 - abs(curCoord.y - rb.y));
                posW += gPosW[int3(rb, layer)].xyz * w;
                weight += w;
            }

            if (length(lbNormal) > 0.5) {
                w = (1 - abs(curCoord.x - lb.x)) * (1 - abs(curCoord.y - lb.y));
                posW += gPosW[int3(lb, layer)].xyz * w;
                weight += w;
            }

            if (length(rtNormal) > 0.5) {
                w = (1 - abs(curCoord.x - rt.x)) * (1 - abs(curCoord.y - rt.y));
                posW += gPosW[int3(rt, layer)].xyz * w;
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

            InterlockedMin(gDepth[coord], depth);



        }

    return;
}
