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
#include "ForwardExtrapolation.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info ForwardExtrapolation::kInfo { "ForwardExtrapolation", "Forward warping without G-buffers for target frames" };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(ForwardExtrapolation::kInfo, ForwardExtrapolation::create);
}

namespace {
const std::string FE_forwardWarpDepthTestShaderFilePath =
    "RenderPasses/ForwardExtrapolation/forwardWarpDepthTest.slang";
const std::string FE_forwardWarpShaderFilePath =
    "RenderPasses/ForwardExtrapolation/forwardWarp.slang";
const std::string FE_splatShaderFilePath =
    "RenderPasses/ForwardExtrapolation/splat.slang";
}  // namespace


ForwardExtrapolation::ForwardExtrapolation() : RenderPass(kInfo)
{

    mpForwardWarpDepthTestPass = nullptr;
    mpForwardWarpPass = nullptr;
    mpSplatPass = nullptr;

}



ForwardExtrapolation::SharedPtr ForwardExtrapolation::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new ForwardExtrapolation());
    return pPass;
}

Dictionary ForwardExtrapolation::getScriptingDictionary()
{
    return Dictionary();
}

void ForwardExtrapolation::ClearVariables()
{

    mFrameCount = 0;
    mExtrapolationNum = 1;

    mpForwardMotionTex = nullptr;
    mpDepthTex = nullptr;
    mpRenderTex = nullptr;

    mpTempWarpTex = nullptr;
    mpTempDepthTex = nullptr;

    mMode = 0;

    mKernelSize = 3;

}

void ForwardExtrapolation::setComputeShaders()
{

    // Create Forward Warping Depth Test Shader
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_forwardWarpDepthTestShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpForwardWarpDepthTestPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpForwardWarpDepthTestPass->setVars(nullptr);  // Trigger vars creation
    }

    // Create Forward Warping Shader
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_forwardWarpShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpForwardWarpPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpForwardWarpPass->setVars(nullptr);  // Trigger vars creation
    }

    // Create Splat Shader
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_splatShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpSplatPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpSplatPass->setVars(nullptr);  // Trigger vars creation
    }
}

void ForwardExtrapolation::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{


    if (pScene) {
        mpScene = pScene;

        ClearVariables();

        setComputeShaders();

    }
}


RenderPassReflection ForwardExtrapolation::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    // Inputs
    reflector.addInput("ForwardMotion_in", "Forward Motion")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("Depth_in", "Depth")
        .format(ResourceFormat::D32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("PreTonemapped_in", "PreTonemapped Image")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    // Outputs
    reflector.addOutput("PreTonemapped_out", "Center Render")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    return reflector;
}

void ForwardExtrapolation::createNewTexture(Texture::SharedPtr &pTex,
                                        const Falcor::uint2 &curDim,
                                        enum Falcor::ResourceFormat dataFormat = ResourceFormat::RGBA32Float,
                                        Falcor::Resource::BindFlags bindFlags = Resource::BindFlags::AllColorViews)
{

    if (!pTex || pTex->getWidth() != curDim.x || pTex->getHeight() != curDim.y) {

        pTex = Texture::create2D(curDim.x, curDim.y, dataFormat,
                                    1U, 1, nullptr,
                                    bindFlags);

    }

}


void ForwardExtrapolation::renderedFrameProcess(RenderContext* pRenderContext, const RenderData& renderData)
{
    auto curDim = renderData.getDefaultTextureDims();

    pRenderContext->blit(renderData.getTexture("Pretonemapped_in")->getSRV(), renderData.getTexture("PreTonemapped_out")->getRTV());

    createNewTexture(mpRenderTex, curDim);
    createNewTexture(mpDepthTex, curDim, ResourceFormat::D32Float);
    createNewTexture(mpForwardMotionTex, curDim, ResourceFormat::RG32Float);

    pRenderContext->blit(renderData.getTexture("Pretonemapped_in")->getSRV(), mpRenderTex->getRTV());
    pRenderContext->blit(renderData.getTexture("Depth_in")->getSRV(), mpDepthTex->getRTV());
    pRenderContext->blit(renderData.getTexture("ForwardMotion_in")->getSRV(), mpForwardMotionTex->getRTV());

}


