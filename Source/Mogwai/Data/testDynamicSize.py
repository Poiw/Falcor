# Graphs
from falcor import *

def render_graph_ForwardExtrapolationUE():
    g = RenderGraph('ForwardExtrapolationUE')
    loadRenderPassLibrary('DLSSPass.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('ForwardExtrapolation.dll')
    loadRenderPassLibrary('UE_loader.dll')
    ForwardExtrapolation = createPass('ForwardExtrapolation')
    g.addPass(ForwardExtrapolation, 'ForwardExtrapolation')
    DLSS = createPass('DLSSPass', {'enabled': True, 'outputSize': IOSize.Default, 'profile': DLSSProfile.Balanced, 'motionVectorScale': DLSSMotionVectorScale.Relative, 'isHDR': True, 'sharpness': 0.0, 'exposure': 0.0})
    g.addPass(DLSS, 'DLSS')
    ToneMapper = createPass('ToneMapper', {'outputSize': IOSize.Default, 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': ToneMapOp.Aces, 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': ExposureMode.AperturePriority})
    g.addPass(ToneMapper, 'ToneMapper')
    loadUE = createPass('UE_loader')
    g.addPass(loadUE, 'loadUE')
    g.addEdge('loadUE.Color', 'ForwardExtrapolation.PreTonemapped_in')
    g.addEdge('loadUE.PosW', 'ForwardExtrapolation.PosW_in')
    g.addEdge('loadUE.NextPosW', 'ForwardExtrapolation.NextPosW_in')
    g.addEdge('loadUE.MotionVector', 'ForwardExtrapolation.MotionVector_in')
    g.addEdge('loadUE.LinearZ', 'ForwardExtrapolation.LinearZ_in')
    g.addEdge('ForwardExtrapolation.MotionVector_out', 'DLSS.mvec')
    g.addEdge('ForwardExtrapolation.LinearZ_out', 'DLSS.depth')
    g.addEdge('ForwardExtrapolation.PreTonemapped_out', 'DLSS.color')
    g.addEdge('DLSS.output', 'ToneMapper.src')
    g.markOutput('ToneMapper.dst')
    return g
m.addGraph(render_graph_ForwardExtrapolationUE())

# Scene
m.loadScene('Arcade/Arcade.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True, useGridVolumes=True)
m.scene.camera.position = float3(-0.875000,1.250000,0.875000)
m.scene.camera.target = float3(-0.875000,1.250000,-0.125000)
m.scene.camera.up = float3(0.000000,1.000000,0.000000)
m.scene.cameraSpeed = 1.0

# Window Configuration
m.resizeSwapChain(1920, 1080)
m.ui = True

# Clock Settings
m.clock.time = 0
m.clock.framerate = 0
# If framerate is not zero, you can use the frame property to set the start frame
# m.clock.frame = 0

# Frame Capture
m.frameCapture.outputDir = '.'
m.frameCapture.baseFilename = 'Mogwai'

