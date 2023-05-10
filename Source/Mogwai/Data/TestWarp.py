from falcor import *

def render_graph_TestWarp():
    g = RenderGraph("TestDLSS")

    loadRenderPassLibrary("PreprocessDLSS.dll")
    loadRenderPassLibrary("WarpTest.dll")

    PreprocessDLSS = createPass("PreprocessDLSS")
    g.addPass(PreprocessDLSS, "PreprocessDLSS")

    WarpTest = createPass("WarpTest")
    g.addPass(WarpTest, "WarpTest")



    g.addEdge("PreprocessDLSS.mvec",            "WarpTest.motionvector")
    g.addEdge("PreprocessDLSS.basecolor",           "WarpTest.basecolor")
    g.addEdge("PreprocessDLSS.normal",            "WarpTest.normal")
    g.addEdge("PreprocessDLSS.color",           "WarpTest.render")

    # Outputs
    g.markOutput("PreprocessDLSS.color")
    g.markOutput("WarpTest.warped")

    return g

TestWarp = render_graph_TestWarp()
try: m.addGraph(TestWarp)
except NameError: None

