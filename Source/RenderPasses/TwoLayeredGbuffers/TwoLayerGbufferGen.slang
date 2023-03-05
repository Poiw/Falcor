import Scene.Raster;

RasterizerOrderedTexture2D<float4> gDepthBuffer;
Texture2D<float> gFirstDepthBuffer;


cbuffer PerFrameCB {
    float gEps;
};

// Default Vertex Shader
VSOut vsMain(VSIn vIn) {
    // VSOut vOut;
    // const GeometryInstanceID instanceID = { vIn.instanceID };

    // float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    // float3 posW = mul(worldMat, float4(vIn.pos, 1.f)).xyz;
    // vOut.posW = posW;
    // vOut.posH = mul(gViewProjMat, float4(posW, 1.f));
    // return vOut;

    return defaultVS(vIn);

}

struct PSOut {
    float4 gDebug : SV_TARGET0;
    float4 gDebug2 : SV_TARGET1;
    // float4 tangentWS : SV_TARGET2;
    // float4 posWS : SV_TARGET3;
};

PSOut psMain(VSOut vsOut) : SV_TARGET {
    int2 pixelIndex = int2(vsOut.posH.xy);
    PSOut psOut = {};

    // psOut.normWS = float4(vsOut.normalW, 0);
    // psOut.tangentWS = vsOut.tangentW;
    // psOut.posWS = float4(vsOut.posW, 0);

    // NDC space depth, range from [0, 1]
    float depth = vsOut.posH.z;
    // View space depth (depth in world units), when using pespective proj.
    // float depthVS = vsOut.posH.w;

    // Generate depth map
    // gDepthBuffer[pixelIndex] = min(float4(depth, depth, depth, 1), gDepthBuffer[pixelIndex]);
    // gDepthBuffer[pixelIndex] = float4(depth, depth, gDepthBuffer[pixelIndex][2], 1);

    if(depth < gFirstDepthBuffer[pixelIndex] + gEps)
        discard;

    psOut.gDebug = float4(vsOut.normalW, 0);
    psOut.gDebug2 = float4(depth, 0, 0, 0);
    // psOut.gDebug = float4(vsOut.posH.z, 0, 0, 0);
    // psOut.gDebug2 = float4(posH.z / posH.w, 0, 0, 0);
    return psOut;
}
