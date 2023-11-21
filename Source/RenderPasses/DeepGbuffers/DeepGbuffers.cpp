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
#include "DeepGbuffers.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info DeepGbuffers::kInfo { "DeepGbuffers", "Generate Deep G-buffers." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(DeepGbuffers::kInfo, DeepGbuffers::create);
}

namespace {
const std::string genDeepGbuffersShaderFilePath =
    "RenderPasses/DeepGbuffers/GenGbuffers.slang";
const std::string warpDepthTestShaderFilePath =
    "RenderPasses/DeepGbuffers/WarpDepthTest.slang";
const std::string warpGbuffersShaderFilePath =
    "RenderPasses/DeepGbuffers/Warp.slang";

std::vector<std::string> albedoLayers = {
    "albedo0",
    "albedo1",
    "albedo2",
    "albedo3",
    "albedo4"
};

std::vector<std::string> normalLayers = {
    "normal0",
    "normal1",
    "normal2",
    "normal3",
    "normal4"
};


}  // namespace



DeepGbuffers::SharedPtr DeepGbuffers::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new DeepGbuffers());
    return pPass;
}

Dictionary DeepGbuffers::getScriptingDictionary()
{
    return Dictionary();
}

DeepGbuffers::DeepGbuffers() : RenderPass(kInfo)
{
    mpScene = nullptr;

    // Initialize objects for raster pass
    RasterizerState::Desc rasterDesc;
    rasterDesc.setFillMode(RasterizerState::FillMode::Solid);
    rasterDesc.setCullMode(RasterizerState::CullMode::None);  // Not working

    // Turn on/off depth test
    DepthStencilState::Desc dsDesc;

    {
        // Initialize raster graphics state
        mRasterPass.pGraphicsState = GraphicsState::create();
        mRasterPass.pGraphicsState->setRasterizerState(
            RasterizerState::create(rasterDesc));
        mRasterPass.pGraphicsState->setDepthStencilState(
            DepthStencilState::create(dsDesc));
        // Create framebuffer
        mRasterPass.pFbo = Fbo::create();
    }

}