void ForwardExtrapolation::extrapolatedFrameProcess(RenderContext* pRenderContext, const RenderData& renderData)
{

    auto curDim = renderData.getDefaultTextureDims();

    createNewTexture(mpTempWarpTex, curDim, ResourceFormat::RGBA32Float);
    createNewTexture(mpTempDepthTex, curDim, ResourceFormat::R32Uint);

    // ########################## Forward Warping Depth Test ####################################
    {

        // Input
        {
            mpForwardWarpDepthTestPass["PerFrameCB"]["gFrameDim"] = curDim;

            auto depthTexSRV = mpDepthTex->getSRV();
            mpForwardWarpDepthTestPass["gDepthTex"].setSrv(depthTexSRV);

            auto forwardMotionTexSRV = mpForwardMotionTex->getSRV();
            mpForwardWarpDepthTestPass["gForwardMotionTex"].setSrv(forwardMotionTexSRV);
        }

        // Output
        {
            auto tempDepthTexUAV = mpTempDepthTex->getUAV();
            pRenderContext->clearUAV(tempDepthTexUAV.get(), uint4(-1));
            mpForwardWarpDepthTestPass["gTempDepthTex"].setUav(tempDepthTexUAV);
        }

        // Execute
        mpForwardWarpDepthTestPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(mpTempDepthTex.get());
        }

    }
    // ###################################################################################



    // ########################## Forward Warping ####################################
    {

        // Input
        {
            mpForwardWarpPass["PerFrameCB"]["gFrameDim"] = curDim;

            auto renderTexSRV = mpRenderTex->getSRV();
            mpForwardWarpPass["gRenderTex"].setSrv(renderTexSRV);

            auto depthTexSRV = mpDepthTex->getSRV();
            mpForwardWarpPass["gDepthTex"].setSrv(depthTexSRV);

            auto tempDepthTexSRV = mpTempDepthTex->getSRV();
            mpForwardWarpPass["gTempDepthTex"].setSrv(tempDepthTexSRV);

            auto forwardMotionTexSRV = mpForwardMotionTex->getSRV();
            mpForwardWarpPass["gForwardMotionTex"].setSrv(forwardMotionTexSRV);
        }

        // Output
        {
            auto tempWarpTexUAV = mpTempWarpTex->getUAV();
            pRenderContext->clearUAV(tempWarpTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpForwardWarpPass["gTempWarpTex"].setUav(tempWarpTexUAV);

        }

        // Execute
        mpForwardWarpPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(mpTempWarpTex.get());
        }

    }
    // ###################################################################################



    // ################################### Splat #########################################
    {
        // Input
        {
            mpSplatPass["PerFrameCB"]["gFrameDim"] = curDim;
            mpSplatPass["PerFrameCB"]["gKernelSize"] = mKernelSize;

            auto tempWarpTexSRV = mpTempWarpTex->getSRV();
            mpSplatPass["gTempWarpTex"].setSrv(tempWarpTexSRV);
        }

        // Output
        {
            auto targetRenderTexUAV = renderData.getTexture("PreTonemapped_out")->getUAV();
            mpSplatPass["targetRenderTex"].setUav(targetRenderTexUAV);
        }

        // Execute
        mpSplatPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(renderData.getTexture("PreTonemapped_out").get());
        }
    }
    // ###################################################################################

}


void ForwardExtrapolation::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (mpScene == nullptr) return;


    // Ground truth rendered frame
    if (mFrameCount % (mExtrapolationNum + 1) == 0) {

        renderedFrameProcess(pRenderContext, renderData);

    }
    // Extrapolated Frame
    else {

        // Do nothing, rendering for every frame
        if (mMode == 0) {

            pRenderContext->blit(renderData.getTexture("Pretonemapped_in")->getSRV(), renderData.getTexture("PreTonemapped_out")->getRTV());

        }
        // Extrapolate frame
        else {

            extrapolatedFrameProcess(pRenderContext, renderData);

        }

    }


    mFrameCount += 1;
    if (mFrameCount >= mExtrapolationNum + 1) mFrameCount = 0;

}

void ForwardExtrapolation::renderUI(Gui::Widgets& widget)
{
    Gui::DropdownList modeList;
    modeList.push_back(Gui::DropdownValue{0, "default"});
    modeList.push_back(Gui::DropdownValue{1, "extrapolation"});
    widget.dropdown("Mode", modeList, mMode);


    widget.var<uint32_t>("Extrapolation Num", mExtrapolationNum, 1u, 1u);

    widget.var<uint32_t>("Kernel Size", mKernelSize, 1u, 32u);

}
