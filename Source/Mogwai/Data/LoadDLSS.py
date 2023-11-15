from falcor import *

def render_graph_TestDLSS():
    g = RenderGraph("TestDLSS")

    loadRenderPassLibrary("PreprocessDLSS.dll")
    loadRenderPassLibrary("DLSSPass.dll")

    PreprocessDLSS = createPass("PreprocessDLSS")
    g.addPass(PreprocessDLSS, "PreprocessDLSS")

    DLSS = createPass("DLSSPass", {'enabled': True, 'profile': DLSSProfile.Balanced, 'motionVectorScale': DLSSMotionVectorScale.Relative, 'isHDR': True, 'sharpness': 0.0, 'exposure': 0.0})
    g.addPass(DLSS, "DLSS")

    g.addEdge("PreprocessDLSS.mvec",            "DLSS.mvec")
    g.addEdge("PreprocessDLSS.depth",           "DLSS.depth")
    g.addEdge("PreprocessDLSS.color",           "DLSS.color")

    # Outputs
    # g.markOutput("PreprocessDLSS.depth")
    # g.markOutput("PreprocessDLSS.color")
    # g.markOutput("PreprocessDLSS.mvec")
    g.markOutput("DLSS.output")

    return g

TestDLSS = render_graph_TestDLSS()
try: m.addGraph(TestDLSS)
except NameError: None

