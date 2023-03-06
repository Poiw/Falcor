from falcor import *

def render_occupancy_map():
    g = RenderGraph("TwoLayerGbuffers")
    loadRenderPassLibrary("GBuffer.dll")
    loadRenderPassLibrary("TwoLayeredGbuffers.dll")


    gbuffer_raster = createPass("GBufferRaster", {'adjustShadingNormals': False})
    gen_twoLayerGbuffer = createPass("TwoLayeredGbuffers")

    g.addPass(gbuffer_raster, "GBufferRaster")
    g.addPass(gen_twoLayerGbuffer, "TwoLayeredGbuffers")


    g.addEdge("GBufferRaster.posW", "TwoLayeredGbuffers.gPosWS")
    g.addEdge("GBufferRaster.normW", "TwoLayeredGbuffers.gNormalWS")
    g.addEdge("GBufferRaster.tangentW", "TwoLayeredGbuffers.gTangentWS")
    g.addEdge("GBufferRaster.depth", "TwoLayeredGbuffers.gFirstDepth")
    g.addEdge("GBufferRaster.linearZ", "TwoLayeredGbuffers.gFirstLinearZ")
    g.markOutput('TwoLayeredGbuffers.gMyDepth')
    g.markOutput('TwoLayeredGbuffers.gDebug')
    g.markOutput('TwoLayeredGbuffers.gDebug2')
    return g


om = render_occupancy_map()
try:
    m.addGraph(om)
except NameError:
    None

# Window Configuration
m.resizeSwapChain(1920, 1080)
m.ui = True

