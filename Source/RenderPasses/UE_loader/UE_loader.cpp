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
#include "UE_loader.h"
#include "RenderGraph/RenderPassLibrary.h"
#include <fstream>

const RenderPass::Info UE_loader::kInfo { "UE_loader", "Load Unreal Engine Data" };

namespace {
const std::string processDataShaderPath=
    "RenderPasses/UE_loader/processData.slang";
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(UE_loader::kInfo, UE_loader::create);
}

UE_loader::UE_loader() : RenderPass(kInfo)
{
    mLoadData = false;
    mFolderPath = "F:/RemoteRenderingData/UE_Data/Bunker/Seq1";

    mpScene = nullptr;

    mStartFrame = 1;
    mEndFrame = 0;
    mCurFrame = 0;

    mpProcessDataPass = nullptr;

}

UE_loader::SharedPtr UE_loader::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new UE_loader());
    return pPass;
}


void UE_loader::clearVariable()
{
    mpColorTex = nullptr;
    mpDepthTex = nullptr;
    mpPosWTex = nullptr;
    mpMotionVectorTex = nullptr;
    mpPrevPosWTex = nullptr;

    mRescaleScene = false;

    mInputLinearZScale = 1.0;
}

void UE_loader::setComputeShader()
{
    // Create data process Shader
    {
        Program::Desc desc;
        desc.addShaderLibrary(processDataShaderPath).csEntry("csMain");
        Program::DefineList defines;
        defines.add(mpScene->getSceneDefines());
        mpProcessDataPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpProcessDataPass->setVars(nullptr);  // Trigger vars creation
    }
}

void UE_loader::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    if (pScene) {
        mpScene = pScene;

        clearVariable();

        setComputeShader();

    }
}

Dictionary UE_loader::getScriptingDictionary()
{
    return Dictionary();
}


std::string UE_loader::getFilePath(const std::string& folderPath, const std::string& fileName, const std::string& fileExt, int frame)
{
    std::string frameString = std::to_string(frame);
    int precision = 4 - std::min((size_t)4, frameString.length());

    frameString.insert(0, precision, '0');

    std::string filePath = folderPath + "/" + fileName + "." + frameString + "." + fileExt;
    return filePath;
}

void UE_loader::loadCamera(const std::string& cameraFilePath, Falcor::float2 frameDim)
{
    std::ifstream cameraFile(cameraFilePath);

    std::string name;

    Falcor::float3 cameraPos;
    Falcor::float3 cameraUp;
    Falcor::float3 cameraDir;

    float JitterX;
    float JitterY;

    float FoVX;


    while (cameraFile >> name) {

        if (name == "Origin:") {
            cameraFile >> cameraPos.x >> cameraPos.y >> cameraPos.z;
        }
        else if (name == "ViewUp:") {
            cameraFile >> cameraUp.x >> cameraUp.y >> cameraUp.z;
        }
        else if (name == "ViewDir:") {
            cameraFile >> cameraDir.x >> cameraDir.y >> cameraDir.z;
        }
        else if (name == "JitterX:") {
            cameraFile >> JitterX;
        }
        else if (name == "JitterY:") {
            cameraFile >> JitterY;
        }
        else if (name == "FOV:") {
            cameraFile >> FoVX;
        }
        // else if (name == "ViewProjectionMatrix:") {
        //     cameraFile >> mUEViewProjMat.data()[0] >> mUEViewProjMat.data()[1] >> mUEViewProjMat.data()[2] >> mUEViewProjMat.data()[3]
        //         >> mUEViewProjMat.data()[4] >> mUEViewProjMat.data()[5] >> mUEViewProjMat.data()[6] >> mUEViewProjMat.data()[7]
        //         >> mUEViewProjMat.data()[8] >> mUEViewProjMat.data()[9] >> mUEViewProjMat.data()[10] >> mUEViewProjMat.data()[11]
        //         >> mUEViewProjMat.data()[12] >> mUEViewProjMat.data()[13] >> mUEViewProjMat.data()[14] >> mUEViewProjMat.data()[15];
        // }

    }



    if (mRescaleScene) {
        cameraPos = (cameraPos - mSceneMin) / (mSceneMax - mSceneMin) * 2.0f - 1.0f;
    }

    Falcor::float3 cameraTarget = cameraPos + cameraDir;

    auto Camera = mpScene->getCamera();

    auto frameWidth = Camera->getFrameWidth();

    float focalLength = frameWidth / (2.0f * std::tan(0.5f * FoVX / 180.f * M_PI));

    Camera->setPosition(cameraPos);
    Camera->setTarget(cameraTarget);
    Camera->setUpVector(cameraUp);

    Camera->setFocalLength(focalLength);

    JitterX /= frameDim.x;
    JitterY /= frameDim.y;
    Camera->setJitter(JitterX, -JitterY); // This is closest to correct jitter


}