void DeepGbuffers::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{

    if (pScene) {

        mpScene = pScene;

        InitVars();

        // Create raster pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(genDeepGbuffersShaderFilePath)
                .vsEntry("vsMain")
                .psEntry("psMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            auto program =
                GraphicsProgram::create(desc, mpScene->getSceneDefines());
            mRasterPass.pGraphicsState->setProgram(program);
            mRasterPass.pVars = GraphicsVars::create(program.get());
        }

        {
            Program::Desc desc;
            desc.addShaderLibrary(warpDepthTestShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpWarpDepthTestPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpWarpDepthTestPass->setVars(nullptr);  // Trigger vars creation
        }

        {
            Program::Desc desc;
            desc.addShaderLibrary(warpGbuffersShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpWarpPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpWarpPass->setVars(nullptr);  // Trigger vars creation
        }


    }
}

RenderPassReflection DeepGbuffers::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    //reflector.addOutput("dst");
    //reflector.addInput("src");

    for (int i = 0; i < 5; ++i) {
        reflector.addOutput(normalLayers[i], "Normal" + std::to_string(i))
                .format(ResourceFormat::RGBA32Float)
                .bindFlags(Resource::BindFlags::AllColorViews)
                .texture2D();

        reflector.addOutput(albedoLayers[i], "Albedo" + std::to_string(i))
                .format(ResourceFormat::RGBA32Float)
                .bindFlags(Resource::BindFlags::AllColorViews)
                .texture2D();

    }

    reflector.addOutput("GTNormal", "GTNormal")
            .format(ResourceFormat::RGBA32Float)
            .bindFlags(Resource::BindFlags::AllColorViews)
            .texture2D();

    reflector.addOutput("GTAlbedo", "GTAlbedo")
            .format(ResourceFormat::RGBA32Float)
            .bindFlags(Resource::BindFlags::AllColorViews)
            .texture2D();

    reflector.addOutput("GTPosW", "GT PosW")
            .format(ResourceFormat::RGBA32Float)
            .bindFlags(Resource::BindFlags::AllColorViews)
            .texture2D();

    reflector.addOutput("Normal", "Output Normal")
            .format(ResourceFormat::RGBA32Float)
            .bindFlags(Resource::BindFlags::AllColorViews)
            .texture2D();
    reflector.addOutput("Albedo", "Output Albedo")
            .format(ResourceFormat::RGBA32Float)
            .bindFlags(Resource::BindFlags::AllColorViews)
            .texture2D();

    return reflector;
}


void DeepGbuffers::GenGbuffers(RenderContext* pRenderContext, const RenderData& renderData)
{



    for (uint level = 0; level < mGbufferLevel; ++level) {

        if (level > 0) {
            auto prevLinearZSRV = mDeepGbuf.pLinearZ->getSRV(0, -1, 0, level);
            mRasterPass.pVars["gPrevLinearZ"].setSrv(prevLinearZSRV);
        }

        mRasterPass.pVars["PerFrameCB"]["gThreshold"] = mThreshold;
        mRasterPass.pVars["PerFrameCB"]["gCurLevel"] = level;


        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pAlbedo, 0, 0, level, 1);
        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pNormal, 1, 0, level, 1);
        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pNextPosW, 2, 0, level, 1);
        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pLinearZ, 3, 0, level, 1);
        mRasterPass.pFbo->attachDepthStencilTarget(mDeepGbuf.pDepth, 0, level, 1);
        pRenderContext->clearFbo(mRasterPass.pFbo.get(), float4(0, 0, 0, 1), 1.0f,
                                0, FboAttachmentType::All);
        mRasterPass.pGraphicsState->setFbo(mRasterPass.pFbo);
        // Rasterize it!
        mpScene->rasterize(pRenderContext, mRasterPass.pGraphicsState.get(),
                        mRasterPass.pVars.get(),
                        RasterizerState::CullMode::None);

    }

    for (uint i = 0; i < mGbufferLevel && i < 5; ++i) {
        auto normalSRV = mDeepGbuf.pNormal->getSRV(0, -1, i, 1);
        auto albedoSRV = mDeepGbuf.pAlbedo->getSRV(0, -1, i, 1);

        pRenderContext->blit(normalSRV, renderData.getTexture(normalLayers[i])->getRTV());
        pRenderContext->blit(albedoSRV, renderData.getTexture(albedoLayers[i])->getRTV());
    }

    auto gtNormalSRV = mDeepGbuf.pNormal->getSRV(0, -1, 0, 1);
    auto gtAlbedoSRV = mDeepGbuf.pAlbedo->getSRV(0, -1, 0, 1);
    auto gtPosWSRV = mDeepGbuf.pNextPosW->getSRV(0, -1, 0, 1);

    pRenderContext->blit(gtNormalSRV, renderData.getTexture("GTNormal")->getRTV());
    pRenderContext->blit(gtAlbedoSRV, renderData.getTexture("GTAlbedo")->getRTV());
    pRenderContext->blit(gtPosWSRV, renderData.getTexture("GTPosW")->getRTV());

    if (mDisplayMode == 1) {
        return;
    }
    pRenderContext->blit(gtNormalSRV, renderData.getTexture("Normal")->getRTV());
    pRenderContext->blit(gtAlbedoSRV, renderData.getTexture("Albedo")->getRTV());


}

void DeepGbuffers::GenGTGbuffers(RenderContext* pRenderContext, const RenderData& renderData)
{
    mRasterPass.pVars["PerFrameCB"]["gThreshold"] = -1;
    mRasterPass.pVars["PerFrameCB"]["gCurLevel"] = 0;


    mRasterPass.pFbo->attachColorTarget(mGTGbuf.pAlbedo, 0);
    mRasterPass.pFbo->attachColorTarget(mGTGbuf.pNormal, 1);
    mRasterPass.pFbo->attachColorTarget(mGTGbuf.pNextPosW, 2);
    mRasterPass.pFbo->attachColorTarget(mGTGbuf.pLinearZ, 3);
    mRasterPass.pFbo->attachDepthStencilTarget(mGTGbuf.pDepth);
    pRenderContext->clearFbo(mRasterPass.pFbo.get(), float4(0, 0, 0, 1), 1.0f,
                            0, FboAttachmentType::All);
    mRasterPass.pGraphicsState->setFbo(mRasterPass.pFbo);
    // Rasterize it!
    mpScene->rasterize(pRenderContext, mRasterPass.pGraphicsState.get(),
                        mRasterPass.pVars.get(),
                        RasterizerState::CullMode::None);

    auto gtNormalSRV = mGTGbuf.pNormal->getSRV();
    auto gtAlbedoSRV = mGTGbuf.pAlbedo->getSRV();

    pRenderContext->blit(gtNormalSRV, renderData.getTexture("GTNormal")->getRTV());
    pRenderContext->blit(gtAlbedoSRV, renderData.getTexture("GTAlbedo")->getRTV());

}


