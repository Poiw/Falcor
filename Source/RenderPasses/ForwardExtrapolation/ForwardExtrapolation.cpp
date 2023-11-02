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
const std::string FE_regularFramePreProcessShaderFilePath =
    "RenderPasses/ForwardExtrapolation/regularFramePreProcess.slang";
const std::string FE_backgroundCollectionDepthTestShaderFilePath =
    "RenderPasses/ForwardExtrapolation/backgroundCollectionDepthTest.slang";
const std::string FE_backgroundCollectionShaderFilePath =
    "RenderPasses/ForwardExtrapolation/backgroundCollection.slang";
const std::string FE_backgroundWarpShaderFilePath =
    "RenderPasses/ForwardExtrapolation/backgroundWarp.slang";
const std::string FE_backgroundWarpDepthTestShaderFilePath =
    "RenderPasses/ForwardExtrapolation/backgroundWarpDepthTest.slang";
}  // namespace


ForwardExtrapolation::ForwardExtrapolation() : RenderPass(kInfo)
{

    mpForwardWarpDepthTestPass = nullptr;
    mpForwardWarpPass = nullptr;
    mpSplatPass = nullptr;
    mpRegularFramePreProcessPass = nullptr;

    mpBackgroundCollectionDepthTestPass = nullptr;
    mpBackgroundCollectionPass = nullptr;

    mpBackgroundWarpDepthTestPass = nullptr;
    mpBackgroundWarpPass = nullptr;

    mDepthScale = 256;
    mBackgroundDepthScale = 256;

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

    mFrameCount = -1;
    mExtrapolationNum = 1;

    mpForwardMotionTex = nullptr;
    mpDepthTex = nullptr;
    mpRenderTex = nullptr;
    mpNextPosWTex = nullptr;

    mpTempWarpTex = nullptr;
    mpTempDepthTex = nullptr;
    mpTempOutputTex = nullptr;;
    mpTempOutputDepthTex = nullptr;
    mpTempOutputMVTex = nullptr;
    mpTempOutputPosWTex = nullptr;

    mpPrevMotionVectorTex = nullptr;
    mpTempMotionVectorTex = nullptr;

    mpBackgroundColorTex = nullptr;
    mpBackgroundPosWTex = nullptr;

    mpBackgroundWarpDepthTestPass = nullptr;
    mpBackgroundWarpPass = nullptr;

    mMode = 0;

    mKernelSize = 3;
    mSplatSigma = 1.;
    gSplatDistSigma = 8.;
    gSplatStrideNum = 3;

    mDumpData = false;
    mDumpDirPath = "";

    mIsNewBackground = true;



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

    // Preprocess output of regular frames
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_regularFramePreProcessShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpRegularFramePreProcessPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpRegularFramePreProcessPass->setVars(nullptr);  // Trigger vars creation
    }


    // Background collection depth test
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_backgroundCollectionDepthTestShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpBackgroundCollectionDepthTestPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpBackgroundCollectionDepthTestPass->setVars(nullptr);  // Trigger vars creation
    }

    // Background collection
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_backgroundCollectionShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpBackgroundCollectionPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpBackgroundCollectionPass->setVars(nullptr);  // Trigger vars creation
    }


    // Background warp depth test
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_backgroundWarpDepthTestShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpBackgroundWarpDepthTestPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpBackgroundWarpDepthTestPass->setVars(nullptr);  // Trigger vars creation
    }

    // Background warp
    {
        Program::Desc desc;
        desc.addShaderLibrary(FE_backgroundWarpShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpBackgroundWarpPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpBackgroundWarpPass->setVars(nullptr);  // Trigger vars creation
    }
}

