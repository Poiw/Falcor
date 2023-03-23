import Scene.Raster;
import GBufferHelpers;

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


[numthreads(16, 16, 1)]
void csMain(uint3 dispatchThreadId: SV_DispatchThreadID) {

    // First Layer
    uint2 pixelCoord = dispatchThreadId.xy;


    uint2 prevCoord = gCurPrevCoord[pixelCoord].xy;

    if (prevCoord.x >= 0 && prevCoord.x < gFrameDim.x && prevCoord.y >= 0 && prevCoord.y < gFrameDim.y) {

        float3 prevShading = gCenterRender[prevCoord].xyz / max(gCenterDiffOpacity[prevCoord].xyz, float3(1e-6, 1e-6, 1e-6));
        float3 curShading = gCurDiffOpacity[pixelCoord].xyz * prevShading;
        gRender[pixelCoord] = float4(curShading, 1.);
    }
    else {
        gRender[pixelCoord] = float4(0, 0, 0, 1,);
    }

    return;
}