void DeepGbuffers::WarpGbuffers(RenderContext* pRenderContext, const RenderData& renderData)
{

    auto curDim = renderData.getDefaultTextureDims();
    auto curViewProjMat = mpScene->getCamera()->getViewProjMatrix();

    // Depth test
    {
        // Input
        {
            mpWarpDepthTestPass["PerFrameCB"]["gViewProjMat"] = curViewProjMat;
            mpWarpDepthTestPass["PerFrameCB"]["gCurDim"] = curDim;
            mpWarpDepthTestPass["PerFrameCB"]["gLevelNum"] = mGbufferLevel;
            mpWarpDepthTestPass["PerFrameCB"]["gLinearZScale"] = mDepthTestScale;
            mpWarpDepthTestPass["PerFrameCB"]["gSubpixelNum"] = mSubpixelNum;
            mpWarpDepthTestPass["PerFrameCB"]["gNearRatio"] = mNearRatio;

            auto posWSRV = mDeepGbuf.pNextPosW->getSRV();
            mpWarpDepthTestPass["gPosW"].setSrv(posWSRV);

            auto normalSRV = mDeepGbuf.pNormal->getSRV();
            mpWarpDepthTestPass["gNormal"].setSrv(normalSRV);

        }

        // Output
        {

            auto depthUAV = mCurGbuf.pDepth->getUAV();
            pRenderContext->clearUAV(depthUAV.get(), uint4(-1));
            mpWarpDepthTestPass["gDepth"].setUav(depthUAV);

        }

        mpWarpDepthTestPass->execute(pRenderContext, uint3(curDim, mGbufferLevel));

        {
            pRenderContext->uavBarrier(mCurGbuf.pDepth.get());
        }

    }


    // Warping
    {
        // Input
        {
            mpWarpPass["PerFrameCB"]["gViewProjMat"] = curViewProjMat;
            mpWarpPass["PerFrameCB"]["gCurDim"] = curDim;
            mpWarpPass["PerFrameCB"]["gLevelNum"] = mGbufferLevel;
            mpWarpPass["PerFrameCB"]["gLinearZScale"] = mDepthTestScale;
            mpWarpPass["PerFrameCB"]["gSubpixelNum"] = mSubpixelNum;
            mpWarpPass["PerFrameCB"]["gNearRatio"] = mNearRatio;

            auto posWSRV = mDeepGbuf.pNextPosW->getSRV();
            mpWarpPass["gPosW"].setSrv(posWSRV);

            auto normalSRV = mDeepGbuf.pNormal->getSRV();
            mpWarpPass["gNormal"].setSrv(normalSRV);

            auto albedoSRV = mDeepGbuf.pAlbedo->getSRV();
            mpWarpPass["gAlbedo"].setSrv(albedoSRV);

            auto depthSRV = mCurGbuf.pDepth->getSRV();
            mpWarpPass["gDepth"].setSrv(depthSRV);
        }

        {
            // Output
            auto normalUAV = mCurGbuf.pNormal->getUAV();
            pRenderContext->clearUAV(normalUAV.get(), float4(0, 0, 0, 0));
            mpWarpPass["gNormalOut"].setUav(normalUAV);

            auto albedoUAV = mCurGbuf.pAlbedo->getUAV();
            pRenderContext->clearUAV(albedoUAV.get(), float4(0, 0, 0, 0));
            mpWarpPass["gAlbedoOut"].setUav(albedoUAV);
        }

        mpWarpPass->execute(pRenderContext, uint3(curDim, mGbufferLevel));

        {
            pRenderContext->uavBarrier(mCurGbuf.pNormal.get());
            pRenderContext->uavBarrier(mCurGbuf.pAlbedo.get());
        }
    }

    pRenderContext->blit(mCurGbuf.pNormal->getSRV(), renderData.getTexture("Normal")->getRTV());
    pRenderContext->blit(mCurGbuf.pAlbedo->getSRV(), renderData.getTexture("Albedo")->getRTV());

}

