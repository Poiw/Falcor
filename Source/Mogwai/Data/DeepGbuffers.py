from falcor import *

def render_deep_gbuffers():
    g = RenderGraph("DeepGbuffers")
    loadRenderPassLibrary("DeepGbuffers.dll")


    gen_DeepGbuffer = createPass("DeepGbuffers")

    g.addPass(gen_DeepGbuffer, "DeepGbuffers")


    # g.markOutput('TwoLayeredGbuffers.gMyDepth')
    g.markOutput('DeepGbuffers.Normal')
    g.markOutput('DeepGbuffers.Albedo')
    return g


dg = render_deep_gbuffers()
try:
    m.addGraph(dg)
except NameError:
    None

# Window Configuration
m.resizeSwapChain(1920, 1080)
m.ui = True

