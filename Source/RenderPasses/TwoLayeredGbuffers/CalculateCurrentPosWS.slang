import Scene.Raster;
import GBufferHelpers;

Texture2D<uint> gFirstLayerInstanceID;
Texture2D<float4> gFirstLayerPosL;

Texture2D<uint> gSecondLayerInstanceID;
Texture2D<float4> gSecondLayerPosL;

RWTexture2D<float4> gFirstLayerPosWS;
RWTexture2D<float4> gSecondLayerPosWS;
RWTexture2D<float2> gFirstLayerMvec;
RWTexture2D<float2> gSecondLayerMvec;

cbuffer PerFrameCB {
    uint2 gFrameDim;
};

[numthreads(1, 1, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    uint2 ipos = dispatchThreadId.xy;

    const GeometryInstanceID instanceID1 = { gFirstLayerInstanceID[ipos] };

    float4x4 worldMat1 = gScene.getWorldMatrix(instanceID1);
    float4 posL1 = gFirstLayerPosL[ipos];

    gFirstLayerPosWS[ipos] = mul(worldMat1, posL1);
    float4 gFirstLayerPrevPosWS = mul(gScene.getPrevWorldMatrix(instanceID1), posL1);
    float4 prevPosH = mul(gScene.camera.data.prevViewProjMatNoJitter, gFirstLayerPrevPosWS);
    float2 prevCoord = prevPosH.xy / prevPosH.w;
    float4 posH = mul(gScene.camera.data.viewProjMat, gFirstLayerPosWS[ipos]);
    float2 coord = posH.xy / posH.w;

    #ifdef FALCOR_FLIP_Y
        coord *= float2(0.5, 0.5);
        prevCoord *= float2(0.5, 0.5);
    #else
        coord *= float2(0.5, -0.5);
        prevCoord *= float2(0.5, -0.5);
    #endif
    coord += float2(0.5, 0.5);
    prevCoord += float2(0.5, 0.5);

    // coord = (int2(coord * gFrameDim).xy + 0.5f) / (float2(gFrameDim));
    gFirstLayerMvec[ipos] = prevCoord - coord + float2(gScene.camera.data.jitterX, -gScene.camera.data.jitterY);

    const GeometryInstanceID instanceID2 = { gSecondLayerInstanceID[ipos] };

    float4x4 worldMat2 = gScene.getWorldMatrix(instanceID2);
    float4 posL2 = gSecondLayerPosL[ipos];

    gSecondLayerPosWS[ipos] = mul(worldMat2, posL2);
    float4 gSecondLayerPrevPosWS = mul(gScene.getPrevWorldMatrix(instanceID2), posL2);
    float4 prevPosH2 = mul(gScene.camera.data.prevViewProjMatNoJitter, gSecondLayerPrevPosWS);
    float2 prevCoord2 = prevPosH2.xy / prevPosH2.w;
    float4 posH2 = mul(gScene.camera.data.viewProjMat, gSecondLayerPosWS[ipos]);
    float2 coord2 = posH2.xy / posH2.w;

    #ifdef FALCOR_FLIP_Y
        coord2 *= float2(0.5, 0.5);
        prevCoord2 *= float2(0.5, 0.5);
    #else
        coord2 *= float2(0.5, -0.5);
        prevCoord2 *= float2(0.5, -0.5);
    #endif
    coord2 += float2(0.5, 0.5);
    prevCoord2 += float2(0.5, 0.5);

    // coord2 = (int2(coord2 * gFrameDim).xy + 0.5f) / (float2(gFrameDim));
    gSecondLayerMvec[ipos] = prevCoord2 - coord2 + float2(gScene.camera.data.jitterX, -gScene.camera.data.jitterY);

    return;
}
