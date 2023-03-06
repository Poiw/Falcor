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
#include "TwoLayeredGbuffers.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info TwoLayeredGbuffers::kInfo { "TwoLayeredGbuffers", "Generate two layer g-buffers" };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(TwoLayeredGbuffers::kInfo, TwoLayeredGbuffers::create);
}


namespace {
const std::string twoLayerGbuffersShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/TwoLayerGbufferGen.slang";
}  // namespace



TwoLayeredGbuffers::TwoLayeredGbuffers() : RenderPass(kInfo) {
    // Initialize objects for raster pass
    RasterizerState::Desc rasterDesc;
    rasterDesc.setFillMode(RasterizerState::FillMode::Solid);
    rasterDesc.setCullMode(RasterizerState::CullMode::None);  // Not working
    // Turn on/off depth test
    DepthStencilState::Desc dsDesc;
    // dsDesc.setDepthEnabled(false);
    // Initialize raster graphics state
    mRasterPass.pGraphicsState = GraphicsState::create();
    mRasterPass.pGraphicsState->setRasterizerState(
        RasterizerState::create(rasterDesc));
    mRasterPass.pGraphicsState->setDepthStencilState(
        DepthStencilState::create(dsDesc));
    // Create framebuffer
    mRasterPass.pFbo = Fbo::create();

    mEps = 0.1f;

    // Create sample generator
    mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
}

void TwoLayeredGbuffers::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
     mRasterPass.pVars = nullptr;

    if (pScene) {
        mpScene = pScene;
        // Create raster pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(twoLayerGbuffersShaderFilePath)
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

TwoLayeredGbuffers::SharedPtr TwoLayeredGbuffers::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new TwoLayeredGbuffers());
    return pPass;
}

Dictionary TwoLayeredGbuffers::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection TwoLayeredGbuffers::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    // Inputs
    reflector.addInput("gPosWS", "Wolrd position")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("gNormalWS", "Normal")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("gTangentWS", "Tangent")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("gFirstLinearZ", "First Linear Z")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("gFirstDepth", "Depth")
        .format(ResourceFormat::D32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    // Outputs
    reflector.addOutput("gMyDepth", "My Depth Buffer")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::UnorderedAccess |
                   Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addOutput("gDepth", "Depth buffer")
        .format(ResourceFormat::D32Float)
        .bindFlags(Resource::BindFlags::DepthStencil)
        .texture2D();
    reflector.addOutput("gDebug", "Debug")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("gDebug2", "Debug2")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    return reflector;
}

void TwoLayeredGbuffers::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (mpScene == nullptr) return;


    const uint mostDetailedMip = 0;
    auto pMyDepthMap = renderData.getTexture("gMyDepth");
    auto pMyDepthMapUAVMip0 = pMyDepthMap->getUAV(mostDetailedMip);
    pRenderContext->clearUAV(pMyDepthMapUAVMip0.get(), uint4(0));
    // mDepthBuffer = pMyDepthMapUAVMip0->getResource()->asTexture();  // Save for dumping

    auto pFirstDepthMap = renderData.getTexture("gFirstDepth");
    auto pFirstDepthMapRSVMip0 = pFirstDepthMap->getSRV(mostDetailedMip);


    auto pFirstLinearZMap = renderData.getTexture("gFirstLinearZ");
    auto pFirstLinearZMapRSVMip0 = pFirstLinearZMap->getSRV(mostDetailedMip);


    mRasterPass.pVars["PerFrameCB"]["gEps"] = mEps;

    mRasterPass.pVars["gDepthBuffer"].setUav(pMyDepthMapUAVMip0);
    mRasterPass.pVars["gFirstDepthBuffer"].setSrv(pFirstDepthMapRSVMip0);
    mRasterPass.pVars["gFirstLinearZBuffer"].setSrv(pFirstLinearZMapRSVMip0);
    mRasterPass.pFbo->attachColorTarget(renderData.getTexture("gDebug"), 0);
    mRasterPass.pFbo->attachColorTarget(renderData.getTexture("gDebug2"), 1);
    // mRasterPass.pFbo->attachColorTarget(renderData.getTexture("gTangentWS"), 2);
    // mRasterPass.pFbo->attachColorTarget(renderData.getTexture("gPosWS"), 3);
    mRasterPass.pFbo->attachDepthStencilTarget(renderData.getTexture("gDepth"));
    pRenderContext->clearFbo(mRasterPass.pFbo.get(), float4(0, 0, 0, 1), 1.0f,
                             0, FboAttachmentType::All);
    mRasterPass.pGraphicsState->setFbo(mRasterPass.pFbo);
    // Rasterize it!
    mpScene->rasterize(pRenderContext, mRasterPass.pGraphicsState.get(),
                       mRasterPass.pVars.get(),
                       RasterizerState::CullMode::None);


}

void TwoLayeredGbuffers::renderUI(Gui::Widgets& widget)
{
    widget.slider("Eps", mEps, 0.0f, 5.0f);
}
