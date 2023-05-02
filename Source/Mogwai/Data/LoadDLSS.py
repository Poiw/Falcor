from falcor import *

def render_graph_TestDLSS():
    g = RenderGraph("TestDLSS")

    loadRenderPassLibrary("PreprocessDLSS.dll")

    PreprocessDLSS = createPass("PreprocessDLSS")
    g.addPass(PreprocessDLSS, "PreprocessDLSS")

    # Outputs
    g.markOutput("PreprocessDLSS.depth")
    g.markOutput("PreprocessDLSS.color")
    g.markOutput("PreprocessDLSS.mvec")

    return g

TestDLSS = render_graph_TestDLSS()
try: m.addGraph(TestDLSS)
except NameError: None

