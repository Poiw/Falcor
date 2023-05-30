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
#include "PreprocessDLSS.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info PreprocessDLSS::kInfo { "PreprocessDLSS", "Prepare data for DLSS." };

namespace {
const std::string preprocessShaderFilePath =
    "RenderPasses/PreprocessDLSS/preprocess.slang";
}

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(PreprocessDLSS::kInfo, PreprocessDLSS::create);
}

PreprocessDLSS::PreprocessDLSS() : RenderPass(kInfo)
{
    mStartLoading = false;
    mCurFrame = -1;
    mDataDir = "E:/Data/Bunker/train/seq1/Seq1";

    // mLoadDepth = true;
    mLoadBaseColor = false;
    mLoadNormal = false;

    mpBaseColorTex = NULL;
    mpNormalTex = NULL;

    mCurReso = uint2(960, 540);

    {
        Program::Desc desc;
        desc.addShaderLibrary(preprocessShaderFilePath).csEntry("csMain");
        Program::DefineList defines;
        mpPreprocessPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpPreprocessPass->setVars(nullptr);  // Trigger vars creation
    }

}

PreprocessDLSS::SharedPtr PreprocessDLSS::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new PreprocessDLSS());
    return pPass;
}

Dictionary PreprocessDLSS::getScriptingDictionary()
{
    return Dictionary();
}

std::string PreprocessDLSS::getMVPath(const std::string& dataDir, int frame)
{
    std::string frameString = std::to_string(frame);
    int precision = 4 - std::min((size_t)4, frameString.length());

    frameString.insert(0, precision, '0');

    std::string path = dataDir + "/MotionVector." + frameString + ".exr";
    return path;
}

std::string PreprocessDLSS::getDepthPath(const std::string& dataDir, int frame)
{
    std::string frameString = std::to_string(frame);
    int precision = 4 - std::min((size_t)4, frameString.length());

    frameString.insert(0, precision, '0');

    std::string path = dataDir + "/SceneDepth." + frameString + ".exr";
    return path;
}

std::string PreprocessDLSS::getColorPath(const std::string& dataDir, int frame)
{
    std::string frameString = std::to_string(frame);
    int precision = 4 - std::min((size_t)4, frameString.length());

    frameString.insert(0, precision, '0');

    std::string path = dataDir + "/PreTonemapHDRColor." + frameString + ".exr";
    return path;
}

std::string PreprocessDLSS::getBaseColorPath(const std::string& dataDir, int frame)
{
    std::string frameString = std::to_string(frame);
    int precision = 4 - std::min((size_t)4, frameString.length());

    frameString.insert(0, precision, '0');

    std::string path = dataDir + "/BaseColor." + frameString + ".exr";
    return path;
}

std::string PreprocessDLSS::getNormalPath(const std::string& dataDir, int frame)
{
    std::string frameString = std::to_string(frame);
    int precision = 4 - std::min((size_t)4, frameString.length());

    frameString.insert(0, precision, '0');

    std::string path = dataDir + "/WorldNormal." + frameString + ".exr";
    return path;
}

RenderPassReflection PreprocessDLSS::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    //reflector.addInput("src");
    //reflector.addOutput("dst");

    reflector.addOutput("depth", "depth")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D(mCurReso.x, mCurReso.y);


    reflector.addOutput("color", "color")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D(mCurReso.x, mCurReso.y);


    reflector.addOutput("mvec", "motion vector")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D(mCurReso.x, mCurReso.y);

    reflector.addOutput("normal", "normal")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D(mCurReso.x, mCurReso.y);

    reflector.addOutput("basecolor", "basecolor")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D(mCurReso.x, mCurReso.y);

    return reflector;
}

void PreprocessDLSS::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    // auto& pTexture = renderData.getTexture("src");


    if (mStartLoading) {
        if (mCurFrame == -1) {
            mCurFrame = mStartFrame;
        }

        {
            mpMVTex = Texture::createFromFile(getMVPath(mDataDir, mCurFrame), false, true);
            mpColorTex = Texture::createFromFile(getColorPath(mDataDir, mCurFrame), false, true);
            mpDepthTex = Texture::createFromFile(getDepthPath(mDataDir, mCurFrame), false, true);

            if (mLoadBaseColor) {
                mpBaseColorTex = Texture::createFromFile(getBaseColorPath(mDataDir, mCurFrame), false, true);
            }
            if (mLoadNormal) {
                mpNormalTex = Texture::createFromFile(getNormalPath(mDataDir, mCurFrame), false, true);
            }

            {
                if (mpBaseColorTex) pRenderContext->blit(mpBaseColorTex->getSRV(), renderData.getTexture("basecolor")->getRTV());
                if (mpNormalTex) pRenderContext->blit(mpNormalTex->getSRV(), renderData.getTexture("normal")->getRTV());
            }

            auto mCurDim = uint2(mpMVTex->getWidth(), mpMVTex->getHeight());

            if (mCurDim.x != mCurReso.x || mCurDim.y != mCurReso.y) {
                mCurReso = mCurDim;
                requestRecompile();
            }

            {
                mpPreprocessPass["PerFrameCB"]["gFrameDim"] = mCurDim;
            }

            {
                auto pMVSRV = mpMVTex->getSRV();
                mpPreprocessPass["gMotionVector"].setSrv(pMVSRV);

                auto pColorSRV = mpColorTex->getSRV();
                mpPreprocessPass["gColor"].setSrv(pColorSRV);

                auto pDepthSRV = mpDepthTex->getSRV();
                mpPreprocessPass["gDepth"].setSrv(pDepthSRV);
            }

            {
                auto pMVUAV = renderData.getTexture("mvec")->getUAV();
                pRenderContext->clearUAV(pMVUAV.get(), float4(0.f));
                mpPreprocessPass["gMotionVectorOut"].setUav(pMVUAV);

                auto pColorUAV = renderData.getTexture("color")->getUAV();
                pRenderContext->clearUAV(pColorUAV.get(), float4(0.f));
                mpPreprocessPass["gColorOut"].setUav(pColorUAV);

                auto pDepthUAV = renderData.getTexture("depth")->getUAV();
                pRenderContext->clearUAV(pDepthUAV.get(), float4(0.f));
                mpPreprocessPass["gDepthOut"].setUav(pDepthUAV);
            }

            mpPreprocessPass->execute(pRenderContext, uint3(mCurDim, 1));

            {
                pRenderContext->uavBarrier(renderData.getTexture("mvec").get());
                pRenderContext->uavBarrier(renderData.getTexture("color").get());
                pRenderContext->uavBarrier(renderData.getTexture("depth").get());
            }

            // pRenderContext->blit(mpColorTex->getSRV(), renderData.getTexture("color")->getRTV());
        }


        mCurFrame++;

        if (mCurFrame > mEndFrame) {
            mCurFrame = -1;
            mStartLoading = false;
        }

    }

}

void PreprocessDLSS::renderUI(Gui::Widgets& widget)
{

    widget.checkbox("Enable Load Imgs", mStartLoading);


    widget.checkbox("Load Base Color", mLoadBaseColor);
    // widget.checkbox("Load Depth", mLoadDepth);
    widget.checkbox("Load Normal", mLoadNormal);

    widget.var<int>("Start Frame", mStartFrame, 0);
    widget.var<int>("End Frame", mEndFrame, 0);


    widget.textbox("Data Dir", mDataDir);

}
