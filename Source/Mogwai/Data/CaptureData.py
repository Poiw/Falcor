from falcor import *

def render_graph_DataCapture():
    g = RenderGraph("DataCapture")
    loadRenderPassLibrary("AccumulatePass.dll")
    loadRenderPassLibrary("GBuffer.dll")
    loadRenderPassLibrary("PathTracer.dll")
    loadRenderPassLibrary("ToneMapper.dll")
    loadRenderPassLibrary("DataCapture.dll")
    PathTracer = createPass("PathTracer", {'samplesPerPixel': 1})
    g.addPass(PathTracer, "PathTracer")
    VBufferRT = createPass("VBufferRT", {'samplePattern': SamplePattern.Stratified, 'sampleCount': 16, 'useAlphaTest': True})
    g.addPass(VBufferRT, "VBufferRT")
    GBufferRT = createPass("GBufferRT", {'samplePattern': SamplePattern.Halton, 'sampleCount': 16, 'useAlphaTest': True})
    g.addPass(GBufferRT, "GBufferRT")
    AccumulatePass = createPass("AccumulatePass", {'enabled': True, 'precisionMode': AccumulatePrecision.Single})
    g.addPass(AccumulatePass, "AccumulatePass")
    # AccumulatePassAlbedo = createPass("AccumulatePass", {'enabled': True, 'precisionMode': AccumulatePrecision.Single})
    # g.addPass(AccumulatePassAlbedo, "AccumulatePassAlbedo")
    # AccumulatePassNormal = createPass("AccumulatePass", {'enabled': True, 'precisionMode': AccumulatePrecision.Single})
    # g.addPass(AccumulatePassNormal, "AccumulatePassNormal")
    ToneMapper = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapper, "ToneMapper")
    DataCapture = createPass("DataCapture")
    g.addPass(DataCapture, "DataCapture")

    g.addEdge("VBufferRT.vbuffer", "PathTracer.vbuffer")
    g.addEdge("VBufferRT.viewW", "PathTracer.viewW")
    g.addEdge("VBufferRT.mvec", "PathTracer.mvec")
    g.addEdge("PathTracer.color", "AccumulatePass.input")
    # g.addEdge("PathTracer.albedo", "AccumulatePassAlbedo.input")
    # g.addEdge("PathTracer.normal", "AccumulatePassNormal.input")
    g.addEdge("AccumulatePass.output", "ToneMapper.src")
    g.addEdge("AccumulatePass.output", "DataCapture.color")
    g.addEdge("GBufferRT.diffuseOpacity", "DataCapture.albedo")
    g.addEdge("GBufferRT.normW", "DataCapture.normal")
    g.addEdge("GBufferRT.posW", "DataCapture.posW")
    g.addEdge("GBufferRT.viewW", "DataCapture.viewW")
    g.markOutput("ToneMapper.dst")
    g.markOutput("DataCapture.empty")
    return g

DataCapture = render_graph_DataCapture()
try: m.addGraph(DataCapture)
except NameError: None

m.resizeSwapChain(960, 540)
m.ui = True
