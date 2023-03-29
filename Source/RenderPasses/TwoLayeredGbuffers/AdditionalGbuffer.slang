import Scene.Raster;
import GBufferHelpers;

#ifndef INTERPOLATION_MODE
#define INTERPOLATION_MODE linear
#endif

cbuffer PerFrameCB {
    float4x4 gViewProjMat;
    float3 gCamPos;
};

struct MyVSOut
{
    INTERPOLATION_MODE float3 normalW : NORMAL;    ///< Shading normal in world space (not normalized!).
    INTERPOLATION_MODE float4 tangentW : TANGENT;  ///< Shading tangent in world space (not normalized!).
    INTERPOLATION_MODE float2 texC : TEXCRD;       ///< Texture coordinate.
    INTERPOLATION_MODE float3 posW : POSW;         ///< Position in world space.
    INTERPOLATION_MODE float4 prevPosH : PREVPOSH; ///< Position in clip space for the previous frame.
    INTERPOLATION_MODE float4 posL : POSL;         ///< Position in original model space.

    // Per-triangle data
    nointerpolation GeometryInstanceID instanceID : INSTANCE_ID; ///< Geometry instance ID.
    nointerpolation uint rawInstanceID : RAW_INSTANCE_ID;        ///< Raw geometry instance ID.
    nointerpolation uint materialID : MATERIAL_ID;               ///< Material ID.

    float4 posH : SV_POSITION;
};

// Default Vertex Shader
MyVSOut vsMain(VSIn vIn) {

    MyVSOut vOut;
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

    vOut.posL = float4(vIn.pos, 1.f);
    vOut.rawInstanceID = vIn.instanceID;

    return vOut;

}

VSOut CopyToVSOut(MyVSOut vsOut) {
    VSOut vOut;
    vOut.posH = vsOut.posH;
    vOut.posW = vsOut.posW;
    vOut.normalW = vsOut.normalW;
    vOut.tangentW = vsOut.tangentW;
    vOut.texC = vsOut.texC;
    vOut.instanceID = vsOut.instanceID;
    vOut.materialID = vsOut.materialID;
    vOut.prevPosH = vsOut.prevPosH;
    return vOut;
}

struct PSOut {
    float4 gNormWS : SV_TARGET0;
    float4 gDiffOpacity : SV_TARGET1;
    float4 gPosWS : SV_TARGET2;
    uint   gInstanceID : SV_TARGET3;
    float4 gPosL : SV_TARGET4;
};

PSOut psMain(MyVSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) : SV_TARGET {

    int2 ipos = int2(vsOut.posH.xy);

    VSOut rawVSOut = CopyToVSOut(vsOut);

    PSOut psOut = {};

    float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    VertexData v = prepareVertexData(rawVSOut, faceNormal);
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
    psOut.gInstanceID = vsOut.rawInstanceID;
    psOut.gPosL = vsOut.posL;

    return psOut;
}
