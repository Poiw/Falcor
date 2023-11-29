from falcor import *

def render_graph_TestWarp():
    g = RenderGraph("TestWarp")

    loadRenderPassLibrary("WarpTest.dll")

    loadRenderPassLibrary("AccumulatePass.dll")
    loadRenderPassLibrary("MyGBuffer.dll")
    loadRenderPassLibrary("ModulateIllumination.dll")
    loadRenderPassLibrary("NRDPass.dll")
    loadRenderPassLibrary("PathTracer.dll")
    loadRenderPassLibrary("ToneMapper.dll")

    WarpTest = createPass("WarpTest")
    g.addPass(WarpTest, "WarpTest")


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
    ToneMapperOurs = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapperOurs, "ToneMapperOurs")



    g.addEdge("GBufferRT.vbuffer",                                      "PathTracer.vbuffer")
    g.addEdge("GBufferRT.viewW",                                        "PathTracer.viewW")

    # Reference path graph
    g.addEdge("PathTracer.color",                                       "AccumulatePass.input")

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

    g.addEdge("GBufferRT.mvec",                                         "WarpTest.motionvector")
    g.addEdge("GBufferRT.diffuseOpacity",                               "WarpTest.basecolor")
    g.addEdge("GBufferRT.normW",                                        "WarpTest.normal")
    g.addEdge("ModulateIllumination.output",                            "WarpTest.render")

    g.addEdge("WarpTest.warped",                                        "ToneMapperOurs.src")

    # Outputs
    g.markOutput("ModulateIllumination.output")
    g.markOutput("WarpTest.warped")
    g.markOutput("ToneMapperOurs.dst")

    return g

TestWarp = render_graph_TestWarp()
try: m.addGraph(TestWarp)
except NameError: None


