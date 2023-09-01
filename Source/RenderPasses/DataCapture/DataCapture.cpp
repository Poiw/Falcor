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

        InitParams();
    }
}

void DataCapture::SetProbesPosition()
{

    if (mProbePositionFilePath != "") {
        std::ifstream fin(mProbePositionFilePath);

        mProbePositions.clear();

        int num;
        fin >> num;
        float3 pos;
        while (num--) {
            fin >> pos.x >> pos.y >> pos.z;
            mProbePositions.push_back(pos);
        }

        fin.close();

        return;
    }
    else {

        auto minPosition = mpScene->getSceneBounds().minPoint;
        auto maxPosition = mpScene->getSceneBounds().maxPoint;

        mProbePositions.clear();


        for (int i = 0; i < mProbeNumAxis; i++) {
            for (int j = 0; j < mProbeNumAxis; j++) {
                for (int k = 0; k < mProbeNumAxis; k++) {
                    float3 position = float3(
                        minPosition.x + (maxPosition.x - minPosition.x) * (i + 0.5) / mProbeNumAxis,
                        minPosition.y + (maxPosition.y - minPosition.y) * (j + 0.5) / mProbeNumAxis,
                        minPosition.z + (maxPosition.z - minPosition.z) * (k + 0.5) / mProbeNumAxis
                    );

                    mProbePositions.push_back(position);
                }
            }
        }

    }

}

void DataCapture::InitParams()
{
    mDump = false;
    mDumpProbes = false;
    mLoadProbes = false;
    mFrameCount = -1;
    mFrameIdx = 0;
    mProbeDirIndex = 0;
    mProbePosIndex = 0;
}

DataCapture::DataCapture() : RenderPass(kInfo) {
    mpScene = NULL;
    mLoadCams = false;
    mLoadProbes = false;
    mSavingDir = "";
    mCameraTrajPath = "";
    mProbePositionFilePath = "";

    mProbeNumAxis = 4;
    mNumPerPosition = 10;


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

    reflector.addInput("posW", "World Position")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::ShaderResource)
        .texture2D();

    reflector.addInput("viewW", "view direction in world space")
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
        float step = (float)1.0 / mNumPerPosition;

        Camera::SharedPtr camera = mpScene->getCamera();

        if (mDump) {

            if (!mLoadCams) {
                mLoadCams = true;
                LoadCamera();
            }

            if (mFrameCount == accCount) {
                DumpData(renderData, camera, std::to_string(mFrameIdx), mSavingDir);
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

            if (!mLoadProbes) {
                mLoadProbes = true;
                SetProbesPosition();
            }

            if (mFrameCount == accCount) {
                DumpData(renderData, camera, std::to_string(mProbePosIndex) + "_" + std::to_string(mProbeDirIndex), mSavingDir);
                mFrameIdx += 1;
            }

            if (mFrameCount == -1 || mFrameCount == accCount) {


                if (mProbeDirIndex == 6) {
                    mProbeDirIndex = 0;
                    mProbePosIndex += 1;

                    if (mProbePosIndex == mProbePositions.size()) {
                        mDumpProbes = false;
                        mFrameCount = -1;
                        mFrameIdx = 0;
                        mProbeDirIndex = 0;
                        mProbePosIndex = 0;
                        return;
                    }
                }

                float3 position = mProbePositions[mProbePosIndex];

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

void DataCapture::DumpData(const RenderData &renderdata, const Camera::SharedPtr &camera, const std::string &name_suffix, const std::string dirPath)
{
    renderdata.getTexture("color")->captureToFile(0, 0, dirPath + "/color_" + name_suffix + ".exr", Bitmap::FileFormat::ExrFile);
    renderdata.getTexture("albedo")->captureToFile(0, 0, dirPath + "/albedo_" + name_suffix + ".exr", Bitmap::FileFormat::ExrFile);
    renderdata.getTexture("normal")->captureToFile(0, 0, dirPath + "/normal_" + name_suffix + ".exr", Bitmap::FileFormat::ExrFile);
    renderdata.getTexture("posW")->captureToFile(0, 0, dirPath + "/posW_" + name_suffix + ".exr", Bitmap::FileFormat::ExrFile);
    renderdata.getTexture("viewW")->captureToFile(0, 0, dirPath + "/viewW_" + name_suffix + ".exr", Bitmap::FileFormat::ExrFile);

    std::ofstream fout(dirPath + "/camera_" + name_suffix + ".txt");

    pybind11::dict d;
    d["position"] = camera->getPosition();
    d["target"] = camera->getTarget();
    d["up"] = camera->getUpVector();
    d["focalLength"] = camera->getFocalLength();
    d["focalDistance"] = camera->getFocalDistance();
    d["apertureRadius"] = camera->getApertureRadius();
    fout << pybind11::str(d) << std::endl;

    fout.close();

    fout.open(dirPath + "/camera-viewproj_" + name_suffix + ".txt");
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) fout << camera->getViewProjMatrix()[i][j] << " ";

}

void DataCapture::renderUI(Gui::Widgets& widget)
{
    widget.checkbox("Dump Data", mDump);
    widget.checkbox("Dump Probe Data", mDumpProbes);
    if (widget.button("Clean states")) {
        InitParams();
    }
    widget.textbox("Saving Dir", mSavingDir);
    widget.textbox("Camera Traj Path File", mCameraTrajPath);
    widget.textbox("Probe Position Path File", mProbePositionFilePath);

    widget.var<uint>("Probe Num Axis", mProbeNumAxis, 1);
    widget.var<uint>("Num Per Position", mNumPerPosition, 1);

}
