/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "WarpShading.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info WarpShading::kInfo { "WarpShading", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(WarpShading::kInfo, WarpShading::create);
}

namespace {
const std::string shadingWarpingShaderFilePath =
    "RenderPasses/WarpShading/ShadingWarping.slang";
}  // namespace


WarpShading::SharedPtr WarpShading::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new WarpShading());
    return pPass;
}

void WarpShading::initVariables()
{
    mAlbedoSigma = 0.1f;
    mNormSigma = 0.1f;
    mPosWSigma = 0.1f;
    mCoordSigma = 0.005f;

    mFrameCount = 0;

    mDumpData = false;

    mCurTargetDim = uint2(960, 540);

    mSavingDir = "C:/Users/songyin/Desktop/TwoLayered/Test";

}

void WarpShading::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
     if (pScene) {
        mpScene = pScene;

        initVariables();

        // create a shading warping pass
        {
            Program::Desc desc;
            // desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(shadingWarpingShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpShadingWarpingPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpShadingWarpingPass->setVars(nullptr);  // Trigger vars creation
        }

    }
}

Dictionary WarpShading::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection WarpShading::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    reflector.addInput("tl_FrameCount", "Current Frame Count")
        .format(ResourceFormat::R32Uint)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D(1, 1);

    reflector.addInput("tl_CenterDiffOpacity", "Center Diff Opactiy")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("tl_CenterNormWS", "Center Norm WS")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("tl_CenterPosWS", "Center Pos WS")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("tl_CenterRender", "Center Render")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();


    reflector.addInput("tl_CurDiffOpacity", "Cur Diff Opactiy")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("tl_CurNormWS", "Cur Norm WS")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("tl_CurPosWS", "Cur Pos WS")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("tl_CurPrevCoord", "Prev Coord")
        .format(ResourceFormat::RG32Uint)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addOutput("tl_FirstPreTonemap", "Pre Tonemapped Rendering")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D(mCurTargetDim.x, mCurTargetDim.y);

    return reflector;
}

void WarpShading::DumpDataFunc(const RenderData &renderData, uint frameIdx, const std::string dirPath)
{
    renderData.getTexture("tl_FirstPreTonemap")->captureToFile(0, 0, dirPath + "/Render_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
}

void WarpShading::execute(RenderContext* pRenderContext, const RenderData& renderData)
{


    if (mpScene == nullptr) return;

    // Falcor::Logger::log(Falcor::Logger::Level::Info, "Shading: " + std::to_string(mFrameCount));

    auto curDim = renderData.getDefaultTextureDims();
    uint tarResoW = renderData.getTexture("tl_CurDiffOpacity")->getWidth();
    uint tarResoH = renderData.getTexture("tl_CurDiffOpacity")->getHeight();
    auto tarDim = uint2(tarResoW, tarResoH);

    // ---------------------------------- Warp Shading ---------------------------------------
    {
        // Input
        {
            mpShadingWarpingPass["PerFrameCB"]["gFrameDim"] = curDim;
            mpShadingWarpingPass["PerFrameCB"]["gTarFrameDim"] = tarDim;
            mpShadingWarpingPass["PerFrameCB"]["gAlbedoSigma"] = mAlbedoSigma;
            mpShadingWarpingPass["PerFrameCB"]["gNormSigma"] = mNormSigma;
            mpShadingWarpingPass["PerFrameCB"]["gPosWSigma"] = mPosWSigma;
            mpShadingWarpingPass["PerFrameCB"]["gCoordSigma"] = mCoordSigma;

            auto pFrameCountSRV = renderData["tl_FrameCount"]->getSRV();
            mpShadingWarpingPass["gFrameCount"].setSrv(pFrameCountSRV);


            auto pCurDiffOpacitySRV = renderData["tl_CurDiffOpacity"]->getSRV();
            mpShadingWarpingPass["gCurDiffOpacity"].setSrv(pCurDiffOpacitySRV);

            auto pCurPosWSSRV = renderData["tl_CurPosWS"]->getSRV();
            mpShadingWarpingPass["gCurPosWS"].setSrv(pCurPosWSSRV);

            auto pCurNormWSSRV = renderData["tl_CurNormWS"]->getSRV();
            mpShadingWarpingPass["gCurNormWS"].setSrv(pCurNormWSSRV);

            auto pCurPrevCoordSRV = renderData["tl_CurPrevCoord"]->getSRV();
            mpShadingWarpingPass["gCurPrevCoord"].setSrv(pCurPrevCoordSRV);

            auto pCenterDiffOpacitySRV = renderData["tl_CenterDiffOpacity"]->getSRV();
            mpShadingWarpingPass["gCenterDiffOpacity"].setSrv(pCenterDiffOpacitySRV);

            auto pCenterPosWSSRV = renderData["tl_CenterPosWS"]->getSRV();
            mpShadingWarpingPass["gCenterPosWS"].setSrv(pCenterPosWSSRV);

            auto pCenterNormWSSRV = renderData["tl_CenterNormWS"]->getSRV();
            mpShadingWarpingPass["gCenterNormWS"].setSrv(pCenterNormWSSRV);

            auto pCenterRenderSRV = renderData["tl_CenterRender"]->getSRV();
            mpShadingWarpingPass["gCenterRender"].setSrv(pCenterRenderSRV);
        }

        // Output
        {
            auto pRenderUAV = renderData["tl_FirstPreTonemap"]->getUAV();
            pRenderContext->clearUAV(pRenderUAV.get(), float4(-1.f));
            mpShadingWarpingPass["gRender"].setUav(pRenderUAV);
        }

        mpShadingWarpingPass->execute(pRenderContext, uint3(tarDim, 1));

        // Set Barriers
        {
            pRenderContext->uavBarrier(renderData["tl_FirstPreTonemap"].get());
        }

    }

    if (mDumpData)
    {
        DumpDataFunc(renderData, mFrameCount, mSavingDir);
    }

    mFrameCount += 1;
}

void WarpShading::renderUI(Gui::Widgets& widget)
{
    widget.var<float>("Albedo Sigma", mAlbedoSigma, 0);
    widget.var<float>("PosW Sigma", mPosWSigma, 0);
    widget.var<float>("Normal Sigma", mNormSigma, 0);
    widget.var<float>("Coord Sigma", mCoordSigma, 0);

    widget.checkbox("Dump Data", mDumpData);
    widget.textbox("Saving Dir", mSavingDir);


    if (widget.var<uint>("Target Resolution X", mCurTargetDim.x, 32)) requestRecompile();
    if (widget.var<uint>("Target Resolution Y", mCurTargetDim.y, 32)) requestRecompile();
}
