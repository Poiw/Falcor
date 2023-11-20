import Scene.Raster;
import GBufferHelpers;

#ifndef INTERPOLATION_MODE
#define INTERPOLATION_MODE linear
#endif

Texture2DArray<float> gPrevLinearZBuffer;

cbuffer PerFrameCB {
    float gThreshold;
    uint gCurLevel;
};

struct MyVSOut
{
    INTERPOLATION_MODE float3 normalW : NORMAL;    ///< Shading normal in world space (not normalized!).
    INTERPOLATION_MODE float4 tangentW : TANGENT;  ///< Shading tangent in world space (not normalized!).
    INTERPOLATION_MODE float2 texC : TEXCRD;       ///< Texture coordinate.
    INTERPOLATION_MODE float3 posW : POSW;         ///< Position in world space.
    INTERPOLATION_MODE float4 prevPosH : PREVPOSH; ///< Position in clip space for the previous frame.
    INTERPOLATION_MODE float4 posL : POSL;         ///< Position in original model space.
    INTERPOLATION_MODE float3 nextPosW : NEXTPOSW;  ///< Next posW

    // Per-triangle data
    nointerpolation GeometryInstanceID instanceID : INSTANCE_ID; ///< Geometry instance ID.
    nointerpolation uint rawInstanceID : RAW_INSTANCE_ID;       ///< Raw geometry instance ID.
    nointerpolation uint materialID : MATERIAL_ID;               ///< Material ID.

    float4 posH : SV_POSITION;
};

// Default Vertex Shader
MyVSOut vsMain(VSIn vIn) {
    // VSOut vOut;
    // const GeometryInstanceID instanceID = { vIn.instanceID };

    // float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    // float3 posW = mul(worldMat, float4(vIn.pos, 1.f)).xyz;
    // vOut.posW = posW;
    // vOut.posH = mul(gViewProjMat, float4(posW, 1.f));
    // return vOut;

    // return defaultVS(vIn);

    MyVSOut vOut;
    const GeometryInstanceID instanceID = { vIn.instanceID };

    float4x4 worldMat = gScene.getWorldMatrix(instanceID);
    float3 posW = mul(worldMat, float4(vIn.pos, 1.f)).xyz;
    vOut.posW = posW;
    vOut.posH = mul(gScene.camera.getViewProj(), float4(posW, 1.f));

    vOut.instanceID = instanceID;
    vOut.materialID = gScene.getMaterialID(instanceID);

    vOut.texC = vIn.texC;
    vOut.normalW = mul(gScene.getInverseTransposeWorldMatrix(instanceID), vIn.unpack().normal);
    float4 tangent = vIn.unpack().tangent;
    vOut.tangentW = float4(mul((float3x3)gScene.getWorldMatrix(instanceID), tangent.xyz), tangent.w);

    // Compute the vertex position in the previous frame.
    float3 prevPos = vIn.pos;
    GeometryInstanceData instance = gScene.getGeometryInstance(instanceID);
    if (instance.isDynamic())
    {
        uint prevVertexIndex = gScene.meshes[instance.geometryIndex].prevVbOffset + vIn.vertexID;
        prevPos = gScene.prevVertices[prevVertexIndex].position;
    }
    float3 prevPosW = mul(gScene.getPrevWorldMatrix(instanceID), float4(prevPos, 1.f)).xyz;
    vOut.prevPosH = mul(gScene.camera.data.prevViewProjMatNoJitter, float4(prevPosW, 1.f));

    vOut.posL = float4(vIn.pos, 1.f);
    vOut.rawInstanceID = vIn.instanceID;

    float3 nextPosW = 2 * posW - prevPosW;
    vOut.nextPosW = nextPosW;

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

#define is_valid(name) (is_valid_##name != 0)

struct PSOut {
    float4 gAlbedo : SV_TARGET0;
    float4 gNormal : SV_TARGET1;
    float4 gNextPosW : SV_TARGET2;
    float gLinearZ : SV_TARGET3;
};

PSOut psMain(MyVSOut vsOut, uint triangleIndex : SV_PrimitiveID, float3 barycentrics : SV_Barycentrics) : SV_TARGET {

    int2 ipos = int2(vsOut.posH.xy);

    VSOut rawVSOut = CopyToVSOut(vsOut);

    float4x4 viewProjMat = gScene.camera.getViewProj();

    // Not a valid second layer gbuffer
    float linearZ = mul(viewProjMat, float4(vsOut.posW, 1.f)).w;

    float maxPrevLinearZ = -65536;
    for (int i = 0; i < gCurLevel; i++) {
        maxPrevLinearZ = max(maxPrevLinearZ, gPrevLinearZBuffer[uint3(ipos, i)]);
    }


    maxPrevLinearZ = maxPrevLinearZ + gThreshold;

    if (linearZ <= maxPrevLinearZ) discard;

    PSOut psOut = {};

    float3 faceNormal = gScene.getFaceNormalW(vsOut.instanceID, triangleIndex);
    VertexData v = prepareVertexData(rawVSOut, faceNormal);
    let lod = ImplicitLodTextureSampler();

    if (gScene.materials.alphaTest(v, vsOut.materialID, lod)) discard;

    const float3 viewDir = normalize(gScene.camera.getPosition() - v.posW);
    ShadingData sd = gScene.materials.prepareShadingData(v, vsOut.materialID, viewDir, lod);

    // Create BSDF instance.
    let bsdf = gScene.materials.getBSDF(sd, lod);

    const GBufferData gbuf = prepareGBufferData(sd, bsdf);

    psOut.gAlbedo = gbuf.diffuseOpacity;
    psOut.gNormal = gbuf.normW;
    psOut.gNextPosW = float4(vsOut.nextPosW, 1);
    psOut.gLinearZ = linearZ;


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
