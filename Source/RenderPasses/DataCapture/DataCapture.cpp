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
    mDumpProbes = false;
    mFrameCount = -1;
    mFrameIdx = 0;
    mpScene = NULL;
    mLoadCams = false;
    mProbeDirIndex = 0;
    mSavingDir = "D:\\songyinwu\\Codes\\Data\\TestData";
    mCameraTrajPath = "D:\\songyinwu\\Codes\\Falcor\\media\\Arcade\\camera_path.txt";


    mProbeDirs.push_back(float3(0, 0, 1));
    mProbeDirs.push_back(float3(0, 0, -1));
    mProbeDirs.push_back(float3(1, 0, 0));
    mProbeDirs.push_back(float3(-1, 0, 0));
    mProbeDirs.push_back(float3(0, 1, 0));
    mProbeDirs.push_back(float3(0, -1, 0));

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


void DataCapture::LoadCamera()
{
    std::ifstream fin(mCameraTrajPath);

    mPositions.clear();
    mTargets.clear();
    mUps.clear();

    int num;
    fin >> num;
    float3 pos, tar, up;
    while (num--) {
        fin >> pos.x >> pos.y >> pos.z;
        fin >> tar.x >> tar.y >> tar.z;
        fin >> up.x >> up.y >> up.z;

        mPositions.push_back(pos);
        mTargets.push_back(tar);
        mUps.push_back(up);

    }

    fin.close();

}

void DataCapture::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    // auto& pTexture = renderData.getTexture("src");

    if (mpScene) {

        int accCount = 1024;
        float step = (float)0.1;

        Camera::SharedPtr camera = mpScene->getCamera();

        if (mDump) {

            if (!mLoadCams) {
                mLoadCams = true;
                LoadCamera();
            }

            if (mFrameCount == accCount) {
                DumpData(renderData, camera, mFrameIdx, mSavingDir);
                mFrameIdx += 1;
            }

            if (mFrameCount == -1 || mFrameCount == accCount) {

                if (mFrameCount == -1) mCurIndex = 0.;

                int prevIdx = (int)floor(mCurIndex);
                int nextIdx = prevIdx + 1;

                if (nextIdx > mPositions.size()-1) {
                    mDump = false;
                    mFrameCount = -1;
                    mFrameIdx = 0;
                    mCurIndex = 0.;
                    return;
                }

                float3 position = (mCurIndex - prevIdx) * mPositions[nextIdx] + (nextIdx - mCurIndex) * mPositions[prevIdx];
                float3 target = (mCurIndex - prevIdx) * mTargets[nextIdx] + (nextIdx - mCurIndex) * mTargets[prevIdx];
                float3 up = (mCurIndex - prevIdx) * mUps[nextIdx] + (nextIdx - mCurIndex) * mUps[prevIdx];

                camera->setPosition(position);
                camera->setTarget(target);
                camera->setUpVector(up);

                mFrameCount = 0;

                mCurIndex += step;

                // camera->setAspectRatio(1);

            }

            mFrameCount += 1;

        }

        if (mDumpProbes) {

            float3 position(-1.025815, 0.192328, -0.527774);

            if (mFrameCount == accCount) {
                DumpData(renderData, camera, mFrameIdx, mSavingDir);
                mFrameIdx += 1;
            }

            if (mFrameCount == -1 || mFrameCount == accCount) {


                if (mProbeDirIndex == 6) {
                    mDumpProbes = false;
                    mProbeDirIndex = 0;
                    return;
                }

                float3 target = position + mProbeDirs[mProbeDirIndex];
                float3 up = float3(0, 1, 0);

                if (mProbeDirIndex == 4 || mProbeDirIndex == 5) {
                    up = float3(0, 0, 1);
                }

                camera->setPosition(position);
                camera->setTarget(target);
                camera->setUpVector(up);

                mFrameCount = 0;

                mProbeDirIndex += 1;


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
    widget.checkbox("Dump Probe Data", mDumpProbes);
    widget.textbox("Saving Dir", mSavingDir);
}
