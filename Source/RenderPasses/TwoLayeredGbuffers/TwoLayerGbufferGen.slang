import Scene.Raster;
import GBufferHelpers;

Texture2D<float2> gFirstLinearZBuffer;


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


#define is_valid(name) (is_valid_##name != 0)

struct PSOut {
    float4 gDebug : SV_TARGET0;
    float4 gMask : SV_TARGET1;
    float4 gNormWS : SV_TARGET2;
    float4 gDiffOpacity : SV_TARGET3;
    // float4 tangentWS : SV_TARGET2;
    // float4 posWS : SV_TARGET3;
};

PSOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) : SV_TARGET {

    int2 ipos = int2(vsOut.posH.xy);

    // Not a valid second layer gbuffer
    float linearZ = (vsOut.posH.z * vsOut.posH.w - gEps);
    if (linearZ < gFirstLinearZBuffer[ipos][0] + 0.00001) // pecision err
        discard;

    PSOut psOut = {};

    float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    VertexData v = prepareVertexData(vsOut, faceNormal);
    let lod = ImplicitLodTextureSampler();

    if (gScene.materials.alphaTest(v, vsOut.materialID, lod)) discard;

    const float3 viewDir = normalize(gScene.camera.getPosition() - v.posW);
    ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

    // Create BSDF instance.
    let bsdf = gScene.materials.getBSDF(sd, lod);

    const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    psOut.gDebug = float4(vsOut.posH.z * vsOut.posH.w, 0, 0, 0);
    psOut.gNormWS = gbuf.normW;
    psOut.gDiffOpacity = gbuf.diffuseOpacity;
    psOut.gMask = float4(1., 0., 0., 0.);


    return psOut;
}


// Old Codes
    // gTargetPosWSBuffer[ipos] = float4(gPosWSBuffer[ipos].xyz, 0);

    // // Not a valid second layer gbuffer
    // // float linearZ = (vsOut.posH.z * vsOut.posH.w - gEps);
    // // if(linearZ <= gFirstLinearZBuffer[ipos][0] - 0.00001)
    // //     discard;

    // PSOut psOut = {};

    // // float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    // // VertexData v = prepareVertexData(vsOut, faceNormal);
    // // let lod = ImplicitLodTextureSampler();

    // // const float3 viewDir = normalize(gScene.camera.getPosition() - v.posW);
    // // ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

    // // // Create BSDF instance.
    // // let bsdf = gScene.materials.getBSDF(sd, lod);

    // // const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    // psOut.gDebug = float4(vsOut.posW, 0);
    // // psOut.gNormW = float4(vsOut.normalW, 0);
    // // psOut.gDiffOpacity = gbuf.diffuseOpacity;
