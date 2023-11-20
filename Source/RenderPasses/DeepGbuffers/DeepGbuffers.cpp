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


        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pAlbedo, 0);
        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pNormal, 1);
        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pNextPosW, 2);
        mRasterPass.pFbo->attachColorTarget(mDeepGbuf.pLinearZ, 3);
        mRasterPass.pFbo->attachDepthStencilTarget(mpDepthStencil);
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


}

void DeepGbuffers::execute(RenderContext* pRenderContext, const RenderData& renderData)
{


    if (!mpScene) {
        return;
    }

    auto curDim = renderData.getDefaultTextureDims();

    // Create Textures
    {
        createNewTexture(mpDepthStencil, curDim, ResourceFormat::D32Float, Resource::BindFlags::DepthStencil, 1);

        createNewTexture(mDeepGbuf.pNormal, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
        createNewTexture(mDeepGbuf.pAlbedo, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
        createNewTexture(mDeepGbuf.pNextPosW, curDim, ResourceFormat::RGBA32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
        createNewTexture(mDeepGbuf.pLinearZ, curDim, ResourceFormat::R32Float, Resource::BindFlags::AllColorViews, mGbufferLevel);
    }



    if (mFrameCount % mGenFreq == 0) {
        // Gen Gbuffers
        GenGbuffers(pRenderContext, renderData);
    }

    else {
        // Warp Gbuffers
    }

    mFrameCount++;

}

void DeepGbuffers::renderUI(Gui::Widgets& widget)
{

    widget.var<float>("Eps", mThreshold, -1.0f, 10.0f);
    widget.var<uint>("Gbuffer Level", mGbufferLevel, 1, 5);
    widget.var<uint>("Gen Freq", mGenFreq, 1, 10);
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

    mGTGbuf.pNormal = nullptr;
    mGTGbuf.pAlbedo = nullptr;
    mGTGbuf.pNextPosW = nullptr;
    mGTGbuf.pLinearZ = nullptr;


    mpDepthStencil = nullptr;

    mFrameCount = 0;
    mGenFreq = 2;

    mGbufferLevel = 5;

    mThreshold = 0.1f;
}
