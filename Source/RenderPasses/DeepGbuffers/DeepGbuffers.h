/***************************************************************************
 # Copyright (c) 2015-21, NVIDIA CORPORATION. All rights reserved.
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
#pragma once
#include "Falcor.h"

using namespace Falcor;

class DeepGbuffers : public RenderPass
{
public:
    using SharedPtr = std::shared_ptr<DeepGbuffers>;

    static const Info kInfo;

    /** Create a new render pass object.
        \param[in] pRenderContext The render context.
        \param[in] dict Dictionary of serialized parameters.
        \return A new object, or an exception is thrown if creation failed.
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override {}
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual bool onMouseEvent(const MouseEvent& mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent& keyEvent) override { return false; }

private:
    DeepGbuffers();

    void createNewTexture(Texture::SharedPtr &pTex, const Falcor::uint2 &curDim, enum Falcor::ResourceFormat dataFormat, Falcor::Resource::BindFlags bindFlags, const uint arraySize);

    void InitVars();

    void CreateTextures(const uint2 &curDim);

    void GenGbuffers(RenderContext* pRenderContext, const RenderData& renderData);

    void GenGTGbuffers(RenderContext* pRenderContext, const RenderData& renderData);

    void WarpGbuffers(RenderContext* pRenderContext, const RenderData& renderData);

    Scene::SharedPtr mpScene;


    struct Gbuffers
    {
        Texture::SharedPtr pNormal;
        Texture::SharedPtr pAlbedo;
        Texture::SharedPtr pNextPosW;
        Texture::SharedPtr pLinearZ;
        Texture::SharedPtr pDepth;
    } mDeepGbuf, mGTGbuf, mCurGbuf;


    // Rasterization pass
    struct {
        GraphicsState::SharedPtr pGraphicsState;
        GraphicsVars::SharedPtr pVars;
        Fbo::SharedPtr pFbo;
    } mRasterPass;

    ComputePass::SharedPtr mpWarpPass = nullptr;
    ComputePass::SharedPtr mpWarpDepthTestPass = nullptr;

    float mThreshold;

    uint mFrameCount = 0;
    uint mGenFreq;

    uint mGbufferLevel = 5;

    uint mDepthTestScale = 65536;

    uint mSubpixelNum = 4;

    uint mDisplayMode = 0;


};
