import Scene.Raster;
import GBufferHelpers;

Texture2D<uint> gFirstLayerInstanceID;
Texture2D<float4> gFirstLayerPosL;

Texture2D<uint> gSecondLayerInstanceID;
Texture2D<float4> gSecondLayerPosL;

RWTexture2D<float4> gFirstLayerPosWS;
RWTexture2D<float4> gSecondLayerPosWS;

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

    const GeometryInstanceID instanceID2 = { gSecondLayerInstanceID[ipos] };

    float4x4 worldMat2 = gScene.getWorldMatrix(instanceID2);
    float4 posL2 = gSecondLayerPosL[ipos];

    gSecondLayerPosWS[ipos] = mul(worldMat2, posL2);

    return;
}