RenderPassReflection UE_loader::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    reflector.addOutput("PosW", "Current posW")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    reflector.addOutput("NextPosW", "Next PosW")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    reflector.addOutput("Color", "PreTonemapped Image")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    reflector.addOutput("MotionVector", "Motion Vector")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    reflector.addOutput("LinearZ", "LinearZ")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();
    return reflector;
}


void UE_loader::processData(RenderContext* pRenderContext, const RenderData& renderData)
{

    auto curDim = renderData.getDefaultTextureDims();

    if (mpPrevPosWTex == nullptr)
        mpPrevPosWTex = mpPosWTex;

    // ----------------------- Process Data -----------------------

    // std::cout << mpScene->getCamera()->getViewMatrix() << std::endl;
    // std::cout << mpScene->getCamera()->getProjMatrix() << std::endl;
    // std::cout << mpScene->getCamera()->getViewProjMatrix() << std::endl;

    // std::cout << "Projection matrix: " << mpScene->getCamera()->getProjMatrix() << std::endl;
    // std::cout <<  mpScene->getCamera()->getFrameHeight() / mpScene->getCamera()->getFocalLength() * 0.5f << std::endl;
    // std::cout <<  mpScene->getCamera()->getFrameWidth() / mpScene->getCamera()->getFocalLength() * 0.5f << std::endl;

    // Input
    {
        mpProcessDataPass["PerFrameCB"]["gFrameDim"] = curDim;
        // mpProcessDataPass["PerFrameCB"]["curViewProjMat"] = mpScene->getCamera()->getViewProjMatrix();
        // mpProcessDataPass["PerFrameCB"]["curViewProjMatInv"] = mpScene->getCamera()->getInvViewProjMatrix();
        mpProcessDataPass["PerFrameCB"]["prevViewProjMatNoJitter"] = mPrevViewProjMatNoJitter;
        mpProcessDataPass["PerFrameCB"]["curViewMatInv"] = rmcv::inverse(mpScene->getCamera()->getViewMatrix());
        mpProcessDataPass["PerFrameCB"]["tan2FovY"] = mpScene->getCamera()->getFrameHeight() / mpScene->getCamera()->getFocalLength() * 0.5f;
        mpProcessDataPass["PerFrameCB"]["tan2FovX"] = mpScene->getCamera()->getFrameWidth() / mpScene->getCamera()->getFocalLength() * 0.5f;
        mpProcessDataPass["PerFrameCB"]["rescaleScene"] = mRescaleScene;
        mpProcessDataPass["PerFrameCB"]["sceneMin"] = mSceneMin;
        mpProcessDataPass["PerFrameCB"]["sceneMax"] = mSceneMax;
        mpProcessDataPass["PerFrameCB"]["inputLinearZScale"] = mInputLinearZScale;

        auto curPosWSRV = mpPosWTex->getSRV();
        mpProcessDataPass["gCurPosWTex"].setSrv(curPosWSRV);

        auto curColorSRV = mpColorTex->getSRV();
        mpProcessDataPass["gCurColorTex"].setSrv(curColorSRV);

        auto curMotionVectorSRV = mpMotionVectorTex->getSRV();
        mpProcessDataPass["gCurMotionVectorTex"].setSrv(curMotionVectorSRV);

        auto curLinearZSRV = mpDepthTex->getSRV();
        mpProcessDataPass["gCurLinearZTex"].setSrv(curLinearZSRV);

        auto prevPosWSRV = mpPrevPosWTex->getSRV();
        mpProcessDataPass["gPrevPosWTex"].setSrv(prevPosWSRV);
    }

    // Output
    {
        auto nextPosWUAV = renderData.getTexture("NextPosW")->getUAV();
        pRenderContext->clearUAV(nextPosWUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
        mpProcessDataPass["gNextPosWTex"].setUav(nextPosWUAV);

        auto curPosWUAV = renderData.getTexture("PosW")->getUAV();
        pRenderContext->clearUAV(curPosWUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
        mpProcessDataPass["gPosWTex"].setUav(curPosWUAV);

        auto colorUAV = renderData.getTexture("Color")->getUAV();
        pRenderContext->clearUAV(colorUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
        mpProcessDataPass["gColorTex"].setUav(colorUAV);

        auto motionVectorUAV = renderData.getTexture("MotionVector")->getUAV();
        pRenderContext->clearUAV(motionVectorUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
        mpProcessDataPass["gMotionVectorTex"].setUav(motionVectorUAV);

        auto linearZUAV = renderData.getTexture("LinearZ")->getUAV();
        pRenderContext->clearUAV(linearZUAV.get(), float4(0.0f, 0.0f, 0.0f, 0.0f));
        mpProcessDataPass["gLinearZTex"].setUav(linearZUAV);

    }

    // Execute
    mpProcessDataPass->execute(pRenderContext, curDim.x, curDim.y);

    // Barrier
    {
        pRenderContext->uavBarrier(renderData.getTexture("NextPosW").get());
        pRenderContext->uavBarrier(renderData.getTexture("Color").get());
        pRenderContext->uavBarrier(renderData.getTexture("MotionVector").get());
        pRenderContext->uavBarrier(renderData.getTexture("LinearZ").get());
        pRenderContext->uavBarrier(renderData.getTexture("PosW").get());
    }


    // ------------------------------------------------------------


    pRenderContext->blit(renderData.getTexture("PosW")->getSRV(), mpPosWTex->getRTV());


    // Blit data
    // pRenderContext->blit(mpColorTex->getSRV(), renderData.getTexture("Color")->getRTV());
    // pRenderContext->blit(mpDepthTex->getSRV(), renderData.getTexture("LinearZ")->getRTV());
    // pRenderContext->blit(mpPosWTex->getSRV(), renderData.getTexture("PosW")->getRTV());
    // pRenderContext->blit(mpMotionVectorTex->getSRV(), renderData.getTexture("MotionVector")->getRTV());
}

void UE_loader::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (mLoadData)
    {
        // Load data
        std::string colorPath = getFilePath(mFolderPath, "PreTonemapHDRColor", "exr", mCurFrame);
        std::string depthPath = getFilePath(mFolderPath, "SceneDepth", "exr", mCurFrame);
        std::string posWPath = getFilePath(mFolderPath, "WorldPosition", "exr", mCurFrame);
        std::string motionVectorPath = getFilePath(mFolderPath, "MotionVector", "exr", mCurFrame);
        std::string cameraPath = getFilePath(mFolderPath, "CameraInfo", "txt", mCurFrame);

        mpColorTex = Texture::createFromFile(colorPath, false, true);
        mpDepthTex = Texture::createFromFile(depthPath, false, true);
        mpPosWTex = Texture::createFromFile(posWPath, false, true, Resource::BindFlags::AllColorViews);
        mpMotionVectorTex = Texture::createFromFile(motionVectorPath, false, true);

        loadCamera(cameraPath, renderData.getDefaultTextureDims());


        if (mCurFrame == mStartFrame) {
            mPrevViewProjMatNoJitter = mpScene->getCamera()->getViewProjMatrixNoJitter();
        }

        // Process data
        processData(pRenderContext, renderData);


        // Store previous data
        mpPrevPosWTex = mpPosWTex;

        // Update frame
        mCurFrame++;
        if (mCurFrame > mEndFrame)
        {
            clearVariable();
            mCurFrame = mStartFrame;
            mLoadData = false;
        }

        mPrevViewProjMatNoJitter = mpScene->getCamera()->getViewProjMatrixNoJitter();

    }

}

void UE_loader::renderUI(Gui::Widgets& widget)
{
    if (widget.button("Load Data"))
    {
        mLoadData = !mLoadData;

        if (mLoadData) {

            clearVariable();

            std::string infoFilePath = mFolderPath + "/info.txt";

            std::ifstream infoFile(infoFilePath);

            std::string name;

            while (infoFile >> name)
            {
                if (name == "Start:")
                {
                    infoFile >> mStartFrame;
                }
                else if (name == "End:")
                {
                    infoFile >> mEndFrame;
                }
                else if (name == "Min:")
                {
                    infoFile >> mSceneMin;
                }
                else if (name == "Max:")
                {
                    infoFile >> mSceneMax;
                    mRescaleScene = true;
                }
                else if (name == "InputLinearZScale:") {
                    infoFile >> mInputLinearZScale;
                }
            }

            mCurFrame = mStartFrame;
            // mEndFrame = mStartFrame + 1;
            infoFile.close();

        }
    }

    widget.checkbox("Rescale Scene", mRescaleScene);

    widget.textbox("Folder Path", mFolderPath);

    widget.text("Start Frame: " + std::to_string(mStartFrame));
    widget.text("End Frame: " + std::to_string(mEndFrame));
    widget.text("Cur Frame: " + std::to_string(mCurFrame));
    widget.text("Scene Min: " + std::to_string(mSceneMin));
    widget.text("Scene Max: " + std::to_string(mSceneMax));
    widget.text("Rescale Scene: " + std::to_string(mRescaleScene));
    widget.text("LinearZ Scale: " + std::to_string(mInputLinearZScale));
}
