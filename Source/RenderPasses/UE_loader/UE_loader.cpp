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

const RenderPass::Info UE_loader::kInfo { "UE_loader", "Insert pass description here." };

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
}

UE_loader::SharedPtr UE_loader::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new UE_loader());
    return pPass;
}

void UE_loader::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    if (pScene) {
        mpScene = pScene;
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

void UE_loader::loadCamera(const std::string& cameraFilePath)
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

    }

    Falcor::float3 cameraTarget = cameraPos + cameraDir;


    auto Camera = mpScene->getCamera();

    auto frameWidth = Camera->getFrameWidth(), frameHeight = Camera->getFrameHeight();

    float focalLength = frameWidth / (2.0f * std::tan(0.5f * FoVX));

    Camera->setPosition(cameraPos);
    Camera->setTarget(cameraTarget);
    Camera->setUpVector(cameraUp);

    Camera->setFocalLength(focalLength);

    JitterX /= frameWidth;
    JitterY /= frameHeight;
    Camera->setJitter(JitterX, JitterY);


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
        mpPosWTex = Texture::createFromFile(posWPath, false, true);
        mpMotionVectorTex = Texture::createFromFile(motionVectorPath, false, true);

        loadCamera(cameraPath);


        if (mCurFrame != mStartFrame)
        {



        }


        // Blit data
        pRenderContext->blit(mpColorTex->getSRV(), renderData.getTexture("Color")->getRTV());
        pRenderContext->blit(mpDepthTex->getSRV(), renderData.getTexture("LinearZ")->getRTV());
        pRenderContext->blit(mpPosWTex->getSRV(), renderData.getTexture("PosW")->getRTV());
        pRenderContext->blit(mpMotionVectorTex->getSRV(), renderData.getTexture("MotionVector")->getRTV());


        // Store previous data
        mpPrevPosWTex = mpPosWTex;

        // Update frame
        mCurFrame++;
        if (mCurFrame > mEndFrame)
        {
            mCurFrame = mStartFrame;
            mLoadData = false;
        }

    }

}

void UE_loader::renderUI(Gui::Widgets& widget)
{
    if (widget.button("Load Data"))
    {
        mLoadData = !mLoadData;

        if (mLoadData) {
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
            }

            mCurFrame = mStartFrame;
            infoFile.close();

        }
    }

    widget.textbox("Folder Path", mFolderPath);

    widget.text("Start Frame: " + std::to_string(mStartFrame));
    widget.text("End Frame: " + std::to_string(mEndFrame));
    widget.text("Cur Frame: " + std::to_string(mCurFrame));
}
