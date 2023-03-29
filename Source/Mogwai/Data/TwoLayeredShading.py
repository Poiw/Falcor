def render_graph_ForwardRenderer():
    loadRenderPassLibrary("CSM.dll")
    loadRenderPassLibrary("DepthPass.dll")
    loadRenderPassLibrary("ForwardLightingPass.dll")
    loadRenderPassLibrary("ToneMapper.dll")
    loadRenderPassLibrary("TwoLayeredGbuffers.dll")
    loadRenderPassLibrary("WarpShading.dll")
    loadRenderPassLibrary("MyGBuffer.dll")

    g = RenderGraph("TwoLayeredShading")

    gbuffer_raster = createPass("GBufferRaster", {'adjustShadingNormals': False})
    g.addPass(gbuffer_raster, "GBufferRaster")

    gen_twoLayerGbuffer = createPass("TwoLayeredGbuffers")
    g.addPass(gen_twoLayerGbuffer, "TwoLayeredGbuffers")

    warp_shading = createPass("WarpShading")
    g.addPass(warp_shading, "WarpShading")

    g.addPass(createPass("DepthPass"), "DepthPrePass")
    g.addPass(createPass("ForwardLightingPass"), "LightingPass")
    g.addPass(createPass("CSM"), "ShadowPass")
    g.addPass(createPass("ToneMapper", {'autoExposure': True}), "ToneMapping")
    g.addPass(createPass("ToneMapper", {'autoExposure': True}), "MyToneMapping")
    g.addPass(createPass("SkyBox"), "SkyBox")

    g.addEdge("DepthPrePass.depth", "SkyBox.depth")
    g.addEdge("SkyBox.target", "LightingPass.color")
    g.addEdge("DepthPrePass.depth", "ShadowPass.depth")
    g.addEdge("DepthPrePass.depth", "LightingPass.depth")
    # g.addEdge("ShadowPass.visibility", "LightingPass.visibilityBuffer")
    g.addEdge("LightingPass.color", "ToneMapping.src")

    g.addEdge("LightingPass.color", "TwoLayeredGbuffers.rPreTonemapped")

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

    g.addEdge("WarpShading.tl_FirstPreTonemap", "MyToneMapping.src")


    g.markOutput("ToneMapping.dst")
    g.markOutput("MyToneMapping.dst")

    return g

TwoLayeredShading = render_graph_ForwardRenderer()
try: m.addGraph(TwoLayeredShading)
except NameError: None
