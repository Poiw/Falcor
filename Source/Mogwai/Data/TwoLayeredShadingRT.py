from falcor import *

def render_graph_TwoLayeredShadingRT():
    g = RenderGraph("TwoLayeredShadingRT")

    loadRenderPassLibrary("AccumulatePass.dll")
    loadRenderPassLibrary("DLSSPass.dll")
    loadRenderPassLibrary("MyGBuffer.dll")
    loadRenderPassLibrary("ModulateIllumination.dll")
    loadRenderPassLibrary("NRDPass.dll")
    loadRenderPassLibrary("PathTracer.dll")
    loadRenderPassLibrary("ToneMapper.dll")
    loadRenderPassLibrary("TwoLayeredGbuffers.dll")
    loadRenderPassLibrary("WarpShading.dll")

    gbuffer_raster = createPass("GBufferRaster", {'adjustShadingNormals': False})
    g.addPass(gbuffer_raster, "GBufferRaster")
    gen_twoLayerGbuffer = createPass("TwoLayeredGbuffers")
    g.addPass(gen_twoLayerGbuffer, "TwoLayeredGbuffers")

    warp_shading = createPass("WarpShading")
    g.addPass(warp_shading, "WarpShading")

    GBufferRT = createPass("GBufferRT", {'samplePattern': SamplePattern.Halton, 'sampleCount': 32, 'useAlphaTest': True})
    g.addPass(GBufferRT, "GBufferRT")
    PathTracer = createPass("PathTracer", {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'useRussianRoulette': True})
    g.addPass(PathTracer, "PathTracer")

    # Reference path passes
    AccumulatePass = createPass("AccumulatePass", {'enabled': True, 'precisionMode': AccumulatePrecision.Single})
    g.addPass(AccumulatePass, "AccumulatePass")
    ToneMapperReference = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapperReference, "ToneMapperReference")

    # NRD path passes
    NRDDiffuseSpecular = createPass("NRD", {'maxIntensity': 250.0})
    g.addPass(NRDDiffuseSpecular, "NRDDiffuseSpecular")
    NRDDeltaReflection = createPass("NRD", {'method': NRDMethod.RelaxDiffuse, 'maxIntensity': 250.0, 'worldSpaceMotion': False,
                                            'enableReprojectionTestSkippingWithoutMotion': True, 'spatialVarianceEstimationHistoryThreshold': 1})
    g.addPass(NRDDeltaReflection, "NRDDeltaReflection")
    NRDDeltaTransmission = createPass("NRD", {'method': NRDMethod.RelaxDiffuse, 'maxIntensity': 250.0, 'worldSpaceMotion': False,
                                              'enableReprojectionTestSkippingWithoutMotion': True})
    g.addPass(NRDDeltaTransmission, "NRDDeltaTransmission")
    NRDReflectionMotionVectors = createPass("NRD", {'method': NRDMethod.SpecularReflectionMv, 'worldSpaceMotion': False})
    g.addPass(NRDReflectionMotionVectors, "NRDReflectionMotionVectors")
    NRDTransmissionMotionVectors = createPass("NRD", {'method': NRDMethod.SpecularDeltaMv, 'worldSpaceMotion': False})
    g.addPass(NRDTransmissionMotionVectors, "NRDTransmissionMotionVectors")
    ModulateIllumination = createPass("ModulateIllumination", {'useResidualRadiance': False})
    g.addPass(ModulateIllumination, "ModulateIllumination")
    DLSS = createPass("DLSSPass", {'enabled': True, 'profile': DLSSProfile.Balanced, 'motionVectorScale': DLSSMotionVectorScale.Relative, 'isHDR': True, 'sharpness': 0.0, 'exposure': 0.0})
    g.addPass(DLSS, "DLSS")
    ToneMapperNRD = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapperNRD, "ToneMapperNRD")
    ToneMapperTwoLayered = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapperTwoLayered, "ToneMapperTwoLayered")

    g.addEdge("GBufferRT.vbuffer",                                      "PathTracer.vbuffer")
    g.addEdge("GBufferRT.viewW",                                        "PathTracer.viewW")

    # Reference path graph
    g.addEdge("PathTracer.color",                                       "AccumulatePass.input")
    g.addEdge("AccumulatePass.output",                                  "ToneMapperReference.src")

    # NRD path graph
    g.addEdge("PathTracer.nrdDiffuseRadianceHitDist",                   "NRDDiffuseSpecular.diffuseRadianceHitDist")
    g.addEdge("PathTracer.nrdSpecularRadianceHitDist",                  "NRDDiffuseSpecular.specularRadianceHitDist")
    g.addEdge("GBufferRT.mvecW",                                        "NRDDiffuseSpecular.mvec")
    g.addEdge("GBufferRT.normWRoughnessMaterialID",                     "NRDDiffuseSpecular.normWRoughnessMaterialID")
    g.addEdge("GBufferRT.linearZ",                                      "NRDDiffuseSpecular.viewZ")

    g.addEdge("PathTracer.nrdDeltaReflectionHitDist",                   "NRDReflectionMotionVectors.specularHitDist")
    g.addEdge("GBufferRT.linearZ",                                      "NRDReflectionMotionVectors.viewZ")
    g.addEdge("GBufferRT.normWRoughnessMaterialID",                     "NRDReflectionMotionVectors.normWRoughnessMaterialID")
    g.addEdge("GBufferRT.mvec",                                         "NRDReflectionMotionVectors.mvec")

    g.addEdge("PathTracer.nrdDeltaReflectionRadianceHitDist",           "NRDDeltaReflection.diffuseRadianceHitDist")
    g.addEdge("NRDReflectionMotionVectors.reflectionMvec",              "NRDDeltaReflection.mvec")
    g.addEdge("PathTracer.nrdDeltaReflectionNormWRoughMaterialID",      "NRDDeltaReflection.normWRoughnessMaterialID")
    g.addEdge("PathTracer.nrdDeltaReflectionPathLength",                "NRDDeltaReflection.viewZ")

    g.addEdge("GBufferRT.posW",                                         "NRDTransmissionMotionVectors.deltaPrimaryPosW")
    g.addEdge("PathTracer.nrdDeltaTransmissionPosW",                    "NRDTransmissionMotionVectors.deltaSecondaryPosW")
    g.addEdge("GBufferRT.mvec",                                         "NRDTransmissionMotionVectors.mvec")

    g.addEdge("PathTracer.nrdDeltaTransmissionRadianceHitDist",         "NRDDeltaTransmission.diffuseRadianceHitDist")
    g.addEdge("NRDTransmissionMotionVectors.deltaMvec",                 "NRDDeltaTransmission.mvec")
    g.addEdge("PathTracer.nrdDeltaTransmissionNormWRoughMaterialID",    "NRDDeltaTransmission.normWRoughnessMaterialID")
    g.addEdge("PathTracer.nrdDeltaTransmissionPathLength",              "NRDDeltaTransmission.viewZ")

    g.addEdge("PathTracer.nrdEmission",                                 "ModulateIllumination.emission")
    g.addEdge("PathTracer.nrdDiffuseReflectance",                       "ModulateIllumination.diffuseReflectance")
    g.addEdge("NRDDiffuseSpecular.filteredDiffuseRadianceHitDist",      "ModulateIllumination.diffuseRadiance")
    g.addEdge("PathTracer.nrdSpecularReflectance",                      "ModulateIllumination.specularReflectance")
    g.addEdge("NRDDiffuseSpecular.filteredSpecularRadianceHitDist",     "ModulateIllumination.specularRadiance")
    g.addEdge("PathTracer.nrdDeltaReflectionEmission",                  "ModulateIllumination.deltaReflectionEmission")
    g.addEdge("PathTracer.nrdDeltaReflectionReflectance",               "ModulateIllumination.deltaReflectionReflectance")
    g.addEdge("NRDDeltaReflection.filteredDiffuseRadianceHitDist",      "ModulateIllumination.deltaReflectionRadiance")
    g.addEdge("PathTracer.nrdDeltaTransmissionEmission",                "ModulateIllumination.deltaTransmissionEmission")
    g.addEdge("PathTracer.nrdDeltaTransmissionReflectance",             "ModulateIllumination.deltaTransmissionReflectance")
    g.addEdge("NRDDeltaTransmission.filteredDiffuseRadianceHitDist",    "ModulateIllumination.deltaTransmissionRadiance")
    g.addEdge("PathTracer.nrdResidualRadianceHitDist",                  "ModulateIllumination.residualRadiance")

    g.addEdge("GBufferRT.mvec",                                         "DLSS.mvec")
    g.addEdge("GBufferRT.linearZ",                                      "DLSS.depth")
    g.addEdge("ModulateIllumination.output",                            "DLSS.color")

    g.addEdge("DLSS.output",                                            "ToneMapperNRD.src")

    g.addEdge("DLSS.output",                                "TwoLayeredGbuffers.rPreTonemapped")

    g.addEdge("GBufferRaster.posW", "TwoLayeredGbuffers.gPosWS")
    g.addEdge("GBufferRaster.normW", "TwoLayeredGbuffers.gNormalWS")
    g.addEdge("GBufferRaster.diffuseOpacity", "TwoLayeredGbuffers.gDiffOpacity")
    g.addEdge("GBufferRaster.linearZ", "TwoLayeredGbuffers.gLinearZ")
    g.addEdge("GBufferRaster.depth", "TwoLayeredGbuffers.gDepth")
    g.addEdge("GBufferRaster.rawInstanceID", "TwoLayeredGbuffers.gRawInstanceID")
    g.addEdge("GBufferRaster.positionLocal", "TwoLayeredGbuffers.gPosL")

    g.addEdge("TwoLayeredGbuffers.tl_FrameCount", "WarpShading.tl_FrameCount")
    g.addEdge("TwoLayeredGbuffers.tl_FirstDiffOpacity", "WarpShading.tl_CurDiffOpacity")
    g.addEdge("TwoLayeredGbuffers.tl_FirstNormWS", "WarpShading.tl_CurNormWS")
    g.addEdge("TwoLayeredGbuffers.tl_FirstPosWS", "WarpShading.tl_CurPosWS")
    g.addEdge("TwoLayeredGbuffers.tl_FirstPrevCoord", "WarpShading.tl_CurPrevCoord")

    g.addEdge("TwoLayeredGbuffers.tl_CenterDiffOpacity", "WarpShading.tl_CenterDiffOpacity")
    g.addEdge("TwoLayeredGbuffers.tl_CenterNormWS", "WarpShading.tl_CenterNormWS")
    g.addEdge("TwoLayeredGbuffers.tl_CenterPosWS", "WarpShading.tl_CenterPosWS")
    g.addEdge("TwoLayeredGbuffers.tl_CenterRender", "WarpShading.tl_CenterRender")

    g.addEdge("WarpShading.tl_FirstPreTonemap", "ToneMapperTwoLayered.src")

    # Outputs
    g.markOutput('TwoLayeredGbuffers.tl_Debug')
    g.markOutput('TwoLayeredGbuffers.tl_Mask')
    g.markOutput('TwoLayeredGbuffers.tl_FirstNormWS')
    g.markOutput('TwoLayeredGbuffers.tl_FirstDiffOpacity')
    g.markOutput('TwoLayeredGbuffers.tl_FirstPosWS')
    g.markOutput('TwoLayeredGbuffers.tl_FirstPrevCoord')
    g.markOutput('TwoLayeredGbuffers.tl_FirstPreTonemap')
    g.markOutput('TwoLayeredGbuffers.tl_SecondNormWS')
    g.markOutput('TwoLayeredGbuffers.tl_SecondDiffOpacity')
    g.markOutput('TwoLayeredGbuffers.tl_SecondPosWS')
    g.markOutput('TwoLayeredGbuffers.tl_SecondPrevCoord')
    g.markOutput("ToneMapperNRD.dst")
    g.markOutput("ToneMapperTwoLayered.dst")
    g.markOutput("ToneMapperReference.dst")

    return g

TwoLayeredShading = render_graph_TwoLayeredShadingRT()
try: m.addGraph(TwoLayeredShading)
except NameError: None

m.resizeSwapChain(1920, 1080)
m.ui = True


