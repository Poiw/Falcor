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
    "RenderPasses/TwoLayeredGbuffers/TwoLayerGbufferVis.slang";
const std::string twoLayerGbuffersGenShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/TwoLayerGbufferGen.slang";
const std::string warpGbuffersShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/WarpGbuffer.slang";
const std::string projectionDepthTestShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/ProjectionDepthTest.slang";
const std::string forwardWarpGbufferShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/ForwardWarpGbuffer.slang";
const std::string mergeLayersShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/MergeLayers.slang";
}  // namespace



TwoLayeredGbuffers::TwoLayeredGbuffers() : RenderPass(kInfo) {
    // Initialize objects for raster pass
    RasterizerState::Desc rasterDesc;
    rasterDesc.setFillMode(RasterizerState::FillMode::Solid);
    rasterDesc.setCullMode(RasterizerState::CullMode::None);  // Not working
    // Turn on/off depth test
    DepthStencilState::Desc dsDesc;
    // dsDesc.setDepthEnabled(false);

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

    {
        // Initialize raster graphics state
        mWarpGbufferPass.pGraphicsState = GraphicsState::create();
        mWarpGbufferPass.pGraphicsState->setRasterizerState(
            RasterizerState::create(rasterDesc));
        mWarpGbufferPass.pGraphicsState->setDepthStencilState(
            DepthStencilState::create(dsDesc));
        // Create framebuffer
        mWarpGbufferPass.pFbo = Fbo::create();
    }

    {
        // Initialize raster graphics state
        mTwoLayerGbufferGenPass.pGraphicsState = GraphicsState::create();
        mTwoLayerGbufferGenPass.pGraphicsState->setRasterizerState(
            RasterizerState::create(rasterDesc));
        mTwoLayerGbufferGenPass.pGraphicsState->setDepthStencilState(
            DepthStencilState::create(dsDesc));
        // Create framebuffer
        mTwoLayerGbufferGenPass.pFbo = Fbo::create();
    }

    mEps = 0.0f;
    mFrameCount = 0;
    mFreshNum = 8;
    mMode = 0;
    mNearestThreshold = 1;
    mSubPixelSample = 1;

    // // Create sample generator
    // mpSampleGenerator = SampleGenerator::create(SAMPLE_GENERATOR_UNIFORM);
}

void TwoLayeredGbuffers::ClearVariables()
{
    // mpPosWSBuffer = nullptr;
    // mpPosWSBufferTemp = nullptr;
    mpDepthTestBuffer = nullptr;

    mFirstLayerGbuffer.mpPosWS = nullptr;
    mFirstLayerGbuffer.mpNormWS = nullptr;
    mFirstLayerGbuffer.mpDiffOpacity = nullptr;

    mSecondLayerGbuffer.mpPosWS = nullptr;
    mSecondLayerGbuffer.mpNormWS = nullptr;
    mSecondLayerGbuffer.mpDiffOpacity = nullptr;

    mProjFirstLayer.mpDepthTest = nullptr;
    mProjFirstLayer.mpNormWS = nullptr;
    mProjFirstLayer.mpDiffOpacity = nullptr;

    mProjSecondLayer.mpDepthTest = nullptr;
    mProjSecondLayer.mpNormWS = nullptr;
    mProjSecondLayer.mpDiffOpacity = nullptr;

    mMergedLayer.mpMask = nullptr;
    mMergedLayer.mpNormWS = nullptr;
    mMergedLayer.mpDiffOpacity = nullptr;

    mMode = 0;
    mNormalThreshold = 1.0;
    mFrameCount = 0;
    mFreshNum = 8;
    mMaxDepthContraint = 0;
    mNormalConstraint = 0;
}

