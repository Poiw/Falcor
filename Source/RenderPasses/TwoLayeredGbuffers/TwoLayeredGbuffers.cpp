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
const std::string additionalGbufferPassShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/AdditionalGbuffer.slang";
const std::string additionalGbufferCopyShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/AdditionalGbufferCopy.slang";
const std::string additionalGbufferCopyDepthShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/AdditionalGbufferCopyDepth.slang";
const std::string projectionDepthTestShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/ProjectionDepthTest.slang";
const std::string calculateCurrentPosWSShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/CalculateCurrentPosWS.slang";
const std::string forwardWarpGbufferShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/ForwardWarpGbuffer.slang";
const std::string mergeLayersShaderFilePath =
    "RenderPasses/TwoLayeredGbuffers/MergeLayers.slang";
// const std::string shadingWarpingShaderFilePath =
//     "RenderPasses/TwoLayeredGbuffers/ShadingWarping.slang";
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

    {
        // Initialize raster graphics state
        mAddtionalGbufferPass.pGraphicsState = GraphicsState::create();
        mAddtionalGbufferPass.pGraphicsState->setRasterizerState(
            RasterizerState::create(rasterDesc));
        mAddtionalGbufferPass.pGraphicsState->setDepthStencilState(
            DepthStencilState::create(dsDesc));
        // Create framebuffer
        mAddtionalGbufferPass.pFbo = Fbo::create();
    }


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
    mFirstLayerGbuffer.mpInstanceID = nullptr;
    mFirstLayerGbuffer.mpPosL = nullptr;

    mSecondLayerGbuffer.mpPosWS = nullptr;
    mSecondLayerGbuffer.mpNormWS = nullptr;
    mSecondLayerGbuffer.mpDiffOpacity = nullptr;
    mSecondLayerGbuffer.mpInstanceID = nullptr;
    mSecondLayerGbuffer.mpPosL = nullptr;

    for (int i = 0; i < maxTexLevel; ++i) {
        mProjFirstLayer[i].mpDepthTest = nullptr;
        mProjFirstLayer[i].mpNormWS = nullptr;
        mProjFirstLayer[i].mpDiffOpacity = nullptr;
        mProjFirstLayer[i].mpPosWS = nullptr;
        mProjFirstLayer[i].mpPrevCoord = nullptr;

        mProjSecondLayer[i].mpDepthTest = nullptr;
        mProjSecondLayer[i].mpNormWS = nullptr;
        mProjSecondLayer[i].mpDiffOpacity = nullptr;
        mProjSecondLayer[i].mpPosWS = nullptr;
        mProjSecondLayer[i].mpPrevCoord = nullptr;
    }

    mMergedLayer.mpMask = nullptr;
    mMergedLayer.mpNormWS = nullptr;
    mMergedLayer.mpDiffOpacity = nullptr;
    mMergedLayer.mpPosWS = nullptr;
    mMergedLayer.mpPrevCoord = nullptr;
    mMergedLayer.mpRender = nullptr;

    mAdditionalGbuffer.mpPosWS = nullptr;
    mAdditionalGbuffer.mpNormWS = nullptr;
    mAdditionalGbuffer.mpDiffOpacity = nullptr;
    mAdditionalGbuffer.mpDepth = nullptr;
    mAdditionalGbuffer.mpProjDepth = nullptr;
    mAdditionalGbuffer.mpInstanceID = nullptr;
    mAdditionalGbuffer.mpPosL = nullptr;

    mpCenterRender = nullptr;

    mEps = 0.001f;

    mMode = 0;
    mNearestThreshold = 1;
    mSubPixelSample = 1;

    mNormalThreshold = 0.1f;
    mFrameCount = 0;
    mFreshNum = 8;
    mMaxDepthContraint = 0;
    mNormalConstraint = 1;
    mCenterRenderScale = 1.2f;

    mEnableSubPixel = false;
    mEnableAdatpiveRadius = true;
    mAdditionalCamNum = 5;

    mPrevPos = float3(0.0f, 0.0f, 0.0f);
    mAdditionalCamRadius = 1.0f;
    mAdditionalCamTarDist = 1.0f;
    mForwardMipLevel = 1;

    mCenterValid = false;
    mDumpData = false;

    mSavingDir = "C:/Users/songyin/Desktop/TwoLayered/Test";

    mRandomGen = std::uniform_real_distribution<float>(0, 1);
}

float TwoLayeredGbuffers::mGetRandom(float min, float max)
{
    return mRandomGen(mRng) * (max - min) + min;
}

