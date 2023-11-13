from falcor import *

def render_graph_ForwardExtrapolation_UE():
    g = RenderGraph("ForwardExtrapolationUE")

    loadRenderPassLibrary("DLSSPass.dll")
    loadRenderPassLibrary("ToneMapper.dll")
    loadRenderPassLibrary("ForwardExtrapolation.dll")
    loadRenderPassLibrary("UE_loader.dll")

    forwardExtrapolation = createPass("ForwardExtrapolation")
    g.addPass(forwardExtrapolation, "ForwardExtrapolation")


    DLSS = createPass("DLSSPass", {'enabled': True, 'profile': DLSSProfile.Balanced, 'motionVectorScale': DLSSMotionVectorScale.Relative, 'isHDR': True, 'sharpness': 0.0, 'exposure': 0.0})
    g.addPass(DLSS, "DLSS")
    ToneMapper = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapper, "ToneMapper")

    loadUE = createPass("UE_loader")
    g.addPass(loadUE, "loadUE")

    g.addEdge("loadUE.Color",                                          "ForwardExtrapolation.PreTonemapped_in")
    g.addEdge("loadUE.PosW",                                           "ForwardExtrapolation.PosW_in")
    g.addEdge("loadUE.NextPosW",                                       "ForwardExtrapolation.NextPosW_in")
    g.addEdge("loadUE.MotionVector",                                   "ForwardExtrapolation.MotionVector_in")
    g.addEdge("loadUE.LinearZ",                                        "ForwardExtrapolation.LinearZ_in")

    # g.addEdge("DLSS.output",                                            "ForwardExtrapolation.PreTonemapped_in")

    # g.addEdge("ForwardExtrapolation.PreTonemapped_out",                 "ToneMapper.src")
    g.addEdge("ForwardExtrapolation.MotionVector_out",                  "DLSS.mvec")
    g.addEdge("ForwardExtrapolation.LinearZ_out",                       "DLSS.depth")
    g.addEdge("ForwardExtrapolation.PreTonemapped_out",                 "DLSS.color")
    # g.addEdge("loadUE.MotionVector",                  "DLSS.mvec")
    # g.addEdge("loadUE.LinearZ",                       "DLSS.depth")
    # g.addEdge("loadUE.Color",                 "DLSS.color")
    g.addEdge("DLSS.output",                                            "ToneMapper.src")

    # Outputs
    g.markOutput("ToneMapper.dst")
    # g.markOutput("ForwardExtrapolation.PreTonemapped_out_woSplat")
    # g.markOutput("ForwardExtrapolation.Background_Color")

    return g

ForwardExtrapolationUE = render_graph_ForwardExtrapolation_UE()
try: m.addGraph(ForwardExtrapolationUE)
except NameError: None

m.resizeSwapChain(1920, 1080)
m.ui = True