void DeepGbuffers::execute(RenderContext* pRenderContext, const RenderData& renderData)
{


    if (!mpScene) {
        return;
    }

    uint2 curDim = renderData.getDefaultTextureDims();

    CreateTextures(curDim);


    if (mFrameCount % mGenFreq == 0) {
        // Gen Gbuffers
        GenGbuffers(pRenderContext, renderData);
    }

    else {
        // Warp Gbuffers

        GenGTGbuffers(pRenderContext, renderData);

        WarpGbuffers(pRenderContext, renderData);

    }

    mFrameCount++;

}

void DeepGbuffers::renderUI(Gui::Widgets& widget)
{

    widget.var<float>("Eps", mThreshold, -1.0f, 10.0f);
    widget.var<uint>("Gbuffer Level", mGbufferLevel, 1, 16);
    widget.var<uint>("Gen Freq", mGenFreq, 1, 10);
    widget.var<uint>("Subpixel Num", mSubpixelNum, 1, 10);
    widget.var<float>("Depth Test Scale", mDepthTestScale, 1, 10000);
    widget.var<float>("Near Ratio", mNearRatio, 0.1f, 4.0f);

    Gui::DropdownList modeList;
    modeList.push_back(Gui::DropdownValue{0, "all"});
    modeList.push_back(Gui::DropdownValue{1, "extrapolation only"});

    widget.dropdown("Mode", modeList, mDisplayMode);
}


void DeepGbuffers::createNewTexture(Texture::SharedPtr &pTex, const Falcor::uint2 &curDim, enum Falcor::ResourceFormat dataFormat, Falcor::Resource::BindFlags bindFlags, const uint arraySize)
{
    if (!pTex || pTex->getWidth() != curDim.x || pTex->getHeight() != curDim.y || pTex->getArraySize() != arraySize) {

        pTex = Texture::create2D(curDim.x, curDim.y, dataFormat,
                                    arraySize, 1, nullptr,
                                    bindFlags);

    }
}


void DeepGbuffers::InitVars()
{
    mDeepGbuf.pNormal = nullptr;
    mDeepGbuf.pAlbedo = nullptr;
    mDeepGbuf.pNextPosW = nullptr;
    mDeepGbuf.pLinearZ = nullptr;
    mDeepGbuf.pDepth = nullptr;

    mGTGbuf.pNormal = nullptr;
    mGTGbuf.pAlbedo = nullptr;
    mGTGbuf.pNextPosW = nullptr;
    mGTGbuf.pLinearZ = nullptr;
    mGTGbuf.pDepth = nullptr;

    mCurGbuf.pNormal = nullptr;
    mCurGbuf.pAlbedo = nullptr;
    mCurGbuf.pNextPosW = nullptr;
    mCurGbuf.pLinearZ = nullptr;
    mCurGbuf.pDepth = nullptr;


    mFrameCount = 0;
    mGenFreq = 2;

    mGbufferLevel = 5;

    mThreshold = 0.1f;
}


void DeepGbuffers::CreateTextures(const uint2 &curDim)
{

    // Create Textures
    {
        createNewTexture(mDeepGbuf.pNormal, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
        createNewTexture(mDeepGbuf.pAlbedo, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
        createNewTexture(mDeepGbuf.pNextPosW, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
        createNewTexture(mDeepGbuf.pLinearZ, curDim, ResourceFormat::R32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
        createNewTexture(mDeepGbuf.pDepth, curDim, ResourceFormat::D32Float, Resource::BindFlags::DepthStencil, mGbufferLevel);

        createNewTexture(mGTGbuf.pNormal, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, 1);
        createNewTexture(mGTGbuf.pAlbedo, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, 1);
        createNewTexture(mGTGbuf.pNextPosW, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, 1);
        createNewTexture(mGTGbuf.pLinearZ, curDim, ResourceFormat::R32Float, Resource::BindFlags::AllColorViews, 1);
        createNewTexture(mGTGbuf.pDepth, curDim, ResourceFormat::D32Float, Resource::BindFlags::DepthStencil, 1);

        createNewTexture(mCurGbuf.pNormal, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, 1);
        createNewTexture(mCurGbuf.pAlbedo, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, 1);
        // createNewTexture(mCurGbuf.pNextPosW, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, 1);
        // createNewTexture(mCurGbuf.pLinearZ, curDim, ResourceFormat::R32Float, Resource::BindFlags::AllColorViews, 1);
        createNewTexture(mCurGbuf.pDepth, curDim, ResourceFormat::R32Uint, Resource::BindFlags::AllColorViews, 1);


    }
}