void TwoLayeredGbuffers::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
     mRasterPass.pVars = nullptr;
     mWarpGbufferPass.pVars = nullptr;
     mTwoLayerGbufferGenPass.pVars = nullptr;
     mAddtionalGbufferPass.pVars = nullptr;

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

        // Create Additional Gbuffer Pass
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(additionalGbufferPassShaderFilePath)
                .vsEntry("vsMain")
                .psEntry("psMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            auto program =
                GraphicsProgram::create(desc, mpScene->getSceneDefines());
            mAddtionalGbufferPass.pGraphicsState->setProgram(program);
            mAddtionalGbufferPass.pVars = GraphicsVars::create(program.get());
        }


        // Create Additional Gbuffer Copy Pass
        {
            Program::Desc desc;
            // desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(additionalGbufferCopyShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpAdditionalGbufferCopyPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpAdditionalGbufferCopyPass->setVars(nullptr);  // Trigger vars creation
            // mpForwardWarpPass["gScene"] = mpScene->getParameterBlock();
        }

        // Create Additional Gbuffer Copy Pass
        {
            Program::Desc desc;
            // desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(additionalGbufferCopyDepthShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpAdditionalGbufferCopyDepthPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpAdditionalGbufferCopyDepthPass->setVars(nullptr);  // Trigger vars creation
            // mpForwardWarpPass["gScene"] = mpScene->getParameterBlock();
        }

        // Create a Forward Warping to calculate current world pos
        {
            Program::Desc desc;
            desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(calculateCurrentPosWSShaderFilePath).csEntry("csMain");
            desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpCalculateCurrentPosWSPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpCalculateCurrentPosWSPass->setVars(nullptr);  // Trigger vars creation
            mpCalculateCurrentPosWSPass["gScene"] = mpScene->getParameterBlock();
        }


        // Create a Forward Warping Depth Test Pass
        {
            Program::Desc desc;
            // desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(projectionDepthTestShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
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
            // desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(forwardWarpGbufferShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
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
            // desc.addShaderModules(mpScene->getShaderModules());
            desc.addShaderLibrary(mergeLayersShaderFilePath).csEntry("csMain");
            // desc.addTypeConformances(mpScene->getTypeConformances());
            Program::DefineList defines;
            defines.add(mpScene->getSceneDefines());
            mpMergeLayerPass = ComputePass::create(desc, defines, false);
            // Bind the scene.
            mpMergeLayerPass->setVars(nullptr);  // Trigger vars creation
            // mpForwardWarpPass["gScene"] = mpScene->getParameterBlock();
        }

        // // create a shading warping pass
        // {
        //     Program::Desc desc;
        //     desc.addShaderModules(mpScene->getShaderModules());
        //     desc.addShaderLibrary(shadingWarpingShaderFilePath).csEntry("csMain");
        //     desc.addTypeConformances(mpScene->getTypeConformances());
        //     Program::DefineList defines;
        //     defines.add(mpScene->getSceneDefines());
        //     mpShadingWarpingPass = ComputePass::create(desc, defines, false);
        //     // Bind the scene.
        //     mpShadingWarpingPass->setVars(nullptr);  // Trigger vars creation
        // }
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
    reflector.addInput("gPosL", "Local position")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();
    reflector.addInput("gRawInstanceID", "Raw Instance ID")
        .format(ResourceFormat::R32Uint)
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
    reflector.addInput("rPreTonemapped", "PreTonemapped Image")
        .format(ResourceFormat::RGBA32Float)
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
    reflector.addOutput("tl_FirstPrevCoord", "Coord for center images")
        .format(ResourceFormat::RG32Uint)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_FirstPreTonemap", "Pre Tonemapped Rendering")
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
    reflector.addOutput("tl_SecondPrevCoord", "Coord for center images")
        .format(ResourceFormat::RG32Uint)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    reflector.addOutput("tl_FrameCount", "Current Frame Count")
        .format(ResourceFormat::R32Uint)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D(1, 1);

    reflector.addOutput("tl_CenterDiffOpacity", "Center Diff Opactiy")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_CenterNormWS", "Center Norm WS")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_CenterPosWS", "Center Pos WS")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();
    reflector.addOutput("tl_CenterRender", "Center Render")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    return reflector;
}

