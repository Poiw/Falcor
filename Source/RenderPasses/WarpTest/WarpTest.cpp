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
#include "WarpTest.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info WarpTest::kInfo { "WarpTest", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(WarpTest::kInfo, WarpTest::create);
}

namespace {
const std::string warpShaderFilePath =
    "RenderPasses/WarpTest/warp.slang";
}  // namespace


WarpTest::SharedPtr WarpTest::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new WarpTest());
    return pPass;
}

WarpTest::WarpTest() : RenderPass(kInfo)
{
    mpPrevRender = NULL;
    mpPrevBaseColor = NULL;
    mpPrevWorldNormal = NULL;

    mCoordSigma = 10.f;
    mFeatureSigma = 0.005f;

    mStartWarp = false;

    // create a shading warping pass
    {
        Program::Desc desc;
        // desc.addShaderModules(mpScene->getShaderModules());
        desc.addShaderLibrary(warpShaderFilePath).csEntry("csMain");
        // desc.addTypeConformances(mpScene->getTypeConformances());
        Program::DefineList defines;
        mpWarpPass = ComputePass::create(desc, defines, false);
        // Bind the scene.
        mpWarpPass->setVars(nullptr);  // Trigger vars creation
    }


}

Dictionary WarpTest::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection WarpTest::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    reflector.addInput("render", "render")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addInput("basecolor", "basecolor")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addInput("normal", "normal")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addInput("motionvector", "motionvector")
        .format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addOutput("warped", "warped frame")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::AllColorViews)
        .texture2D();


    //reflector.addOutput("dst");
    //reflector.addInput("src");
    return reflector;
}

void WarpTest::createNewTexture(Texture::SharedPtr &pTex,
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

void WarpTest::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    // auto& pTexture = renderData.getTexture("src");



    auto curDim = uint2(renderData.getTexture("render")->getWidth(), renderData.getTexture("render")->getHeight());

    {
        createNewTexture(mpPrevRender, curDim);
        createNewTexture(mpPrevBaseColor, curDim);
        createNewTexture(mpPrevWorldNormal, curDim);
    }


    if (mStartWarp) {


        {

            mpWarpPass["PerFrameCB"]["gFrameDim"] = curDim;
            mpWarpPass["PerFrameCB"]["coord_sigma"] = mCoordSigma;
            mpWarpPass["PerFrameCB"]["feature_sigma"] = mFeatureSigma;

            auto pCurNormalSRV = renderData.getTexture("normal")->getSRV();
            mpWarpPass["gCurNormal"].setSrv(pCurNormalSRV);

            auto pCurBaseColorSRV = renderData.getTexture("basecolor")->getSRV();
            mpWarpPass["gCurBaseColor"].setSrv(pCurBaseColorSRV);

            auto pCurMotionVectorSRV = renderData.getTexture("motionvector")->getSRV();
            mpWarpPass["gCurMotionVector"].setSrv(pCurMotionVectorSRV);

            auto pCurRenderSRV = renderData.getTexture("render")->getSRV();
            mpWarpPass["gCurRender"].setSrv(pCurRenderSRV);

            auto pPrevNormalSRV = mpPrevWorldNormal->getSRV();
            mpWarpPass["gPrevNormal"].setSrv(pPrevNormalSRV);

            auto pPrevBaseColorSRV = mpPrevBaseColor->getSRV();
            mpWarpPass["gPrevBaseColor"].setSrv(pPrevBaseColorSRV);

            auto pPrevRenderSRV = mpPrevRender->getSRV();
            mpWarpPass["gPrevRender"].setSrv(pPrevRenderSRV);

        }

        {
            auto pWarpedUAV = renderData.getTexture("warped")->getUAV();
            pRenderContext->clearUAV(pWarpedUAV.get(), float4(0.0f));
            mpWarpPass["gWarped"].setUav(pWarpedUAV);
        }

        mpWarpPass->execute(pRenderContext, uint3(curDim, 1));

        {
            pRenderContext->uavBarrier(renderData.getTexture("warped").get());
        }


    }


    pRenderContext->blit(renderData.getTexture("render")->getSRV(), mpPrevRender->getRTV());
    pRenderContext->blit(renderData.getTexture("basecolor")->getSRV(), mpPrevBaseColor->getRTV());
    pRenderContext->blit(renderData.getTexture("normal")->getSRV(), mpPrevWorldNormal->getRTV());


}

void WarpTest::renderUI(Gui::Widgets& widget)
{


    widget.var<float>("Normal Sigma", mFeatureSigma, 0);
    widget.var<float>("Coord Sigma", mCoordSigma, 0);

    widget.checkbox("Start Warp", mStartWarp);
}
