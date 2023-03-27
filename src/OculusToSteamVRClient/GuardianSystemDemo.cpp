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

#include "GuardianSystemDemo.h"

#pragma warning(disable: 4324)

#include <chrono>
#include <algorithm>

DirectX11 DIRECTX;
typedef std::chrono::time_point<std::chrono::high_resolution_clock> high_resolution_clock;
float randVelocity() { return (rand() % 201) * 0.01f - 1.0f; }
float randColor() { return (rand() % 81 + 20) * 0.01f; };

void GuardianSystemDemo::InitRenderTargets(const ovrHmdDesc& hmdDesc)
{
    // For each eye
    for (int i = 0; i < ovrEye_Count; ++i)
    {
        // Viewport
        const float kPixelsPerDisplayPixel = 1.0f;
        ovrSizei idealSize = ovr_GetFovTextureSize(mSession, (ovrEyeType)i, hmdDesc.DefaultEyeFov[i], kPixelsPerDisplayPixel);
        mEyeRenderViewport[i] = { 0, 0, idealSize.w, idealSize.h };

        // Create Swap Chain
        ovrTextureSwapChainDesc desc =
        {
            ovrTexture_2D, OVR_FORMAT_R8G8B8A8_UNORM_SRGB, 1, idealSize.w, idealSize.h, 1, 1,
            ovrFalse, ovrTextureMisc_DX_Typeless, ovrTextureBind_DX_RenderTarget
        };

        // Configure Eye render layers
        mEyeRenderLayer.Header.Type = ovrLayerType_EyeFov;
        mEyeRenderLayer.Viewport[i] = mEyeRenderViewport[i];
        mEyeRenderLayer.Fov[i] = hmdDesc.DefaultEyeFov[i];
        mHmdToEyePose[i] = ovr_GetRenderDesc(mSession, (ovrEyeType)i, hmdDesc.DefaultEyeFov[i]).HmdToEyePose;

        // DirectX 11 - Generate RenderTargetView from textures in swap chain
        // ----------------------------------------------------------------------
        ovrResult result = ovr_CreateTextureSwapChainDX(mSession, DIRECTX.Device, &desc, &mTextureChain[i]);
        if (!OVR_SUCCESS(result))
        {
            printf("ovr_CreateTextureSwapChainDX failed"); exit(-1);
        }

        // Render Target, normally triple-buffered
        int textureCount = 0;
        ovr_GetTextureSwapChainLength(mSession, mTextureChain[i], &textureCount);
        for (int j = 0; j < textureCount; ++j)
        {
            ID3D11Texture2D* renderTexture = nullptr;
            ovr_GetTextureSwapChainBufferDX(mSession, mTextureChain[i], j, IID_PPV_ARGS(&renderTexture));

            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc =
            {
                DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_RTV_DIMENSION_TEXTURE2D
            };

            ID3D11RenderTargetView* renderTargetView = nullptr;
            DIRECTX.Device->CreateRenderTargetView(renderTexture, &renderTargetViewDesc, &renderTargetView);
            mEyeRenderTargets[i].push_back(renderTargetView);
            renderTexture->Release();
        }

        // DirectX 11 - Generate Depth
        // ----------------------------------------------------------------------
        D3D11_TEXTURE2D_DESC depthTextureDesc =
        {
            (UINT)idealSize.w, (UINT)idealSize.h, 1, 1, DXGI_FORMAT_D32_FLOAT, {1, 0},
            D3D11_USAGE_DEFAULT, D3D11_BIND_DEPTH_STENCIL, 0, 0
        };

        ID3D11Texture2D* depthTexture = nullptr;
        DIRECTX.Device->CreateTexture2D(&depthTextureDesc, NULL, &depthTexture);
        DIRECTX.Device->CreateDepthStencilView(depthTexture, NULL, &mEyeDepthTarget[i]);
        depthTexture->Release();
    }
}