void ForwardExtrapolation::DumpDataFunc(const RenderData &renderData, uint frameIdx, const std::string dirPath)
{
    renderData.getTexture("PreTonemapped_out")->captureToFile(0, 0, dirPath + "/Render." + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("PreTonemapped_in")->captureToFile(0, 0, dirPath + "/GT." + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("MotionVector_out")->captureToFile(0, 0, dirPath + "/MotionVector." + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("LinearZ_out")->captureToFile(0, 0, dirPath + "/LinearZ." + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("PreTonemapped_out_woSplat")->captureToFile(0, 0, dirPath + "/Render_woSplat." + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
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
    reflector.addInput("PosW_in", "Current posW")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("NextPosW_in", "Next PosW")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("PreTonemapped_in", "PreTonemapped Image")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("MotionVector_in", "Motion Vector")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("LinearZ_in", "LinearZ")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    // Outputs
    reflector.addOutput("PreTonemapped_out", "Center Render")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    reflector.addOutput("PreTonemapped_out_woSplat", "Center Render")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    reflector.addOutput("MotionVector_out", "Motion Vector")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    reflector.addOutput("LinearZ_out", "LinearZ")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();

    reflector.addOutput("Background_Color", "Background Color")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
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

    pRenderContext->blit(renderData.getTexture("PreTonemapped_in")->getSRV(), renderData.getTexture("PreTonemapped_out")->getRTV());
    pRenderContext->blit(renderData.getTexture("PreTonemapped_in")->getSRV(), renderData.getTexture("PreTonemapped_out_woSplat")->getRTV());

    createNewTexture(mpRenderTex, curDim);
    createNewTexture(mpNextPosWTex, curDim);
    createNewTexture(mpTempOutputDepthTex, curDim, ResourceFormat::R32Float);
    createNewTexture(mpTempOutputMVTex, curDim, ResourceFormat::RG32Float);

    pRenderContext->blit(renderData.getTexture("PreTonemapped_in")->getSRV(), mpRenderTex->getRTV());
    pRenderContext->blit(renderData.getTexture("NextPosW_in")->getSRV(), mpNextPosWTex->getRTV());


    // ########################### Motion Vector #####################################################
    if (mMode && mpPrevMotionVectorTex && mpForwardMotionTex) {

        // ########################## Calculate Motion Vector ####################################
        {

            // Input
            {
                mpRegularFramePreProcessPass["PerFrameCB"]["gFrameDim"] = curDim;

                auto mpCurMotionVectorSRV = renderData.getTexture("MotionVector_in")->getSRV();
                mpRegularFramePreProcessPass["gCurMotionVectorTex"].setSrv(mpCurMotionVectorSRV);

                auto mpPrevMotionVectorSRV = mpPrevMotionVectorTex->getSRV();
                mpRegularFramePreProcessPass["gPrevMotionVectorTex"].setSrv(mpPrevMotionVectorSRV);

                auto mpForwardMotionSRV = mpForwardMotionTex->getSRV();
                mpRegularFramePreProcessPass["gForwardMotionTex"].setSrv(mpForwardMotionSRV);

                auto mpCurLinearZSRV = renderData.getTexture("LinearZ_in")->getSRV();
                mpRegularFramePreProcessPass["gCurLinearZTex"].setSrv(mpCurLinearZSRV);

            }

            // Output
            {
                auto motionVectorOutUav = mpTempOutputMVTex->getUAV();
                pRenderContext->clearUAV(motionVectorOutUav.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
                mpRegularFramePreProcessPass["gMotionVectorOut"].setUav(motionVectorOutUav);

                auto LinearZOutUav = mpTempOutputDepthTex->getUAV();
                pRenderContext->clearUAV(LinearZOutUav.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
                mpRegularFramePreProcessPass["gLinearZOut"].setUav(LinearZOutUav);
            }

            // Execute
            mpRegularFramePreProcessPass->execute(pRenderContext, curDim.x, curDim.y);

            // Barrier
            {
                pRenderContext->uavBarrier(mpTempOutputMVTex.get());
                pRenderContext->uavBarrier(mpTempOutputDepthTex.get());
            }

            pRenderContext->blit(mpTempOutputMVTex->getSRV(), renderData.getTexture("MotionVector_out")->getRTV());
            pRenderContext->blit(mpTempOutputDepthTex->getSRV(), renderData.getTexture("LinearZ_out")->getRTV());

        }
        // ###################################################################################

    }

    else {
        pRenderContext->blit(renderData.getTexture("MotionVector_in")->getSRV(), renderData.getTexture("MotionVector_out")->getRTV());
        pRenderContext->blit(renderData.getTexture("LinearZ_in")->getSRV(), renderData.getTexture("LinearZ_out")->getRTV());
    }


}


void ForwardExtrapolation::collectBackground(RenderContext* pRenderContext, const RenderData& renderData)
{
    auto curDim = renderData.getDefaultTextureDims();
    createNewTexture(mpBackgroundColorTex, curDim, ResourceFormat::RGBA32Float);
    createNewTexture(mpBackgroundPosWTex, curDim, ResourceFormat::RGBA32Float);
    createNewTexture(mpTempOutputTex, curDim, ResourceFormat::RGBA32Float);
    createNewTexture(mpTempOutputPosWTex, curDim, ResourceFormat::RGBA32Float);


    if (mIsNewBackground) {

        pRenderContext->blit(renderData.getTexture("PreTonemapped_in")->getSRV(), mpBackgroundColorTex->getRTV());
        pRenderContext->blit(renderData.getTexture("PosW_in")->getSRV(), mpBackgroundPosWTex->getRTV());

        mIsNewBackground = false;
    }
    else {


        // ################################# Background collection depth test #####################################


        // Input
        {
            mpBackgroundCollectionDepthTestPass["PerFrameCB"]["gFrameDim"] = curDim;
            mpBackgroundCollectionDepthTestPass["PerFrameCB"]["curViewProjMat"] = mpScene->getCamera()->getViewProjMatrix();
            mpBackgroundCollectionDepthTestPass["PerFrameCB"]["mDepthScale"] = mBackgroundDepthScale;

            auto backgroundPosWSRV = mpBackgroundPosWTex->getSRV();
            mpBackgroundCollectionDepthTestPass["gBackgroundPosWTex"].setSrv(backgroundPosWSRV);

            auto curPosWSRV = renderData.getTexture("PosW_in")->getSRV();
            mpBackgroundCollectionDepthTestPass["gCurPosWTex"].setSrv(curPosWSRV);

        }

        // Output
        {
            auto tempDepthTexUAV = mpTempDepthTex->getUAV();
            pRenderContext->clearUAV(tempDepthTexUAV.get(), uint4(0));
            mpBackgroundCollectionDepthTestPass["gTempDepthTex"].setUav(tempDepthTexUAV);
        }

        // Execute
        mpBackgroundCollectionDepthTestPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(mpTempDepthTex.get());
        }


        // ########################################################################################################



        // ################################# Background collection #####################################


        // Input
        {
            mpBackgroundCollectionPass["PerFrameCB"]["gFrameDim"] = curDim;
            mpBackgroundCollectionPass["PerFrameCB"]["curViewProjMat"] = mpScene->getCamera()->getViewProjMatrix();
            mpBackgroundCollectionPass["PerFrameCB"]["mDepthScale"] = mBackgroundDepthScale;

            auto backgroundPosWSRV = mpBackgroundPosWTex->getSRV();
            mpBackgroundCollectionPass["gBackgroundPosWTex"].setSrv(backgroundPosWSRV);

            auto curPosWSRV = renderData.getTexture("PosW_in")->getSRV();
            mpBackgroundCollectionPass["gCurPosWTex"].setSrv(curPosWSRV);

            auto backgroundColorSRV = mpBackgroundColorTex->getSRV();
            mpBackgroundCollectionPass["gBackgroundColorTex"].setSrv(backgroundColorSRV);

            auto curColorSRV = renderData.getTexture("PreTonemapped_in")->getSRV();
            mpBackgroundCollectionPass["gCurColorTex"].setSrv(curColorSRV);

            auto testedDepthSRV = mpTempDepthTex->getSRV();
            mpBackgroundCollectionPass["gTestedDepthTex"].setSrv(testedDepthSRV);

        }

        // Output
        {
            auto tempOutputTexUAV = mpTempOutputTex->getUAV();
            pRenderContext->clearUAV(tempOutputTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpBackgroundCollectionPass["gTempOutputTex"].setUav(tempOutputTexUAV);

            auto tempOutputPosWTexUAV = mpTempOutputPosWTex->getUAV();
            pRenderContext->clearUAV(tempOutputPosWTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpBackgroundCollectionPass["gTempOutputPosWTex"].setUav(tempOutputPosWTexUAV);
        }

        // Execute
        mpBackgroundCollectionPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(mpTempOutputTex.get());
            pRenderContext->uavBarrier(mpTempOutputPosWTex.get());
        }

        pRenderContext->blit(mpTempOutputTex->getSRV(), mpBackgroundColorTex->getRTV());
        pRenderContext->blit(mpTempOutputPosWTex->getSRV(), mpBackgroundPosWTex->getRTV());

        pRenderContext->blit(mpTempOutputTex->getSRV(), renderData.getTexture("Background_Color")->getRTV());

        // ########################################################################################################


    }


}

void ForwardExtrapolation::warpBackground(RenderContext* pRenderContext, const RenderData& renderData, const Camera::SharedPtr& nextCamera)
{

    auto curDim = renderData.getDefaultTextureDims();
    createNewTexture(mpBackgroundWarpedTex, curDim, ResourceFormat::RGBA32Float);
    createNewTexture(mpBackgroundWarpedDepthTex, curDim, ResourceFormat::R32Uint);

    // ########################## Background Warping Depth Test ####################################

    // Input
    {
        mpBackgroundWarpDepthTestPass["PerFrameCB"]["gFrameDim"] = curDim;
        mpBackgroundWarpDepthTestPass["PerFrameCB"]["curViewProjMat"] = nextCamera->getViewProjMatrixNoJitter();
        mpBackgroundWarpDepthTestPass["PerFrameCB"]["mDepthScale"] = mDepthScale;

        auto backgroundPosWSRV = mpBackgroundPosWTex->getSRV();
        mpBackgroundWarpDepthTestPass["gBackgroundPosWTex"].setSrv(backgroundPosWSRV);
    }

    // Output
    {
        auto backgroundWarpedDepthTexUAV = mpBackgroundWarpedDepthTex->getUAV();
        pRenderContext->clearUAV(backgroundWarpedDepthTexUAV.get(), uint4(-1));
        mpBackgroundWarpDepthTestPass["gBackgroundWarpedDepthTex"].setUav(backgroundWarpedDepthTexUAV);
    }

    // Execute
    mpBackgroundWarpDepthTestPass->execute(pRenderContext, curDim.x, curDim.y);

    // Barrier
    {
        pRenderContext->uavBarrier(mpBackgroundWarpedDepthTex.get());
    }

    // ############################################################################################

    // ########################## Background Warping ####################################

    // Input
    {
        mpBackgroundWarpPass["PerFrameCB"]["gFrameDim"] = curDim;
        mpBackgroundWarpPass["PerFrameCB"]["curViewProjMat"] = nextCamera->getViewProjMatrixNoJitter();
        mpBackgroundWarpPass["PerFrameCB"]["mDepthScale"] = mDepthScale;

        auto backgroundPosWSRV = mpBackgroundPosWTex->getSRV();
        mpBackgroundWarpPass["gBackgroundPosWTex"].setSrv(backgroundPosWSRV);

        auto backgroundWarpedDepthTexSRV = mpBackgroundWarpedDepthTex->getSRV();
        mpBackgroundWarpPass["gBackgroundWarpedDepthTex"].setSrv(backgroundWarpedDepthTexSRV);

        auto backgroundColorSRV = mpBackgroundColorTex->getSRV();
        mpBackgroundWarpPass["gBackgroundColorTex"].setSrv(backgroundColorSRV);
    }

    // Output
    {
        auto backgroundWarpedTexUAV = mpBackgroundWarpedTex->getUAV();
        pRenderContext->clearUAV(backgroundWarpedTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
        mpBackgroundWarpPass["gBackgroundWarpedTex"].setUav(backgroundWarpedTexUAV);
    }

    // Execute
    mpBackgroundWarpPass->execute(pRenderContext, curDim.x, curDim.y);

    // Barrier
    {
        pRenderContext->uavBarrier(mpBackgroundWarpedTex.get());
    }

    // ##################################################################################

    pRenderContext->blit(mpBackgroundWarpedTex->getSRV(), renderData.getTexture("Background_Color")->getRTV());

}

void ForwardExtrapolation::extrapolatedFrameInit(RenderContext* pRenderContext, const RenderData& renderData, const Camera::SharedPtr& nextCamera)
{
    createNewTexture(mpPrevMotionVectorTex, renderData.getDefaultTextureDims(), ResourceFormat::RG32Float);

    pRenderContext->blit(renderData.getTexture("MotionVector_in")->getSRV(), mpPrevMotionVectorTex->getRTV());
}

void ForwardExtrapolation::extrapolatedFrameProcess(RenderContext* pRenderContext, const RenderData& renderData, const Camera::SharedPtr& nextCamera)
{

    auto curDim = renderData.getDefaultTextureDims();

    createNewTexture(mpForwardMotionTex, curDim, ResourceFormat::RG32Float);
    createNewTexture(mpDepthTex, curDim, ResourceFormat::R32Uint);
    createNewTexture(mpTempWarpTex, curDim, ResourceFormat::RGBA32Float);
    createNewTexture(mpTempDepthTex, curDim, ResourceFormat::R32Uint);
    createNewTexture(mpTempMotionVectorTex, curDim, ResourceFormat::RG32Float);
    createNewTexture(mpTempOutputTex, curDim, ResourceFormat::RGBA32Float);
    createNewTexture(mpTempOutputDepthTex, curDim, ResourceFormat::R32Float);
    createNewTexture(mpTempOutputMVTex, curDim, ResourceFormat::RG32Float);

    // ########################## Forward Warping Depth Test ####################################
    {

        // Input
        {
            mpForwardWarpDepthTestPass["PerFrameCB"]["gFrameDim"] = curDim;
            mpForwardWarpDepthTestPass["PerFrameCB"]["curViewProjMat"] = nextCamera->getViewProjMatrixNoJitter();
            mpForwardWarpDepthTestPass["PerFrameCB"]["mDepthScale"] = mDepthScale;

            auto nextPosWSRV = mpNextPosWTex->getSRV();
            mpForwardWarpDepthTestPass["gNextPosWTex"].setSrv(nextPosWSRV);

        }

        // Output
        {
            auto tempDepthTexUAV = mpTempDepthTex->getUAV();
            pRenderContext->clearUAV(tempDepthTexUAV.get(), uint4(-1));
            mpForwardWarpDepthTestPass["gTempDepthTex"].setUav(tempDepthTexUAV);

            auto forwardMotionTexUAV = mpForwardMotionTex->getUAV();
            pRenderContext->clearUAV(forwardMotionTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpForwardWarpDepthTestPass["gForwardMotionTex"].setUav(forwardMotionTexUAV);

            auto depthTexUAV = mpDepthTex->getUAV();
            pRenderContext->clearUAV(depthTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpForwardWarpDepthTestPass["gDepthTex"].setUav(depthTexUAV);
        }

        // Execute
        mpForwardWarpDepthTestPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(mpTempDepthTex.get());
            pRenderContext->uavBarrier(mpForwardMotionTex.get());
            pRenderContext->uavBarrier(mpDepthTex.get());
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
            pRenderContext->clearUAV(tempWarpTexUAV.get(), float4(-1.f));
            mpForwardWarpPass["gTempWarpTex"].setUav(tempWarpTexUAV);

            auto tempMotionVectorTexUAV = mpTempMotionVectorTex->getUAV();
            pRenderContext->clearUAV(tempMotionVectorTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpForwardWarpPass["gTempMotionVectorTex"].setUav(tempMotionVectorTexUAV);

        }

        // Execute
        mpForwardWarpPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(mpTempWarpTex.get());
            pRenderContext->uavBarrier(mpTempMotionVectorTex.get());
        }

        pRenderContext->blit(mpTempWarpTex->getSRV(), renderData.getTexture("PreTonemapped_out_woSplat")->getRTV());

    }
    // ###################################################################################



    // ################################### Splat #########################################
    {
        // Input
        {
            mpSplatPass["PerFrameCB"]["gFrameDim"] = curDim;
            mpSplatPass["PerFrameCB"]["gKernelSize"] = mKernelSize;
            mpSplatPass["PerFrameCB"]["gSplatSigma"] = mSplatSigma;
            mpSplatPass["PerFrameCB"]["gSplatDistSigma"] = gSplatDistSigma;
            mpSplatPass["PerFrameCB"]["gStrideNum"] = gSplatStrideNum;
            mpSplatPass["PerFrameCB"]["mDepthScale"] = mDepthScale;

            auto tempWarpTexSRV = mpTempWarpTex->getSRV();
            mpSplatPass["gTempWarpTex"].setSrv(tempWarpTexSRV);

            auto tempDepthTexSRV = mpTempDepthTex->getSRV();
            mpSplatPass["gTempDepthTex"].setSrv(tempDepthTexSRV);

            auto tempMotionVectorTexSRV = mpTempMotionVectorTex->getSRV();
            mpSplatPass["gTempMotionVectorTex"].setSrv(tempMotionVectorTexSRV);

            auto backgroundWarpedTexSRV = mpBackgroundWarpedTex->getSRV();
            mpSplatPass["gBackgroundWarpedTex"].setSrv(backgroundWarpedTexSRV);

            auto backgroundWarpedDepthTexSRV = mpBackgroundWarpedDepthTex->getSRV();
            mpSplatPass["gBackgroundWarpedDepthTex"].setSrv(backgroundWarpedDepthTexSRV);

        }

        // Output
        {
            auto targetRenderTexUAV = mpTempOutputTex->getUAV();
            pRenderContext->clearUAV(targetRenderTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpSplatPass["targetRenderTex"].setUav(targetRenderTexUAV);

            auto targetDepthTexUAV = mpTempOutputDepthTex->getUAV();
            pRenderContext->clearUAV(targetDepthTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpSplatPass["targetDepthTex"].setUav(targetDepthTexUAV);

            auto targetMotionVectorTexUAV = mpTempOutputMVTex->getUAV();
            pRenderContext->clearUAV(targetMotionVectorTexUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
            mpSplatPass["targetMotionVectorTex"].setUav(targetMotionVectorTexUAV);
        }

        // Execute
        mpSplatPass->execute(pRenderContext, curDim.x, curDim.y);

        // Barrier
        {
            pRenderContext->uavBarrier(mpTempOutputTex.get());
            pRenderContext->uavBarrier(mpTempOutputDepthTex.get());
            pRenderContext->uavBarrier(mpTempOutputMVTex.get());
        }
    }
    // ###################################################################################

    // Copy to output
    pRenderContext->blit(mpTempOutputTex->getSRV(), renderData.getTexture("PreTonemapped_out")->getRTV());
    pRenderContext->blit(mpTempOutputDepthTex->getSRV(), renderData.getTexture("LinearZ_out")->getRTV());
    pRenderContext->blit(mpTempOutputMVTex->getSRV(), renderData.getTexture("MotionVector_out")->getRTV());


}


void ForwardExtrapolation::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (mpScene == nullptr) return;


    Falcor::float3 curCameraPos = mpScene->getCamera()->getPosition();
    Falcor::float3 curCameraLookat = mpScene->getCamera()->getTarget();
    Falcor::float3 curCameraUp = mpScene->getCamera()->getUpVector();

    if (mFrameCount == -1 && mMode) {

        mPrevCameraPos = curCameraPos;
        mPrevCameraLookat = curCameraLookat;
        mPrevCameraUp = curCameraUp;

        mFrameCount += 1;

        pRenderContext->blit(renderData.getTexture("PreTonemapped_in")->getSRV(), renderData.getTexture("PreTonemapped_out")->getRTV());

        return;

    }


    // Ground truth rendered frame
    if (mFrameCount % (mExtrapolationNum + 1) == 0) {

        renderedFrameProcess(pRenderContext, renderData);

        if (mMode) {
            collectBackground(pRenderContext, renderData);
        }

        // Predict Future Camera

        mNextCameraPos = curCameraPos + (curCameraPos - mPrevCameraPos);
        mNextCameraLookat = curCameraLookat + (curCameraLookat - mPrevCameraLookat);
        mNextCameraUp = curCameraUp + (curCameraUp - mPrevCameraUp);



    }
    // Extrapolated Frame
    else {

        // Do nothing, rendering for every frame
        if (mMode == 0) {

            pRenderContext->blit(renderData.getTexture("PreTonemapped_in")->getSRV(), renderData.getTexture("PreTonemapped_out")->getRTV());
            pRenderContext->blit(renderData.getTexture("MotionVector_in")->getSRV(), renderData.getTexture("MotionVector_out")->getRTV());
            pRenderContext->blit(renderData.getTexture("LinearZ_in")->getSRV(), renderData.getTexture("LinearZ_out")->getRTV());
            pRenderContext->blit(renderData.getTexture("PreTonemapped_in")->getSRV(), renderData.getTexture("PreTonemapped_out_woSplat")->getRTV());

        }
        // Extrapolate frame
        else {

            auto nextCamera = Camera::create("nextCamera");
            *nextCamera = *(mpScene->getCamera());

            nextCamera->setPosition(mNextCameraPos);
            nextCamera->setTarget(mNextCameraLookat);
            nextCamera->setUpVector(mNextCameraUp);

            // Warp Background
            warpBackground(pRenderContext, renderData, nextCamera);

            // Some other operations
            extrapolatedFrameInit(pRenderContext, renderData, nextCamera);


            // Extrapolate Frame
            extrapolatedFrameProcess(pRenderContext, renderData, nextCamera);

        }

    }


    // Update Camera
    {
        mPrevCameraPos = curCameraPos;
        mPrevCameraLookat = curCameraLookat;
        mPrevCameraUp = curCameraUp;
    }

    if (mMode) mFrameCount += 1;
    // if (mFrameCount >= mExtrapolationNum + 1) mFrameCount = 0;

    if (mDumpData) DumpDataFunc(renderData, mFrameCount, mDumpDirPath);

}

void ForwardExtrapolation::renderUI(Gui::Widgets& widget)
{
    Gui::DropdownList modeList;
    modeList.push_back(Gui::DropdownValue{0, "default"});
    modeList.push_back(Gui::DropdownValue{1, "extrapolation"});
    widget.dropdown("Mode", modeList, mMode);


    widget.var<uint32_t>("Extrapolation Num", mExtrapolationNum, 1u, 1u);

    widget.var<uint32_t>("Kernel Size", mKernelSize, 1u, 32u);
    widget.var<float>("Splat Sigma", mSplatSigma, 0.1f, 20.f);
    widget.var<float>("Splat Dist Sigma", gSplatDistSigma, 0.1f, 20.f);
    widget.var<uint>("Splat Stride Num", gSplatStrideNum, 1u, 10u);

    widget.var<uint>("Background Depth Scale", mBackgroundDepthScale, 1u, 1024u);

    widget.checkbox("Dump Data", mDumpData);
    widget.textbox("Dump Dir Path", mDumpDirPath);

    widget.checkbox("Refresh background data", mIsNewBackground);

}
