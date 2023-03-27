//This source has been modified.

/************************************************************************************
Authors     :   Bruno Evangelista
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Licensed under the Oculus Master SDK License Version 1.0 (the "License");
you may not use the Oculus VR Rift SDK except in compliance with the License,
which is provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

You may obtain a copy of the License at
https://developer.oculus.com/licenses/oculusmastersdk-1.0

Unless required by applicable law or agreed to in writing, the Oculus VR SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

#include <functional>
#include <OVR_CAPI_D3D.h> //Oculus SDK.
#include "Win32_DirectXAppUtil.h" //DirectX Helper.

class GuardianSystemDemo
{
public:
    void Start(std::function<bool(ovrSession, uint64_t)> mainLoopCallback);
    void InitRenderTargets(const ovrHmdDesc& hmdDesc);
    void Render();

private:
    ovrSession mSession = nullptr;

    uint32_t mFrameIndex = 0;                                               // Global frame counter
    ovrPosef mHmdToEyePose[ovrEye_Count] = {};                              // Offset from the center of the HMD to each eye
    ovrRecti mEyeRenderViewport[ovrEye_Count] = {};                         // Eye render target viewport

    ovrLayerEyeFov mEyeRenderLayer = {};                                    // OVR  - Eye render layers description
    ovrTextureSwapChain mTextureChain[ovrEye_Count] = {};                   // OVR  - Eye render target swap chain
    ID3D11DepthStencilView* mEyeDepthTarget[ovrEye_Count] = {};             // DX11 - Eye depth view
    std::vector<ID3D11RenderTargetView*> mEyeRenderTargets[ovrEye_Count];   // DX11 - Eye render view

    bool mShouldQuit = ovrFalse;
};