void GuardianSystemDemo::Start(std::function<bool(ovrSession, uint64_t)> mainLoopCallback)
{
    ovrResult result;
    result = ovr_Initialize(nullptr);
    if (!OVR_SUCCESS(result))
    {
        printf("ovr_Initialize failed");
		//TODO: Change these exits to returns.
        exit(-1);
    }

    ovrGraphicsLuid luid;
    result = ovr_Create(&mSession, &luid);
    if (!OVR_SUCCESS(result))
    {
        printf("ovr_Create failed");
        exit(-1);
    }

    if (!DIRECTX.InitWindow(0/*hinst*/, L"GuardianSystemDemo"))
    {
        printf("DIRECTX.InitWindow failed");
        exit(-1);
    }

    // Use HMD desc to initialize device
    ovrHmdDesc hmdDesc = ovr_GetHmdDesc(mSession);
    if (!DIRECTX.InitDevice(hmdDesc.Resolution.w / 2, hmdDesc.Resolution.h / 2, reinterpret_cast<LUID*>(&luid)))
    {
        printf("DIRECTX.InitDevice failed");
        exit(-1);
    }

    // Use FloorLevel tracking origin
    ovr_SetTrackingOriginType(mSession, ovrTrackingOrigin_FloorLevel);

    InitRenderTargets(hmdDesc);

    //Possibly move this into the main method to dynamically get the number of devices.
    //comm_buffer->num_objects = (ovr_GetConnectedControllerTypes(mSession) >> 8) & 0xf;

    // Main Loop
    uint64_t counter = 0;
    Render();
    uint64_t frameCount = 0;

    while (DIRECTX.HandleMessages() && !mShouldQuit)
    {
        ovrSessionStatus sessionStatus;
        ovr_GetSessionStatus(mSession, &sessionStatus);
        if (sessionStatus.ShouldQuit)
            break;

        mShouldQuit = !mainLoopCallback(mSession, frameCount);

        if ((frameCount & 0xF) == 0)
        {
            // 1000/16 ~= 60Hz, render black frame at 60fps to keep oculus happy
            Render();
        }

        frameCount++;
        Sleep(1);
    }

    ovr_Shutdown();
}

void GuardianSystemDemo::Render()
{
    // Get current eye pose for rendering
    double eyePoseTime = 0;
    ovrPosef eyePose[ovrEye_Count] = {};
    ovr_GetEyePoses(mSession, mFrameIndex, ovrTrue, mHmdToEyePose, eyePose, &eyePoseTime);

    // Render each eye
    for (int i = 0; i < ovrEye_Count; ++i)
    {
        int renderTargetIndex = 0;
        ovr_GetTextureSwapChainCurrentIndex(mSession, mTextureChain[i], &renderTargetIndex);
        ID3D11RenderTargetView* renderTargetView = mEyeRenderTargets[i][renderTargetIndex];
        ID3D11DepthStencilView* depthTargetView = mEyeDepthTarget[i];

        // Clear and set render/depth target and viewport
        DIRECTX.SetAndClearRenderTarget(renderTargetView, depthTargetView, 0.0f, 0.0f, 0.0f, 1.0f);  // THE SCREEN RENDER COLOUR
        DIRECTX.SetViewport((float)mEyeRenderViewport[i].Pos.x, (float)mEyeRenderViewport[i].Pos.y, (float)mEyeRenderViewport[i].Size.w, (float)mEyeRenderViewport[i].Size.h);

        // Eye
        XMVECTOR eyeRot = XMVectorSet(eyePose[i].Orientation.x, eyePose[i].Orientation.y, eyePose[i].Orientation.z, eyePose[i].Orientation.w);
        XMVECTOR eyePos = XMVectorSet(eyePose[i].Position.x, eyePose[i].Position.y, eyePose[i].Position.z, 0);
        XMVECTOR eyeForward = XMVector3Rotate(XMVectorSet(0, 0, -1, 0), eyeRot);

        // Matrices
        XMMATRIX viewMat = XMMatrixLookAtRH(eyePos, XMVectorAdd(eyePos, eyeForward), XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), eyeRot));
        ovrMatrix4f proj = ovrMatrix4f_Projection(mEyeRenderLayer.Fov[i], 0.001f, 1000.0f, ovrProjection_None);
        XMMATRIX projMat = XMMatrixTranspose(XMMATRIX(&proj.M[0][0]));
        XMMATRIX viewProjMat = XMMatrixMultiply(viewMat, projMat);

        // Render and commit to swap chain
        //mDynamicScene.Render(&viewProjMat, 1.0f, 1.0f, 1.0f, 1.0f, TRUE);
        ovr_CommitTextureSwapChain(mSession, mTextureChain[i]);

        // Update eye layer
        mEyeRenderLayer.ColorTexture[i] = mTextureChain[i];
        mEyeRenderLayer.RenderPose[i] = eyePose[i];
        mEyeRenderLayer.SensorSampleTime = eyePoseTime;
    }

    // Submit frames
    ovrLayerHeader* layers = &mEyeRenderLayer.Header;
    ovrResult result = ovr_SubmitFrame(mSession, mFrameIndex++, nullptr, &layers, 1);
    if (!OVR_SUCCESS(result))
    {
        printf("ovr_SubmitFrame failed");
        exit(-1);
    }
}
