import Scene.Raster;
import GBufferHelpers;


cbuffer PerFrameCB {
    float4x4 gViewProjMat;
    float3 gCamPos;
};

// Default Vertex Shader
VSOut vsMain(VSIn vIn) {

    VSOut vOut;
    const GeometryInstanceID instanceID = { vIn.instanceID };

    float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    float3 posW = mul(worldMat, float4(vIn.pos, 1.f)).xyz;
    vOut.posW = posW;
    vOut.posH = mul(gViewProjMat, float4(posW, 1.f));

    vOut.instanceID = instanceID;
    vOut.materialID = gScene.getMaterialID(instanceID);

    vOut.texC = vIn.texC;
    vOut.normalW = mul(gScene.getInverseTransposeWorldMatrix(instanceID), vIn.unpack().normal);
    float4 tangent = vIn.unpack().tangent;
    vOut.tangentW = float4(mul((float3x3)gScene.getWorldMatrix(instanceID), tangent.xyz), tangent.w);

    return vOut;

}

struct PSOut {
    float4 gNormWS : SV_TARGET0;
    float4 gDiffOpacity : SV_TARGET1;
    float4 gPosWS : SV_TARGET2;
};

PSOut psMain(VSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) : SV_TARGET {

    int2 ipos = int2(vsOut.posH.xy);

    PSOut psOut = {};

    float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    VertexData v = prepareVertexData(vsOut, faceNormal);
    let lod = ImplicitLodTextureSampler();

    if (gScene.materials.alphaTest(v, vsOut.materialID, lod)) discard;

    const float3 viewDir = normalize(gCamPos - v.posW);
    ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

    // Create BSDF instance.
    let bsdf = gScene.materials.getBSDF(sd, lod);

    const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    psOut.gNormWS = gbuf.normW;
    psOut.gDiffOpacity = gbuf.diffuseOpacity;
    psOut.gPosWS = gbuf.posW;

    return psOut;
}