void TwoLayeredGbuffers::createNewTexture(Texture::SharedPtr &pTex,
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

void TwoLayeredGbuffers::DumpDataFunc(const RenderData &renderData, uint frameIdx, const std::string dirPath)
{
    renderData.getTexture("tl_Mask")->captureToFile(0, 0, dirPath + "/Mask_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("tl_FirstNormWS")->captureToFile(0, 0, dirPath + "/Normal_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("tl_FirstDiffOpacity")->captureToFile(0, 0, dirPath + "/Albedo_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("tl_FirstPosWS")->captureToFile(0, 0, dirPath + "/PosWS_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);

    renderData.getTexture("gPosWS")->captureToFile(0, 0, dirPath + "/PosWS-GT_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("gNormalWS")->captureToFile(0, 0, dirPath + "/Normal-GT_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderData.getTexture("gDiffOpacity")->captureToFile(0, 0, dirPath + "/Albedo-GT_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);

    renderData.getTexture("rPreTonemapped")->captureToFile(0, 0, dirPath + "/Render-GT_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
}

void TwoLayeredGbuffers::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (mpScene == nullptr) return;

    // Falcor::Logger::log(Falcor::Logger::Level::Info, "Gbuffer: " + std::to_string(mFrameCount));

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

        // auto pFrameCountUAV = renderData.getTexture("tl_FrameCount")->getUAV();
        // pRenderContext->clearUAV(pFrameCountUAV.get(), uint4(mFrameCount % mFreshNum));

        uint tempV = mFrameCount % mFreshNum;
        Texture::SharedPtr Temp = nullptr;
        Temp = Texture::create2D(1, 1, ResourceFormat::R32Uint,
                                    1U, 1, &tempV,
                                    Resource::BindFlags::RenderTarget);
        pRenderContext->blit(Temp->getSRV(), renderData.getTexture("tl_FrameCount")->getRTV());

        if (mFrameCount % mFreshNum == 0) {

            mCenterValid = true;

            auto curDim = renderData.getDefaultTextureDims();

            {
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["gEps"] = mEps;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["gNormalThreshold"] = mNormalThreshold;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["maxConstraint"] = mMaxDepthContraint;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["normalConstraint"] = mNormalConstraint;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["curDim"] = curDim;
                mTwoLayerGbufferGenPass.pVars["PerFrameCB"]["renderScale"] = mCenterRenderScale;
                // mCurEps = mEps;
            }

            // Save the projection matrix
            mCenterMatrix = mpScene->getCamera()->getViewProjMatrix();
            mCenterMatrixInv = mpScene->getCamera()->getInvViewProjMatrix();

            auto speed = mpScene->getCamera()->getPosition() - mPrevPos;

            Falcor::Logger::log(Falcor::Logger::Level::Info, "Current Camera Speed: "
                                + Falcor::to_string(mpScene->getCamera()->getPosition() - mPrevPos) + " "
                                + Falcor::to_string((float16_t)glm::length(mpScene->getCamera()->getPosition() - mPrevPos)));
            Falcor::Logger::log(Falcor::Logger::Level::Info, "Target - CamPos: "
                                + Falcor::to_string(mpScene->getCamera()->getPosition() - mpScene->getCamera()->getTarget()));
            Falcor::Logger::log(Falcor::Logger::Level::Info, "Camera Speed: "
                                + std::to_string(mpScene->getCameraSpeed()));
            mPrevPos = mpScene->getCamera()->getPosition();


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
                createNewTexture(mFirstLayerGbuffer.mpInstanceID, curDim, ResourceFormat::R32Uint);
                createNewTexture(mFirstLayerGbuffer.mpPosL, curDim);
                createNewTexture(mFirstLayerGbuffer.mpDepth, curDim, ResourceFormat::R32Float);

                createNewTexture(mpCenterRender, curDim);

                createNewTexture(mSecondLayerGbuffer.mpPosWS, curDim);
                createNewTexture(mSecondLayerGbuffer.mpNormWS, curDim);
                createNewTexture(mSecondLayerGbuffer.mpDiffOpacity, curDim);
                createNewTexture(mSecondLayerGbuffer.mpInstanceID, curDim, ResourceFormat::R32Uint);
                createNewTexture(mSecondLayerGbuffer.mpPosL, curDim);
                createNewTexture(mSecondLayerGbuffer.mpDepth, curDim, ResourceFormat::R32Float);

                createNewTexture(mAdditionalGbuffer.mpProjDepth, curDim, ResourceFormat::R32Float);
                createNewTexture(mAdditionalGbuffer.mpPosWS, curDim);
                createNewTexture(mAdditionalGbuffer.mpNormWS, curDim);
                createNewTexture(mAdditionalGbuffer.mpDiffOpacity, curDim);
                createNewTexture(mAdditionalGbuffer.mpInstanceID, curDim, ResourceFormat::R32Uint);
                createNewTexture(mAdditionalGbuffer.mpPosL, curDim);
                createNewTexture(mAdditionalGbuffer.mpDepth, curDim, ResourceFormat::D32Float, Resource::BindFlags::DepthStencil);
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
            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(mSecondLayerGbuffer.mpInstanceID, 6);
            mTwoLayerGbufferGenPass.pFbo->attachColorTarget(mSecondLayerGbuffer.mpPosL, 7);

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
            pRenderContext->blit(renderData.getTexture("rPreTonemapped")->getSRV(), renderData.getTexture("tl_FirstPreTonemap")->getRTV());

            // Copy to texture
            pRenderContext->blit(renderData.getTexture("gNormalWS")->getSRV(), mFirstLayerGbuffer.mpNormWS->getRTV());
            pRenderContext->blit(renderData.getTexture("gDiffOpacity")->getSRV(), mFirstLayerGbuffer.mpDiffOpacity->getRTV());
            pRenderContext->blit(renderData.getTexture("gPosWS")->getSRV(), mFirstLayerGbuffer.mpPosWS->getRTV());
            pRenderContext->blit(renderData.getTexture("gDepth")->getSRV(), mFirstLayerGbuffer.mpDepth->getRTV());
            pRenderContext->blit(renderData.getTexture("gRawInstanceID")->getSRV(), mFirstLayerGbuffer.mpInstanceID->getRTV());
            pRenderContext->blit(renderData.getTexture("gPosL")->getSRV(), mFirstLayerGbuffer.mpPosL->getRTV());

            pRenderContext->blit(renderData.getTexture("rPreTonemapped")->getSRV(), mpCenterRender->getRTV());

            pRenderContext->blit(renderData.getTexture("tl_SecondNormWS")->getSRV(), mSecondLayerGbuffer.mpNormWS->getRTV());
            pRenderContext->blit(renderData.getTexture("tl_SecondDiffOpacity")->getSRV(), mSecondLayerGbuffer.mpDiffOpacity->getRTV());
            pRenderContext->blit(renderData.getTexture("tl_SecondPosWS")->getSRV(), mSecondLayerGbuffer.mpPosWS->getRTV());


            float preDefineX[] = {-1, 0, 1, 0};
            float preDefineY[] = {0, -1, 0, 1};


            for (int i = 0; i < mAdditionalCamNum; i++) {

                auto AdditionalCam = Camera::create("AdditionalCam");
                *AdditionalCam = *(mpScene->getCamera());

                auto dir = AdditionalCam->getTarget() - AdditionalCam->getPosition();
                // AdditionalCam->setTarget(AdditionalCam->getPosition() + glm::normalize(dir) * mAdditionalCamTarDist);
                auto base_x = glm::normalize(glm::cross(dir, AdditionalCam->getUpVector()));
                auto base_y = glm::normalize(glm::cross(dir, base_x));

                auto randomAngle = mGetRandom(0, 2. * glm::pi<float>());
                auto scale_ratio = mGetRandom(0, 1.0);


                // Falcor::Logger::log(Falcor::Logger::Level::Info, "Random Number: "
                //                     + Falcor::to_string((float16_t)randomAngle));

                float disRadius = mAdditionalCamRadius;
                if (mEnableAdatpiveRadius) disRadius = std::max<float>(glm::length(speed) * mAdditionalCamRadius, 0.4f);

                Falcor::Logger::log(Falcor::Logger::Level::Info, "Current Radius: "
                    + Falcor::to_string((float16_t)disRadius));

                float3 offset;
                if (i == 0) {
                    offset = speed;
                }
                else if (i < 5 && i > 0) {
                    offset = base_x * disRadius * preDefineX[i] + base_y * disRadius * preDefineY[i];
                }
                else {
                    offset = base_x * cos(randomAngle) * disRadius * scale_ratio + base_y * sin(randomAngle) * disRadius * scale_ratio;
                }

                auto newPos = AdditionalCam->getPosition() + offset;
                auto newTar = AdditionalCam->getTarget() + offset;


                // auto newPos = AdditionalCam->getPosition() + base_x * (randomAngle - glm::pi<float>());

                AdditionalCam->setPosition(newPos);
                AdditionalCam->setTarget(newTar);


                float4x4 AdditionalCamViewProjMat = AdditionalCam->getViewProjMatrix();
                float3 AdditionalCamPos = AdditionalCam->getPosition();


                {
                    // Rasterization
                    mAddtionalGbufferPass.pVars["PerFrameCB"]["gViewProjMat"] = AdditionalCamViewProjMat;
                    mAddtionalGbufferPass.pVars["PerFrameCB"]["gCamPos"] = AdditionalCamPos;

                    {
                        mAddtionalGbufferPass.pFbo->attachColorTarget(mAdditionalGbuffer.mpNormWS, 0);
                        mAddtionalGbufferPass.pFbo->attachColorTarget(mAdditionalGbuffer.mpDiffOpacity, 1);
                        mAddtionalGbufferPass.pFbo->attachColorTarget(mAdditionalGbuffer.mpPosWS, 2);
                        mAddtionalGbufferPass.pFbo->attachColorTarget(mAdditionalGbuffer.mpInstanceID, 3);
                        mAddtionalGbufferPass.pFbo->attachColorTarget(mAdditionalGbuffer.mpPosL, 4);
                        mAddtionalGbufferPass.pFbo->attachDepthStencilTarget(mAdditionalGbuffer.mpDepth);
                    }

                    pRenderContext->clearFbo(mAddtionalGbufferPass.pFbo.get(), float4(0, 0, 0, 1), 1.0f,
                                            0, FboAttachmentType::All);
                    mAddtionalGbufferPass.pGraphicsState->setFbo(mAddtionalGbufferPass.pFbo);

                    // Rasterize it!
                    mpScene->rasterize(pRenderContext, mAddtionalGbufferPass.pGraphicsState.get(),
                                    mAddtionalGbufferPass.pVars.get(),
                                    RasterizerState::CullMode::None);
                }

                {

                    // Depth Projection Test
                    {
                        mpAdditionalGbufferCopyDepthPass["PerFrameCB"]["gFrameDim"] = curDim;
                        mpAdditionalGbufferCopyDepthPass["PerFrameCB"]["gCenterViewProjMat"] = mCenterMatrix;
                        mpAdditionalGbufferCopyDepthPass["PerFrameCB"]["renderScale"] = mCenterRenderScale;
                    }

                    {
                        // Input Texture
                        auto pPosWSSRV = mAdditionalGbuffer.mpPosWS->getSRV();
                        mpAdditionalGbufferCopyDepthPass["gPosWS"].setSrv(pPosWSSRV);

                        auto pProjFirstLinearZSRV = renderData.getTexture("gLinearZ")->getSRV();
                        mpAdditionalGbufferCopyDepthPass["gProjFirstLinearZ"].setSrv(pProjFirstLinearZSRV);
                    }

                    {
                        // Output Texture
                        auto pProjDepthUAV = mAdditionalGbuffer.mpProjDepth->getUAV();
                        if (i == 0) {
                            pRenderContext->clearUAV(pProjDepthUAV.get(), uint4(0));
                        }
                        mpAdditionalGbufferCopyDepthPass["gProjDepthBuf"].setUav(pProjDepthUAV);
                    }

                    mpAdditionalGbufferCopyDepthPass->execute(pRenderContext, uint3(curDim, 1));

                    {
                        pRenderContext->uavBarrier(mAdditionalGbuffer.mpProjDepth.get());
                    }

                }



                {
                    // Project Additional Gbuffer
                    {
                        mpAdditionalGbufferCopyPass["PerFrameCB"]["gFrameDim"] = curDim;
                        mpAdditionalGbufferCopyPass["PerFrameCB"]["gCenterViewProjMat"] = mCenterMatrix;
                        mpAdditionalGbufferCopyPass["PerFrameCB"]["renderScale"] = mCenterRenderScale;
                    }

                    {
                        // Input Texture
                        auto pPosWSSRV = mAdditionalGbuffer.mpPosWS->getSRV();
                        mpAdditionalGbufferCopyPass["gPosWS"].setSrv(pPosWSSRV);

                        auto pNormWSSRV = mAdditionalGbuffer.mpNormWS->getSRV();
                        mpAdditionalGbufferCopyPass["gNormWS"].setSrv(pNormWSSRV);

                        auto pDiffOpacitySRV = mAdditionalGbuffer.mpDiffOpacity->getSRV();
                        mpAdditionalGbufferCopyPass["gDiffOpacity"].setSrv(pDiffOpacitySRV);

                        auto pInstanceIDSRV = mAdditionalGbuffer.mpInstanceID->getSRV();
                        mpAdditionalGbufferCopyPass["gInstanceID"].setSrv(pInstanceIDSRV);

                        auto pPosLSRV = mAdditionalGbuffer.mpPosL->getSRV();
                        mpAdditionalGbufferCopyPass["gPosL"].setSrv(pPosLSRV);

                        auto pProjDepthSRV = mAdditionalGbuffer.mpProjDepth->getSRV();
                        mpAdditionalGbufferCopyPass["gProjDepthBuf"].setSrv(pProjDepthSRV);
                    }

                    {
                        // Output Texture

                        auto pProjPosWSUAV = mSecondLayerGbuffer.mpPosWS->getUAV();
                        mpAdditionalGbufferCopyPass["gProjPosWS"].setUav(pProjPosWSUAV);

                        auto pProjNormWSUAV = mSecondLayerGbuffer.mpNormWS->getUAV();
                        mpAdditionalGbufferCopyPass["gProjNormWS"].setUav(pProjNormWSUAV);

                        auto pProjDiffOpacityUAV = mSecondLayerGbuffer.mpDiffOpacity->getUAV();
                        mpAdditionalGbufferCopyPass["gProjDiffOpacity"].setUav(pProjDiffOpacityUAV);

                        auto pProjInstanceIDUAV = mSecondLayerGbuffer.mpInstanceID->getUAV();
                        mpAdditionalGbufferCopyPass["gProjInstanceID"].setUav(pProjInstanceIDUAV);

                        auto pProjPosLUAV = mSecondLayerGbuffer.mpPosL->getUAV();
                        mpAdditionalGbufferCopyPass["gProjPosL"].setUav(pProjPosLUAV);

                    }

                    mpAdditionalGbufferCopyPass->execute(pRenderContext, uint3(curDim, 1));

                    {
                        pRenderContext->uavBarrier(mSecondLayerGbuffer.mpPosWS.get());
                        pRenderContext->uavBarrier(mSecondLayerGbuffer.mpNormWS.get());
                        pRenderContext->uavBarrier(mSecondLayerGbuffer.mpDiffOpacity.get());
                        pRenderContext->uavBarrier(mSecondLayerGbuffer.mpInstanceID.get());
                        pRenderContext->uavBarrier(mSecondLayerGbuffer.mpPosL.get());
                    }


                }

                pRenderContext->blit(mAdditionalGbuffer.mpNormWS->getSRV(), renderData.getTexture("tl_Debug")->getRTV());

            }



            pRenderContext->blit(mSecondLayerGbuffer.mpPosWS->getSRV(), renderData.getTexture("tl_SecondPosWS")->getRTV());
            pRenderContext->blit(mSecondLayerGbuffer.mpNormWS->getSRV(), renderData.getTexture("tl_SecondNormWS")->getRTV());
            pRenderContext->blit(mSecondLayerGbuffer.mpDiffOpacity->getSRV(), renderData.getTexture("tl_SecondDiffOpacity")->getRTV());

            pRenderContext->blit(mFirstLayerGbuffer.mpDiffOpacity->getSRV(), renderData.getTexture("tl_CenterDiffOpacity")->getRTV());
            pRenderContext->blit(mFirstLayerGbuffer.mpNormWS->getSRV(), renderData.getTexture("tl_CenterNormWS")->getRTV());
            pRenderContext->blit(mFirstLayerGbuffer.mpPosWS->getSRV(), renderData.getTexture("tl_CenterPosWS")->getRTV());
            pRenderContext->blit(mpCenterRender->getSRV(), renderData.getTexture("tl_CenterRender")->getRTV());

        }

        else {

            if (!mCenterValid) {
                mFrameCount++;
                return;
            }

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
                    for (int level = 0; level < maxTexLevel; ++level) {

                        auto texDim = curDim / (1u << (level));

                        createNewTexture(mProjFirstLayer[level].mpDepthTest, texDim, ResourceFormat::R32Uint);
                        createNewTexture(mProjFirstLayer[level].mpNormWS, texDim, ResourceFormat::RGBA32Float);
                        createNewTexture(mProjFirstLayer[level].mpDiffOpacity, texDim, ResourceFormat::RGBA32Float);
                        createNewTexture(mProjFirstLayer[level].mpPosWS, texDim, ResourceFormat::RGBA32Float);
                        createNewTexture(mProjFirstLayer[level].mpPrevCoord, texDim, ResourceFormat::RG32Uint);


                        createNewTexture(mProjSecondLayer[level].mpDepthTest, texDim, ResourceFormat::R32Uint);
                        createNewTexture(mProjSecondLayer[level].mpNormWS, texDim, ResourceFormat::RGBA32Float);
                        createNewTexture(mProjSecondLayer[level].mpDiffOpacity, texDim, ResourceFormat::RGBA32Float);
                        createNewTexture(mProjSecondLayer[level].mpPosWS, texDim, ResourceFormat::RGBA32Float);
                        createNewTexture(mProjSecondLayer[level].mpPrevCoord, texDim, ResourceFormat::RG32Uint);
                    }

                    createNewTexture(mMergedLayer.mpMask, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mMergedLayer.mpNormWS, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mMergedLayer.mpDiffOpacity, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mMergedLayer.mpPosWS, curDim, ResourceFormat::RGBA32Float);
                    createNewTexture(mMergedLayer.mpPrevCoord, curDim, ResourceFormat::RG32Uint);
                    createNewTexture(mMergedLayer.mpRender, curDim, ResourceFormat::RGBA32Float);
                }


                // ----------------------------- Calculate Current Depth ------------------------//

                {
                    {
                        mpCalculateCurrentPosWSPass["PerFrameCB"]["gFrameDim"] = curDim;
                    }

                    {

                        auto pFirstInstanceIDSRV = mFirstLayerGbuffer.mpInstanceID->getSRV();
                        mpCalculateCurrentPosWSPass["gFirstLayerInstanceID"].setSrv(pFirstInstanceIDSRV);

                        auto pFirstPosLSRV = mFirstLayerGbuffer.mpPosL->getSRV();
                        mpCalculateCurrentPosWSPass["gFirstLayerPosL"].setSrv(pFirstPosLSRV);

                        auto pSecondInstanceIDSRV = mSecondLayerGbuffer.mpInstanceID->getSRV();
                        mpCalculateCurrentPosWSPass["gSecondLayerInstanceID"].setSrv(pSecondInstanceIDSRV);

                        auto pSecondPosLSRV = mSecondLayerGbuffer.mpPosL->getSRV();
                        mpCalculateCurrentPosWSPass["gSecondLayerPosL"].setSrv(pSecondPosLSRV);
                    }

                    {
                        auto pFirstPosWSUAV = mFirstLayerGbuffer.mpPosWS->getUAV();
                        mpCalculateCurrentPosWSPass["gFirstLayerPosWS"].setUav(pFirstPosWSUAV);

                        auto pSecondPosWSUAV = mSecondLayerGbuffer.mpPosWS->getUAV();
                        mpCalculateCurrentPosWSPass["gSecondLayerPosWS"].setUav(pSecondPosWSUAV);
                    }

                    mpCalculateCurrentPosWSPass->execute(pRenderContext, uint3(curDim.x, curDim.y, 1));

                    {
                        pRenderContext->uavBarrier(mSecondLayerGbuffer.mpPosWS.get());
                    }

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
                        mpProjectionDepthTestPass["PerFrameCB"]["gEnableSubPixel"] = mEnableSubPixel;
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

                        for (int level = 0; level < maxTexLevel; ++level) {

                            auto pFirstDepthTestUAV = mProjFirstLayer[level].mpDepthTest->getUAV();
                            pRenderContext->clearUAV(pFirstDepthTestUAV.get(), uint4(-1));
                            mpProjectionDepthTestPass["gProjFirstLayerDepthTest" + std::to_string(level)].setUav(pFirstDepthTestUAV);

                            auto pSecondDepthTestUAV = mProjSecondLayer[level].mpDepthTest->getUAV();
                            pRenderContext->clearUAV(pSecondDepthTestUAV.get(), uint4(-1));
                            mpProjectionDepthTestPass["gProjSecondLayerDepthTest" + std::to_string(level)].setUav(pSecondDepthTestUAV);

                        }

                    }

                    // Run Forward Warping Shader
                    mpProjectionDepthTestPass->execute(pRenderContext, uint3(curDim, 1));

                    // Set barriers
                    {

                        for (int level = 0; level < maxTexLevel; ++level) {
                            pRenderContext->uavBarrier(mProjFirstLayer[level].mpDepthTest.get());
                            pRenderContext->uavBarrier(mProjSecondLayer[level].mpDepthTest.get());
                        }

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
                        mpForwardWarpPass["PerFrameCB"]["gEnableSubPixel"] = mEnableSubPixel;
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

                        auto pFirstDepthMip = mFirstLayerGbuffer.mpDepth->getSRV();
                        mpForwardWarpPass["gFirstDepth"].setSrv(pFirstDepthMip);

                        auto pSecondDepthMip = mSecondLayerGbuffer.mpDepth->getSRV();
                        mpForwardWarpPass["gSecondDepth"].setSrv(pSecondDepthMip);


                        for (int level = 0; level < maxTexLevel; ++level) {
                            auto pFirstDepthTestMip = mProjFirstLayer[level].mpDepthTest->getSRV();
                            mpForwardWarpPass["gProjFirstLayerDepthTest" + std::to_string(level)].setSrv(pFirstDepthTestMip);

                            auto pSecondDepthTestMip = mProjSecondLayer[level].mpDepthTest->getSRV();
                            mpForwardWarpPass["gProjSecondLayerDepthTest" + std::to_string(level)].setSrv(pSecondDepthTestMip);
                        }

                    }

                    // Output Textures
                    {


                        for (int level = 0; level < maxTexLevel; ++level) {

                            auto pFirstNormWSUAV = mProjFirstLayer[level].mpNormWS->getUAV();
                            pRenderContext->clearUAV(pFirstNormWSUAV.get(), float4(0.f));
                            mpForwardWarpPass["gProjFirstLayerNormWS" + std::to_string(level)].setUav(pFirstNormWSUAV);

                            auto pFirstDiffOpacityUAV = mProjFirstLayer[level].mpDiffOpacity->getUAV();
                            pRenderContext->clearUAV(pFirstDiffOpacityUAV.get(), float4(0.f));
                            mpForwardWarpPass["gProjFirstLayerDiffOpacity" + std::to_string(level)].setUav(pFirstDiffOpacityUAV);

                            auto pFirstPosWSUAV = mProjFirstLayer[level].mpPosWS->getUAV();
                            pRenderContext->clearUAV(pFirstPosWSUAV.get(), float4(0.f));
                            mpForwardWarpPass["gProjFirstLayerPosWS" + std::to_string(level)].setUav(pFirstPosWSUAV);

                            auto pFirstPrevCoordUAV = mProjFirstLayer[level].mpPrevCoord->getUAV();
                            pRenderContext->clearUAV(pFirstPrevCoordUAV.get(), uint4(100000));
                            mpForwardWarpPass["gProjFirstLayerPrevCoord" + std::to_string(level)].setUav(pFirstPrevCoordUAV);

                            auto pSecondNormWSUAV = mProjSecondLayer[level].mpNormWS->getUAV();
                            pRenderContext->clearUAV(pSecondNormWSUAV.get(), float4(0.f));
                            mpForwardWarpPass["gProjSecondLayerNormWS" + std::to_string(level)].setUav(pSecondNormWSUAV);

                            auto pSecondDiffOpacityUAV = mProjSecondLayer[level].mpDiffOpacity->getUAV();
                            pRenderContext->clearUAV(pSecondDiffOpacityUAV.get(), float4(0.f));
                            mpForwardWarpPass["gProjSecondLayerDiffOpacity" + std::to_string(level)].setUav(pSecondDiffOpacityUAV);

                            auto pSecondPosWSUAV = mProjSecondLayer[level].mpPosWS->getUAV();
                            pRenderContext->clearUAV(pSecondPosWSUAV.get(), float4(0.f));
                            mpForwardWarpPass["gProjSecondLayerPosWS" + std::to_string(level)].setUav(pSecondPosWSUAV);

                            auto pSecondPrevCoordUAV = mProjSecondLayer[level].mpPrevCoord->getUAV();
                            pRenderContext->clearUAV(pSecondPrevCoordUAV.get(), uint4(100000));
                            mpForwardWarpPass["gProjSecondLayerPrevCoord" + std::to_string(level)].setUav(pSecondPrevCoordUAV);

                        }

                    }

                    // Run Forward Warping Shader
                    mpForwardWarpPass->execute(pRenderContext, uint3(curDim, 1));

                    // Set barriers
                    {

                        for (int level = 0; level < maxTexLevel; ++level) {
                            pRenderContext->uavBarrier(mProjFirstLayer[level].mpNormWS.get());
                            pRenderContext->uavBarrier(mProjFirstLayer[level].mpDiffOpacity.get());
                            pRenderContext->uavBarrier(mProjFirstLayer[level].mpPosWS.get());
                            pRenderContext->uavBarrier(mProjFirstLayer[level].mpPrevCoord.get());

                            pRenderContext->uavBarrier(mProjSecondLayer[level].mpNormWS.get());
                            pRenderContext->uavBarrier(mProjSecondLayer[level].mpDiffOpacity.get());
                            pRenderContext->uavBarrier(mProjSecondLayer[level].mpPosWS.get());
                            pRenderContext->uavBarrier(mProjSecondLayer[level].mpPrevCoord.get());
                        }

                    }

                }

                // ---------------------------------- Merge Layers ---------------------------------------

                {
                    // Input Textures
                    {

                        mpMergeLayerPass["PerFrameCB"]["gNearestFilter"] = mNearestThreshold;
                        mpMergeLayerPass["PerFrameCB"]["gFrameDim"] = curDim;
                        mpMergeLayerPass["PerFrameCB"]["gUsedMipLevel"] = mForwardMipLevel;


                        for (int level = 0; level < maxTexLevel; ++level) {

                            auto pFirstDepthTestSRV = mProjFirstLayer[level].mpDepthTest->getSRV();
                            mpMergeLayerPass["gFirstLayerDepthTest" + std::to_string(level)].setSrv(pFirstDepthTestSRV);

                            auto pFirstNormWSSRV = mProjFirstLayer[level].mpNormWS->getSRV();
                            mpMergeLayerPass["gFirstLayerNormWS" + std::to_string(level)].setSrv(pFirstNormWSSRV);

                            auto pFirstDiffOpacitySRV = mProjFirstLayer[level].mpDiffOpacity->getSRV();
                            mpMergeLayerPass["gFirstLayerDiffOpacity" + std::to_string(level)].setSrv(pFirstDiffOpacitySRV);

                            auto pFirstPosWSSRV = mProjFirstLayer[level].mpPosWS->getSRV();
                            mpMergeLayerPass["gFirstLayerPosWS" + std::to_string(level)].setSrv(pFirstPosWSSRV);

                            auto pFirstPrevCoordSRV = mProjFirstLayer[level].mpPrevCoord->getSRV();
                            mpMergeLayerPass["gFirstLayerPrevCoord" + std::to_string(level)].setSrv(pFirstPrevCoordSRV);


                            auto pSecondDepthTestSRV = mProjSecondLayer[level].mpDepthTest->getSRV();
                            mpMergeLayerPass["gSecondLayerDepthTest" + std::to_string(level)].setSrv(pSecondDepthTestSRV);

                            auto pSecondNormWSSRV = mProjSecondLayer[level].mpNormWS->getSRV();
                            mpMergeLayerPass["gSecondLayerNormWS" + std::to_string(level)].setSrv(pSecondNormWSSRV);

                            auto pSecondDiffOpacitySRV = mProjSecondLayer[level].mpDiffOpacity->getSRV();
                            mpMergeLayerPass["gSecondLayerDiffOpacity" + std::to_string(level)].setSrv(pSecondDiffOpacitySRV);

                            auto pSecondPosWSSRV = mProjSecondLayer[level].mpPosWS->getSRV();
                            mpMergeLayerPass["gSecondLayerPosWS" + std::to_string(level)].setSrv(pSecondPosWSSRV);

                            auto pSecondPrevCoordSRV = mProjSecondLayer[level].mpPrevCoord->getSRV();
                            mpMergeLayerPass["gSecondLayerPrevCoord" + std::to_string(level)].setSrv(pSecondPrevCoordSRV);

                        }

                    }

                    // Output Textures
                    {

                        auto pNormWSUAV = mMergedLayer.mpNormWS->getUAV();
                        pRenderContext->clearUAV(pNormWSUAV.get(), float4(0.f));
                        mpMergeLayerPass["gNormWS"].setUav(pNormWSUAV);

                        auto pDiffOpacityUAV = mMergedLayer.mpDiffOpacity->getUAV();
                        pRenderContext->clearUAV(pDiffOpacityUAV.get(), float4(0.f));
                        mpMergeLayerPass["gDiffOpacity"].setUav(pDiffOpacityUAV);

                        auto pPosWSUAV = mMergedLayer.mpPosWS->getUAV();
                        pRenderContext->clearUAV(pPosWSUAV.get(), float4(0.f));
                        mpMergeLayerPass["gPosWS"].setUav(pPosWSUAV);

                        auto pPrevCoordUAV = mMergedLayer.mpPrevCoord->getUAV();
                        pRenderContext->clearUAV(pPrevCoordUAV.get(), uint4(100000));
                        mpMergeLayerPass["gPrevCoord"].setUav(pPrevCoordUAV);

                        auto pMaskUAV = mMergedLayer.mpMask->getUAV();
                        pRenderContext->clearUAV(pMaskUAV.get(), float4(0.f));
                        mpMergeLayerPass["gMask"].setUav(pMaskUAV);

                    }

                    mpMergeLayerPass->execute(pRenderContext, uint3(curDim, 1));

                    // Set Barriers
                    {
                        pRenderContext->uavBarrier(mMergedLayer.mpNormWS.get());
                        pRenderContext->uavBarrier(mMergedLayer.mpDiffOpacity.get());
                        pRenderContext->uavBarrier(mMergedLayer.mpPosWS.get());
                        pRenderContext->uavBarrier(mMergedLayer.mpPrevCoord.get());
                        pRenderContext->uavBarrier(mMergedLayer.mpMask.get());
                    }

                    // Copy Data
                    // pRenderContext->blit(mProjFirstLayer[0].mpDepthTest ->getSRV(), renderData.getTexture("tl_Debug")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpNormWS->getSRV(), renderData.getTexture("tl_FirstNormWS")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpDiffOpacity->getSRV(), renderData.getTexture("tl_FirstDiffOpacity")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpPosWS->getSRV(), renderData.getTexture("tl_FirstPosWS")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpPrevCoord->getSRV(), renderData.getTexture("tl_FirstPrevCoord")->getRTV());
                    pRenderContext->blit(mMergedLayer.mpMask->getSRV(), renderData.getTexture("tl_Mask")->getRTV());

                }


                // // ---------------------------------- Warp Shading ---------------------------------------
                // {
                //     // Input
                //     {


                //         mpShadingWarpingPass["PerFrameCB"]["gFrameDim"] = curDim;


                //         auto pCurDiffOpacitySRV = mMergedLayer.mpDiffOpacity->getSRV();
                //         mpShadingWarpingPass["gCurDiffOpacity"].setSrv(pCurDiffOpacitySRV);

                //         auto pCurPosWSSRV = mMergedLayer.mpPosWS->getSRV();
                //         mpShadingWarpingPass["gCurPosWS"].setSrv(pCurPosWSSRV);

                //         auto pCurNormWSSRV = mMergedLayer.mpNormWS->getSRV();
                //         mpShadingWarpingPass["gCurNormWS"].setSrv(pCurNormWSSRV);

                //         auto pCurPrevCoordSRV = mMergedLayer.mpPrevCoord->getSRV();
                //         mpShadingWarpingPass["gCurPrevCoord"].setSrv(pCurPrevCoordSRV);


                //         auto pCenterDiffOpacitySRV = mFirstLayerGbuffer.mpDiffOpacity->getSRV();
                //         mpShadingWarpingPass["gCenterDiffOpacity"].setSrv(pCenterDiffOpacitySRV);

                //         auto pCenterPosWSSRV = mFirstLayerGbuffer.mpPosWS->getSRV();
                //         mpShadingWarpingPass["gCenterPosWS"].setSrv(pCenterPosWSSRV);

                //         auto pCenterNormWSSRV = mFirstLayerGbuffer.mpNormWS->getSRV();
                //         mpShadingWarpingPass["gCenterNormWS"].setSrv(pCenterNormWSSRV);

                //         auto pCenterRenderSRV = mpCenterRender->getSRV();
                //         mpShadingWarpingPass["gCenterRender"].setSrv(pCenterRenderSRV);
                //     }

                //     // Output
                //     {
                //         auto pRenderUAV = mMergedLayer.mpRender->getUAV();
                //         pRenderContext->clearUAV(pRenderUAV.get(), float4(-1.f));
                //         mpShadingWarpingPass["gRender"].setUav(pRenderUAV);
                //     }

                //     mpShadingWarpingPass->execute(pRenderContext, uint3(curDim, 1));

                //     // Set Barriers
                //     {
                //         pRenderContext->uavBarrier(mMergedLayer.mpRender.get());
                //     }

                //     // Copy Data
                //     pRenderContext->blit(mMergedLayer.mpRender->getSRV(), renderData.getTexture("tl_FirstPreTonemap")->getRTV());
                // }

            }

        }

        if (mDumpData) {
            DumpDataFunc(renderData, mFrameCount, mSavingDir);
        }

    }

    mFrameCount += 1;

}

