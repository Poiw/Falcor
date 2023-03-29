import Scene.Raster;
import GBufferHelpers;

Texture2D<uint> gSecondLayerInstanceID;
Texture2D<float4> gSecondLayerPosL;

RWTexture2D<float4> gSecondLayerPosWS;

cbuffer PerFrameCB {
    uint2 gFrameDim;
};

[numthreads(1, 1, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    uint2 ipos = dispatchThreadId.xy;

    const GeometryInstanceID instanceID = { gSecondLayerInstanceID[ipos] };

    float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    float4 posL = gSecondLayerPosL[ipos];

    gSecondLayerPosWS[ipos] = mul(worldMat, posL);

    return;
}