void TwoLayeredGbuffers::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
     mRasterPass.pVars = nullptr;
     mWarpGbufferPass.pVars = nullptr;
     mTwoLayerGbufferGenPass.pVars = nullptr;

    if (pScene) {
        mpScene = pScene;

        ClearVariables();

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
        // Create warp Gbuffer pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(warpGbuffersShaderFilePath)
                .vsEntry("vsMain")
                .psEntry("psMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            auto program =
                GraphicsProgram::create(desc, mpScene->getSceneDefines());
            mWarpGbufferPass.pGraphicsState->setProgram(program);
            mWarpGbufferPass.pVars = GraphicsVars::create(program.get());
        }

        // Create Gen Two Gbuffer pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(twoLayerGbuffersGenShaderFilePath)
                .vsEntry("vsMain")
                .psEntry("psMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            auto program =
                GraphicsProgram::create(desc, mpScene->getSceneDefines());
            mTwoLayerGbufferGenPass.pGraphicsState->setProgram(program);
            mTwoLayerGbufferGenPass.pVars = GraphicsVars::create(program.get());
        }

        // Create a Forward Warping Pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(projectionDepthTestShaderFilePath).csEntry("csMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpProjectionDepthTestPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpProjectionDepthTestPass->setVars(nullptr);  // Trigger vars creation
            // mpForwardWarpPass["gScene"] = mpScene->getParameterBlock();
        }


        // Create a Forward Warping Pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(forwardWarpGbufferShaderFilePath).csEntry("csMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpForwardWarpPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpForwardWarpPass->setVars(nullptr);  // Trigger vars creation
            // mpForwardWarpPass["gScene"] = mpScene->getParameterBlock();
        }

        // Create a Merge Layer Warping Pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(mergeLayersShaderFilePath).csEntry("csMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpMergeLayerPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpMergeLayerPass->setVars(nullptr);  // Trigger vars creation
            // mpForwardWarpPass["gScene"] = mpScene->getParameterBlock();
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
    reflector.addInput("gDiffOpacity", "Diffuse reflection albedo and opacity")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("gLinearZ", "Linear Z")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("gDepth", "Depth")
        .format(ResourceFormat::D32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    // Outputs
    reflector.addOutput("tl_Depth", "Depth buffer")
        .format(ResourceFormat::D32Float)
        .bindFlags(Resource::BindFlags::DepthStencil)
        .texture2D();

    reflector.addOutput("tl_Debug", "Debug Info")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    reflector.addOutput("tl_Mask", "Mask Info")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    // First Layer
    reflector.addOutput("tl_FirstNormWS", "World Normal")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_FirstDiffOpacity", "Albedo and Opacity")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_FirstPosWS", "World Space Position")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    // Second Layer
    reflector.addOutput("tl_SecondNormWS", "World Normal")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_SecondDiffOpacity", "Albedo and Opacity")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_SecondPosWS", "World Space Position")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    return reflector;
}

void TwoLayeredGbuffers::createNewTexture(Texture::SharedPtr &pTex, const Falcor::uint2 &curDim, enum Falcor::ResourceFormat dataFormat = ResourceFormat::RGBA32Float)
{

    if (!pTex || pTex->getWidth() != curDim.x || pTex->getHeight() != curDim.y) {

        pTex = Texture::create2D(curDim.x, curDim.y, dataFormat,
                                    1U, 1, nullptr,
                                    Resource::BindFlags::AllColorViews);

    }

}

