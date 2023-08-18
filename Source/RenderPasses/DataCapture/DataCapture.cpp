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
#include "DataCapture.h"
#include "RenderGraph/RenderPassLibrary.h"

const RenderPass::Info DataCapture::kInfo { "DataCapture", "Insert pass description here." };

// Don't remove this. it's required for hot-reload to function properly
extern "C" FALCOR_API_EXPORT const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" FALCOR_API_EXPORT void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerPass(DataCapture::kInfo, DataCapture::create);
}

DataCapture::SharedPtr DataCapture::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new DataCapture());
    return pPass;
}


void DataCapture::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    if (pScene) {
        mpScene = pScene;
    }
}

DataCapture::DataCapture() : RenderPass(kInfo) {
    mDump = false;
    mFrameCount = -1;
    mFrameIdx = 0;
    mpScene = NULL;
    mSavingDir = "D:\\songyinwu\\Codes\\Data\\TestData";
}

Dictionary DataCapture::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection DataCapture::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    //reflector.addOutput("dst");
    //reflector.addInput("src");


    reflector.addInput("color", "Path Traced Results")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();


    reflector.addInput("albedo", "Base Color")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addInput("normal", "Normal")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addOutput("empty", "empty")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget)
        .texture2D();

    return reflector;
}

void DataCapture::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    // auto& pTexture = renderData.getTexture("src");

    if (mpScene) {

        int accCount = 1024;


        float3 position(-1.14331, 1.84309, 2.44233);
        float3 target(-0.701423, 1.48637, 1.61924);
        float3 up(-0.376237, 0.634521, 0.675103);

        Camera::SharedPtr camera = mpScene->getCamera();

        if (mDump) {

            if (mFrameCount == accCount) {
                DumpData(renderData, camera, mFrameIdx, mSavingDir);
                mFrameIdx += 1;
            }

            if (mFrameCount == -1 || mFrameCount == accCount) {

                camera->setPosition(position);
                camera->setTarget(target);
                camera->setUpVector(up);

                mFrameCount = 0;

                // camera->setAspectRatio(1);

            }

            mFrameCount += 1;

        }
    }

}

void DataCapture::DumpData(const RenderData &renderdata, const Camera::SharedPtr &camera, uint frameIdx, const std::string dirPath)
{
    renderdata.getTexture("color")->captureToFile(0, 0, dirPath + "/color_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderdata.getTexture("albedo")->captureToFile(0, 0, dirPath + "/albedo_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);
    renderdata.getTexture("normal")->captureToFile(0, 0, dirPath + "/normal_" + std::to_string(frameIdx) + ".exr", Bitmap::FileFormat::ExrFile);

    std::ofstream fout(dirPath + "/camera_" + std::to_string(frameIdx) + ".txt");

    pybind11::dict d;
    d["position"] = camera->getPosition();
    d["target"] = camera->getTarget();
    d["up"] = camera->getUpVector();
    d["focalLength"] = camera->getFocalLength();
    d["focalDistance"] = camera->getFocalDistance();
    d["apertureRadius"] = camera->getApertureRadius();
    fout << pybind11::str(d) << std::endl;

    fout.close();
}

void DataCapture::renderUI(Gui::Widgets& widget)
{
    widget.checkbox("Dump Data", mDump);
    widget.textbox("Saving Dir", mSavingDir);
}