void TwoLayeredGbuffers::renderUI(Gui::Widgets& widget)
{
    widget.var<float>("Eps", mEps, -1.0f, 5.0f);
    widget.var<float>("Back Face Culling Threshold", mNormalThreshold, -1.0f, 1.0f);

    Gui::DropdownList modeList;
    modeList.push_back(Gui::DropdownValue{0, "default"});
    modeList.push_back(Gui::DropdownValue{1, "warped gbuffers"});
    modeList.push_back(Gui::DropdownValue{2, "forward warped gbuffers"});

    widget.dropdown("Mode", modeList, mMode);

    widget.var<uint32_t>("Fresh Frequency", mFreshNum, 1);
    widget.var<uint>("Nearest Filling Dist", mNearestThreshold, 0);
    widget.var<uint>("Sub Pixel Sample", mSubPixelSample, 1);
    widget.var<uint>("Additional Camera Number", mAdditionalCamNum, 0);
    widget.var<uint>("Forward Warping Mip Level", mForwardMipLevel, 0, 3);
    widget.var<float>("Additional Camera Radius", mAdditionalCamRadius, 0, 10);
    widget.var<float>("Additional Camera Target Distance", mAdditionalCamTarDist, 0, 10);
    widget.var<float>("Render Scale Factor", mCenterRenderScale, 1);

    widget.checkbox("Max Depth Constraint", mMaxDepthContraint);
    widget.checkbox("Normal Constraint", mNormalConstraint);
    widget.checkbox("Enable Subpixel Sampling in Forward Warping", mEnableSubPixel);
    widget.checkbox("Enable Adatptive Radius", mEnableAdatpiveRadius);

    widget.checkbox("Dump Data", mDumpData);
    widget.textbox("Saving Dir", mSavingDir);

}