void TwoLayeredGbuffers::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (mpScene == nullptr) return;

    // const uint mostDetailedMip = 0;

    if (mMode == 0) {

        mRasterPass.pVars["PerFrameCB"]["gEps"] = mEps;

        // auto pMyDepthMap = renderData.getTexture("gMyDepth");
        // auto pMyDepthMapUAVMip0 = pMyDepthMap->getUAV();
        // pRenderContext->clearUAV(pMyDepthMapUAVMip0.get(), uint4(0));
        // mDepthBuffer = pMyDepthMapUAVMip0->getResource()->asTexture();  // Save for dumping

        // auto pFirstDepthMap = renderData.getTexture("gFirstDepth");
        // auto pFirstDepthMapRSVMip0 = pFirstDepthMap->getSRV();

        auto pFirstLinearZMap = renderData.getTexture("gLinearZ");
        auto pFirstLinearZMapRSVMip0 = pFirstLinearZMap->getSRV();



        // mRasterPass.pVars["gDepthBuffer"].setUav(pMyDepthMapUAVMip0);
        // mRasterPass.pVars["gFirstDepthBuffer"].setSrv(pFirstDepthMapRSVMip0);
        mRasterPass.pVars["gFirstLinearZBuffer"].setSrv(pFirstLinearZMapRSVMip0);
        mRasterPass.pFbo->attachColorTarget(renderData.getTexture("tl_Debug"), 0);
        mRasterPass.pFbo->attachColorTarget(renderData.getTexture("tl_SecondNormWS"), 1);
        mRasterPass.pFbo->attachColorTarget(renderData.getTexture("tl_SecondDiffOpacity"), 2);
        // mRasterPass.pFbo->attachColorTarget(renderData.getTexture("gTangentWS"), 2);
        // mRasterPass.pFbo->attachColorTarget(renderData.getTexture("gPosWS"), 3);
        mRasterPass.pFbo->attachDepthStencilTarget(renderData.getTexture("tl_Depth"));
        pRenderContext->clearFbo(mRasterPass.pFbo.get(), float4(0, 0, 0, 1), 1.0f,
                                0, FboAttachmentType::All);
        mRasterPass.pGraphicsState->setFbo(mRasterPass.pFbo);
        // Rasterize it!
        mpScene->rasterize(pRenderContext, mRasterPass.pGraphicsState.get(),
                        mRasterPass.pVars.get(),
                        RasterizerState::CullMode::None);

    }

    else if (mMode == 1 || mMode == 2) {

        if (mFrameCount % mFreshNum == 0) {

            {
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["gEps"] = mEps;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["gNormalThreshold"] = mNormalThreshold;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["maxConstraint"] = mMaxDepthContraint;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["normalConstraint"] = mNormalConstraint;
                // mCurEps = mEps;
            }

            // Save the projection matrix
            mCenterMatrix = mpScene->getCamera()->getViewProjMatrix();
            mCenterMatrixInv = mpScene->getCamera()->getInvViewProjMatrix();


            auto curDim = renderData.getDefaultTextureDims();

            // if (!mpPosWSBuffer || mpPosWSBuffer->getWidth() != curDim.x || mpPosWSBuffer->getHeight() != curDim.y) {

            //     mpPosWSBuffer = Texture::create2D(curDim.x, curDim.y, ResourceFormat::RGBA32Float,
            //                                 1U, 1, nullptr,
            //                                 Resource::BindFlags::AllColorViews);

            // }


            // if (!mpLinearZBuffer || mpLinearZBuffer->getWidth() != curDim.x || mpLinearZBuffer->getHeight() != curDim.y) {

            //     mpLinearZBuffer = Texture::create2D(curDim.x, curDim.y, ResourceFormat::RGBA32Float,
            //                                 1U, 1, nullptr,
            //                                 Resource::BindFlags::AllColorViews);

            // }

            // Create textures if necessary
            {
                // createNewTexture(mpPosWSBuffer, curDim);
                createNewTexture(mpLinearZBuffer, curDim, ResourceFormat::RGBA32Float);

                createNewTexture(mFirstLayerGbuffer.mpPosWS, curDim);
                createNewTexture(mFirstLayerGbuffer.mpNormWS, curDim);
                createNewTexture(mFirstLayerGbuffer.mpDiffOpacity, curDim);
                createNewTexture(mFirstLayerGbuffer.mpDepth, curDim, ResourceFormat::R32Float);

                createNewTexture(mSecondLayerGbuffer.mpPosWS, curDim);
                createNewTexture(mSecondLayerGbuffer.mpNormWS, curDim);
                createNewTexture(mSecondLayerGbuffer.mpDiffOpacity, curDim);
                createNewTexture(mSecondLayerGbuffer.mpDepth, curDim, ResourceFormat::R32Float);
            }



            // Textures
            {
                // auto pPosWSMap = renderData.getTexture("gPosWS");
                // auto pPosWSMapUAVMip0 = pPosWSMap->getSRV();

                auto pLinearZBufferUAV = mpLinearZBuffer->getUAV();
                pRenderContext->clearUAV(pLinearZBufferUAV.get(), float4(0.));
                mTwoLayerGbufferGenPass.pVars["gLinearZThresholdBuffer"].setUav(pLinearZBufferUAV);

                // mTwoLayerGbufferGenPass.pVars["gPosWSBuffer"].setSrv(pPosWSMapUAVMip0);
                // mTwoLayerGbufferGenPass.pVars["gTargetPosWSBuffer"].setUav(pWritePosWSMap);

                auto pFirstLinearZMap = renderData.getTexture("gLinearZ");
                auto pFirstLinearZMapRSVMip0 = pFirstLinearZMap->getSRV();
                mTwoLayerGbufferGenPass.pVars["gFirstLinearZBuffer"].setSrv(pFirstLinearZMapRSVMip0);

                auto pFirstNormWSMap = renderData.getTexture("gNormalWS");
                auto pFirstNormWSMapRSV = pFirstNormWSMap->getSRV();
                mTwoLayerGbufferGenPass.pVars["gFirstNormWSBuffer"].setSrv(pFirstNormWSMapRSV);
            }



            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(renderData.getTexture("tl_Debug"), 0);
            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(renderData.getTexture("tl_Mask"), 1);
            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(renderData.getTexture("tl_SecondNormWS"), 2);
            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(renderData.getTexture("tl_SecondDiffOpacity"), 3);
            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(renderData.getTexture("tl_SecondPosWS"), 4);
            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(mSecondLayerGbuffer.mpDepth, 5);

            mTwoLayerGbufferGenPass.pFbo->attachDepthStencilTarget(renderData.getTexture("tl_Depth"));

            pRenderContext->clearFbo(mTwoLayerGbufferGenPass.pFbo.get(), float4(0, 0, 0, 1), 1.0f,
                                    0, FboAttachmentType::All);
            mTwoLayerGbufferGenPass.pGraphicsState->setFbo(mTwoLayerGbufferGenPass.pFbo);
            // Rasterize it!
            mpScene->rasterize(pRenderContext, mTwoLayerGbufferGenPass.pGraphicsState.get(),
                            mTwoLayerGbufferGenPass.pVars.get(),
                            RasterizerState::CullMode::None);

            // Copy to render target
            pRenderContext->blit(renderData.getTexture("gNormalWS")->getSRV(), renderData.getTexture("tl_FirstNormWS")->getRTV());
            pRenderContext->blit(renderData.getTexture("gDiffOpacity")->getSRV(), renderData.getTexture("tl_FirstDiffOpacity")->getRTV());
            pRenderContext->blit(renderData.getTexture("gPosWS")->getSRV(), renderData.getTexture("tl_FirstPosWS")->getRTV());

            // Copy to texture
            pRenderContext->blit(renderData.getTexture("gNormalWS")->getSRV(), mFirstLayerGbuffer.mpNormWS->getRTV());
            pRenderContext->blit(renderData.getTexture("gDiffOpacity")->getSRV(), mFirstLayerGbuffer.mpDiffOpacity->getRTV());
            pRenderContext->blit(renderData.getTexture("gPosWS")->getSRV(), mFirstLayerGbuffer.mpPosWS->getRTV());
            pRenderContext->blit(renderData.getTexture("gDepth")->getSRV(), mFirstLayerGbuffer.mpDepth->getRTV());

            pRenderContext->blit(renderData.getTexture("tl_SecondNormWS")->getSRV(), mSecondLayerGbuffer.mpNormWS->getRTV());
            pRenderContext->blit(renderData.getTexture("tl_SecondDiffOpacity")->getSRV(), mSecondLayerGbuffer.mpDiffOpacity->getRTV());
            pRenderContext->blit(renderData.getTexture("tl_SecondPosWS")->getSRV(), mSecondLayerGbuffer.mpPosWS->getRTV());


        }

        else {

            if(mMode == 1) {

                auto curDim = renderData.getDefaultTextureDims();


                // Vairables
                {
                    mWarpGbufferPass.pVars["PerFrameCB"]["gFrameDim"] = curDim;
                    mWarpGbufferPass.pVars["PerFrameCB"]["gCenterViewProjMat"] = mCenterMatrix;
                    // mWarpGbufferPass.pVars["PerFrameCB"]["gCurEps"] = mCurEps;
                }


                {
                    // Textures
                    auto pLinearZMip = mpLinearZBuffer->getSRV();
                    mWarpGbufferPass.pVars["gLinearZ"].setSrv(pLinearZMip);

                    auto pFirstNormWSMip = mFirstLayerGbuffer.mpNormWS->getSRV();
                    mWarpGbufferPass.pVars["gFirstLayerNormWS"].setSrv(pFirstNormWSMip);

                    auto pFirstDiffOpacityMip = mFirstLayerGbuffer.mpDiffOpacity->getSRV();
                    mWarpGbufferPass.pVars["gFirstLayerDiffOpacity"].setSrv(pFirstDiffOpacityMip);

                    auto pSecondNormWSMip = mSecondLayerGbuffer.mpNormWS->getSRV();
                    mWarpGbufferPass.pVars["gSecondLayerNormWS"].setSrv(pSecondNormWSMip);

                    auto pSecondDiffOpacityMip = mSecondLayerGbuffer.mpDiffOpacity->getSRV();
                    mWarpGbufferPass.pVars["gSecondLayerDiffOpacity"].setSrv(pSecondDiffOpacityMip);
                }

                mWarpGbufferPass.pFbo->attachColorTarget(renderData.getTexture("tl_Debug"), 0);
                mWarpGbufferPass.pFbo->attachColorTarget(renderData.getTexture("tl_Mask"), 1);
                mWarpGbufferPass.pFbo->attachColorTarget(renderData.getTexture("tl_FirstNormWS"), 2);
                mWarpGbufferPass.pFbo->attachColorTarget(renderData.getTexture("tl_FirstDiffOpacity"), 3);
                mWarpGbufferPass.pFbo->attachDepthStencilTarget(renderData.getTexture("gDepth"));
                pRenderContext->clearFbo(mWarpGbufferPass.pFbo.get(), float4(0, 0, 0, 1), 1.0f,
                                        0, FboAttachmentType::All);
                mWarpGbufferPass.pGraphicsState->setFbo(mWarpGbufferPass.pFbo);
                // Rasterize it!
                mpScene->rasterize(pRenderContext, mWarpGbufferPass.pGraphicsState.get(),
                                mWarpGbufferPass.pVars.get(),
                                RasterizerState::CullMode::None);

            }

            else if (mMode == 2) {


                auto curDim = renderData.getDefaultTextureDims();
                auto curViewProjMat = mpScene->getCamera()->getViewProjMatrix();

                // Create Depth Buffer
                {
                    createNewTexture(mProjFirstLayer.mpDepthTest, curDim, ResourceFormat::R32Uint);
                    createNewTexture(mProjFirstLayer.mpNormWS, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mProjFirstLayer.mpDiffOpacity, curDim, ResourceFormat::RGBA32Float);


                    createNewTexture(mProjSecondLayer.mpDepthTest, curDim, ResourceFormat::R32Uint);
                    createNewTexture(mProjSecondLayer.mpNormWS, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mProjSecondLayer.mpDiffOpacity, curDim, ResourceFormat::RGBA32Float);

                    createNewTexture(mMergedLayer.mpMask, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mMergedLayer.mpNormWS, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mMergedLayer.mpDiffOpacity, curDim, ResourceFormat::RGBA32Float);
                }


                // ------------------------------ Depth Test ------------------------------ //

                {

                    // Varibales
                    {
                        mpProjectionDepthTestPass["PerFrameCB"]["gFrameDim"] = curDim;
                        mpProjectionDepthTestPass["PerFrameCB"]["gCenterViewProjMat"] = mCenterMatrix;
                        mpProjectionDepthTestPass["PerFrameCB"]["gCenterViewProjMatInv"] = mCenterMatrixInv;
                        mpProjectionDepthTestPass["PerFrameCB"]["gCurViewProjMat"] = curViewProjMat;
                        mpProjectionDepthTestPass["PerFrameCB"]["subSampleNum"] = mSubPixelSample;
                    }

                    // Input Textures
                    {
                        // Textures

                        auto pFirstPosWSMip = mFirstLayerGbuffer.mpPosWS->getSRV();
                        mpProjectionDepthTestPass["gFirstLayerPosWS"].setSrv(pFirstPosWSMip);

                        auto pSecondPosWSMip = mSecondLayerGbuffer.mpPosWS->getSRV();
                        mpProjectionDepthTestPass["gSecondLayerPosWS"].setSrv(pSecondPosWSMip);

                        auto pFirstDepthMip = mFirstLayerGbuffer.mpDepth->getSRV();
                        mpProjectionDepthTestPass["gFirstDepth"].setSrv(pFirstDepthMip);

                        auto pSecondDepthMip = mSecondLayerGbuffer.mpDepth->getSRV();
                        mpProjectionDepthTestPass["gSecondDepth"].setSrv(pSecondDepthMip);

                    }

                    // Output Textures
                    {

                        auto pFirstDepthTestUAV = mProjFirstLayer.mpDepthTest->getUAV();
                        pRenderContext->clearUAV(pFirstDepthTestUAV.get(), uint4(-1));
                        mpProjectionDepthTestPass["gProjFirstLayerDepthTest"].setUav(pFirstDepthTestUAV);

                        auto pSecondDepthTestUAV = mProjSecondLayer.mpDepthTest->getUAV();
                        pRenderContext->clearUAV(pSecondDepthTestUAV.get(), uint4(-1));
                        mpProjectionDepthTestPass["gProjSecondLayerDepthTest"].setUav(pSecondDepthTestUAV);

                    }

                    // Run Forward Warping Shader
                    mpProjectionDepthTestPass->execute(pRenderContext, uint3(curDim, 1));

                    // Set barriers
                    {

                        pRenderContext->uavBarrier(mProjFirstLayer.mpDepthTest.get());
                        pRenderContext->uavBarrier(mProjSecondLayer.mpDepthTest.get());

                    }

                }


                // ------------------------------ Forward Warping ------------------------------ //

                {
                    // Varibales
                    {
                        mpForwardWarpPass["PerFrameCB"]["gFrameDim"] = curDim;
                        mpForwardWarpPass["PerFrameCB"]["gCenterViewProjMat"] = mCenterMatrix;
                        mpForwardWarpPass["PerFrameCB"]["gCenterViewProjMatInv"] = mCenterMatrixInv;
                        mpForwardWarpPass["PerFrameCB"]["gCurViewProjMat"] = curViewProjMat;
                        mpForwardWarpPass["PerFrameCB"]["subSampleNum"] = mSubPixelSample;
                    }

                    // Input Textures
                    {
                        // Textures

                        auto pFirstNormWSMip = mFirstLayerGbuffer.mpNormWS->getSRV();
                        mpForwardWarpPass["gFirstLayerNormWS"].setSrv(pFirstNormWSMip);

                        auto pFirstDiffOpacityMip = mFirstLayerGbuffer.mpDiffOpacity->getSRV();
                        mpForwardWarpPass["gFirstLayerDiffOpacity"].setSrv(pFirstDiffOpacityMip);

                        auto pFirstPosWSMip = mFirstLayerGbuffer.mpPosWS->getSRV();
                        mpForwardWarpPass["gFirstLayerPosWS"].setSrv(pFirstPosWSMip);

                        auto pSecondNormWSMip = mSecondLayerGbuffer.mpNormWS->getSRV();
                        mpForwardWarpPass["gSecondLayerNormWS"].setSrv(pSecondNormWSMip);

                        auto pSecondDiffOpacityMip = mSecondLayerGbuffer.mpDiffOpacity->getSRV();
                        mpForwardWarpPass["gSecondLayerDiffOpacity"].setSrv(pSecondDiffOpacityMip);

                        auto pSecondPosWSMip = mSecondLayerGbuffer.mpPosWS->getSRV();
                        mpForwardWarpPass["gSecondLayerPosWS"].setSrv(pSecondPosWSMip);

                        auto pFirstDepthTestMip = mProjFirstLayer.mpDepthTest->getSRV();
                        mpForwardWarpPass["gProjFirstLayerDepthTest"].setSrv(pFirstDepthTestMip);

                        auto pSecondDepthTestMip = mProjSecondLayer.mpDepthTest->getSRV();
                        mpForwardWarpPass["gProjSecondLayerDepthTest"].setSrv(pSecondDepthTestMip);

                        auto pFirstDepthMip = mFirstLayerGbuffer.mpDepth->getSRV();
                        mpForwardWarpPass["gFirstDepth"].setSrv(pFirstDepthMip);

                        auto pSecondDepthMip = mSecondLayerGbuffer.mpDepth->getSRV();
                        mpForwardWarpPass["gSecondDepth"].setSrv(pSecondDepthMip);
                    }

                    // Output Textures
                    {

                        auto pFirstNormWSUAV = mProjFirstLayer.mpNormWS->getUAV();
                        pRenderContext->clearUAV(pFirstNormWSUAV.get(), float4(0.f));
                        mpForwardWarpPass["gProjFirstLayerNormWS"].setUav(pFirstNormWSUAV);

                        auto pFirstDiffOpacityUAV = mProjFirstLayer.mpDiffOpacity->getUAV();
                        pRenderContext->clearUAV(pFirstDiffOpacityUAV.get(), float4(0.f));
                        mpForwardWarpPass["gProjFirstLayerDiffOpacity"].setUav(pFirstDiffOpacityUAV);

                        auto pSecondNormWSUAV = mProjSecondLayer.mpNormWS->getUAV();
                        pRenderContext->clearUAV(pSecondNormWSUAV.get(), float4(0.f));
                        mpForwardWarpPass["gProjSecondLayerNormWS"].setUav(pSecondNormWSUAV);

                        auto pSecondDiffOpacityUAV = mProjSecondLayer.mpDiffOpacity->getUAV();
                        pRenderContext->clearUAV(pSecondDiffOpacityUAV.get(), float4(0.f));
                        mpForwardWarpPass["gProjSecondLayerDiffOpacity"].setUav(pSecondDiffOpacityUAV);

                    }

                    // Run Forward Warping Shader
                    mpForwardWarpPass->execute(pRenderContext, uint3(curDim, 1));

                    // Set barriers
                    {
                        pRenderContext->uavBarrier(mProjFirstLayer.mpNormWS.get());
                        pRenderContext->uavBarrier(mProjFirstLayer.mpDiffOpacity.get());

                        pRenderContext->uavBarrier(mProjSecondLayer.mpNormWS.get());
                        pRenderContext->uavBarrier(mProjSecondLayer.mpDiffOpacity.get());

                    }

                }

                // ---------------------------------- Merge Layers ---------------------------------------

                {
                    // Input Textures
                    {

                        mpMergeLayerPass["PerFrameCB"]["gNearestFilter"] = mNearestThreshold;
                        mpMergeLayerPass["PerFrameCB"]["gFrameDim"] = curDim;

                        auto pFirstDepthTestSRV = mProjFirstLayer.mpDepthTest->getSRV();
                        mpMergeLayerPass["gFirstLayerDepthTest"].setSrv(pFirstDepthTestSRV);

                        auto pFirstNormWSSRV = mProjFirstLayer.mpNormWS->getSRV();
                        mpMergeLayerPass["gFirstLayerNormWS"].setSrv(pFirstNormWSSRV);

                        auto pFirstDiffOpacitySRV = mProjFirstLayer.mpDiffOpacity->getSRV();
                        mpMergeLayerPass["gFirstLayerDiffOpacity"].setSrv(pFirstDiffOpacitySRV);

                        auto pSecondDepthTestSRV = mProjSecondLayer.mpDepthTest->getSRV();
                        mpMergeLayerPass["gSecondLayerDepthTest"].setSrv(pSecondDepthTestSRV);

                        auto pSecondNormWSSRV = mProjSecondLayer.mpNormWS->getSRV();
                        mpMergeLayerPass["gSecondLayerNormWS"].setSrv(pSecondNormWSSRV);

                        auto pSecondDiffOpacitySRV = mProjSecondLayer.mpDiffOpacity->getSRV();
                        mpMergeLayerPass["gSecondLayerDiffOpacity"].setSrv(pSecondDiffOpacitySRV);

                    }

                    // Output Textures
                    {

                        auto pNormWSUAV = mMergedLayer.mpNormWS->getUAV();
                        pRenderContext->clearUAV(pNormWSUAV.get(), float4(0.f));
                        mpMergeLayerPass["gNormWS"].setUav(pNormWSUAV);

                        auto pDiffOpacityUAV = mMergedLayer.mpDiffOpacity->getUAV();
                        pRenderContext->clearUAV(pDiffOpacityUAV.get(), float4(0.f));
                        mpMergeLayerPass["gDiffOpacity"].setUav(pDiffOpacityUAV);

                        auto pMaskUAV = mMergedLayer.mpMask->getUAV();
                        pRenderContext->clearUAV(pMaskUAV.get(), float4(0.f));
                        mpMergeLayerPass["gMask"].setUav(pMaskUAV);

                    }

                    mpMergeLayerPass->execute(pRenderContext, uint3(curDim, 1));

                    // Set Barriers
                    {
                        pRenderContext->uavBarrier(mMergedLayer.mpNormWS.get());
                        pRenderContext->uavBarrier(mMergedLayer.mpDiffOpacity.get());
                        pRenderContext->uavBarrier(mMergedLayer.mpMask.get());
                    }

                    // Copy Data
                    pRenderContext->blit(mProjFirstLayer.mpDepthTest ->getSRV(), renderData.getTexture("tl_Debug")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpNormWS->getSRV(), renderData.getTexture("tl_FirstNormWS")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpDiffOpacity->getSRV(), renderData.getTexture("tl_FirstDiffOpacity")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpMask->getSRV(), renderData.getTexture("tl_Mask")->getRTV());

                }

            }

        }

        mFrameCount += 1;

    }

}

void TwoLayeredGbuffers::renderUI(Gui::Widgets& widget)
{
    widget.slider("Eps", mEps, -1.0f, 5.0f);
    widget.slider("Back Face Culling Threshold", mNormalThreshold, -1.0f, 1.0f);

    Gui::DropdownList modeList;
    modeList.push_back(Gui::DropdownValue{0, "default"});
    modeList.push_back(Gui::DropdownValue{1, "warped gbuffers"});
    modeList.push_back(Gui::DropdownValue{2, "forward warped gbuffers"});

    widget.dropdown("Mode", modeList, mMode);

    widget.var<uint32_t>("Fresh Frequency", mFreshNum, 1);
    widget.var<uint>("Nearest Filling Dist", mNearestThreshold, 0);
    widget.var<uint>("Sub Pixel Sample", mSubPixelSample, 1);

    widget.checkbox("Max Depth Constraint", mMaxDepthContraint);
    widget.checkbox("Normal Constraint", mNormalConstraint);
}
