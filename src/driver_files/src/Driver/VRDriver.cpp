#include "VRDriver.hpp"
#include <Driver/HMDDevice.hpp>
#include <Driver/TrackerDevice.hpp>
#include <Driver/ControllerDevice.hpp>
#include <Driver/TrackingReferenceDevice.hpp>
#include <thread>
#include "OculusTexture.hpp"

vr::EVRInitError OculusToSteamVR::VRDriver::Init(vr::IVRDriverContext* pDriverContext)
{
    //Perform driver context initialisation
    if (vr::EVRInitError init_error = vr::InitServerDriverContext(pDriverContext); init_error != vr::EVRInitError::VRInitError_None) {
        return init_error;
    }

    active = true;
    Log("Activating OculusToSteamVR...");

    ovrInitParams initParams = { ovrInit_RequestVersion | ovrInit_FocusAware, OVR_MINOR_VERSION, NULL, 0, 0 };
    ovrResult oculusVRResult = ovr_Initialize(&initParams);
    if (OVR_FAILURE(oculusVRResult))
    {
        //TODO: If the error is due to the Oculus service not being active, e.g. -3003 try to launch the Oculus service.
        Log("Failed to initalize OVR. Code " + std::to_string(oculusVRResult));
        return vr::VRInitError_Driver_Failed;
    }
    ovrGraphicsLuid oculusVRLuid;
    oculusVRResult = ovr_Create(&oculusVRSession, &oculusVRLuid);
    if (OVR_FAILURE(oculusVRResult))
    {
        Log("Failed to initalize OVR. Code " + std::to_string(oculusVRResult));
        ovr_Shutdown();
        return vr::VRInitError_Driver_Failed;
    }
    oculusVRInitialized = true;
    //ovr_SetTrackingOriginType(oculusVRSession, ovrTrackingOrigin_FloorLevel);
    ovr_SetTrackingOriginType(oculusVRSession, ovrTrackingOrigin_EyeLevel);
    ovrPosef oculusVROrigin;
    oculusVROrigin.Orientation.w = 1;
    oculusVROrigin.Orientation.x = oculusVROrigin.Orientation.y = oculusVROrigin.Orientation.z = 0;
    oculusVROrigin.Position.x = oculusVROrigin.Position.y = oculusVROrigin.Position.z = 0;
    ovr_SpecifyTrackingOrigin(oculusVRSession, oculusVROrigin);
    offset = oculusVROrigin;

    //Setup rendering to oculus, required to get focus and obtain input.
    //A nice bit of threaded mess here :)
    //I caused myself a lot of pain and fustration becuase this wasn't working and the error codes I was getting wer pretty useless
    //In the end I think the error was logical, where I had added a logging line caused the render loop to break early because I forgot to wrap thee two lines in brackets :/.
    //I pass in the driver here becuase it seems that GetDriver doesn't work across threads, despite the driver being static (I believe). My multithreaded knowledge is limited.
    std::thread([this](std::shared_ptr<OculusToSteamVR::IVRDriver> driver, ovrSession session, ovrGraphicsLuid luid)
    {
        while (driver->active)
        {
            if (!DIRECTX.InitWindow(GetModuleHandle(NULL), L"OculusToSteamVR [DO NOT CLOSE]")) { return vr::VRInitError_Driver_Failed; }
            DIRECTX.Run(OculusRenderLoop, driver, session, luid);
            
            ovrSessionStatus oculusVRSessionStatus;
            ovr_GetSessionStatus(session, &oculusVRSessionStatus);
            if (oculusVRSessionStatus.ShouldQuit) { break; }
        }
        Cleanup();
    }, GetDriver(), oculusVRSession, oculusVRLuid).detach();

    //Add controllers
    //this->AddDevice(std::make_shared<ControllerDevice>("oculus_to_steamvr_controller_ref", ControllerDevice::Handedness::ANY));
    this->AddDevice(std::make_shared<ControllerDevice>("oculus_to_steamvr_controller_right", ControllerDevice::Handedness::RIGHT));
    this->AddDevice(std::make_shared<ControllerDevice>("oculus_to_steamvr_controller_left", ControllerDevice::Handedness::LEFT));

    //Add a tracker
    //this->AddDevice(std::make_shared<TrackerDevice>("oculus_to_steamvr_TrackerDevice"));

    //Add a tracking reference. I would like to use this to indicate the 0 point/hmd location for calibration but I can't seem to get a model to appear for this so I am currently using a controller.
    for (unsigned int i = 0; i < ovr_GetTrackerCount(oculusVRSession); i++)
    {
        ovrTrackerPose oculusPose = ovr_GetTrackerPose(oculusVRSession, i);
        vr::DriverPose_t steamVRPose = IVRDevice::MakeDefaultPose();
        steamVRPose.vecPosition[0] = oculusPose.LeveledPose.Position.x;
        steamVRPose.vecPosition[1] = oculusPose.LeveledPose.Position.y;
        steamVRPose.vecPosition[2] = oculusPose.LeveledPose.Position.z;
        steamVRPose.qRotation.w = oculusPose.LeveledPose.Orientation.w;
        steamVRPose.qRotation.x = oculusPose.LeveledPose.Orientation.x;
        steamVRPose.qRotation.y = oculusPose.LeveledPose.Orientation.y;
        steamVRPose.qRotation.z = oculusPose.LeveledPose.Orientation.z;
        this->AddDevice(std::make_shared<TrackingReferenceDevice>("oculus_to_steamvr_TrackingReference_" + std::to_string(i), steamVRPose));
    }

    Log("OculusToSteamVR Loaded Successfully");

	return vr::VRInitError_None;
}

void OculusToSteamVR::VRDriver::Cleanup()
{
    active = false;
    Log("Shutting down OculusToSteamVR...");
    if (oculusVRInitialized)
    {
        ovr_Destroy(oculusVRSession);
        ovr_Shutdown();
    }
}

void OculusToSteamVR::VRDriver::RunFrame()
{
    // Collect events
    vr::VREvent_t event;
    std::vector<vr::VREvent_t> events;
    while (vr::VRServerDriverHost()->PollNextEvent(&event, sizeof(event)))
    {
        events.push_back(event);
    }
    this->openvr_events_ = events;

    // Update frame timing
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    this->frame_timing_ = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_frame_time_);
    this->last_frame_time_ = now;

    //Check oculus flags.
    ovrSessionStatus oculusVRSessionStatus;
    ovr_GetSessionStatus(oculusVRSession, &oculusVRSessionStatus);
    if (oculusVRSessionStatus.ShouldQuit) { Cleanup(); }
    //We don't want to change the Oculus origin as it could throw off the calibration within Steam space.
    if (oculusVRSessionStatus.ShouldRecenter) { ovr_ClearShouldRecenterFlag(oculusVRSession); }

    // Update devices
    for (auto& device : this->devices_)
    {
        device->Update();
    }
}

bool OculusToSteamVR::VRDriver::ShouldBlockStandbyMode()
{
    return false;
}

void OculusToSteamVR::VRDriver::EnterStandby()
{
}

void OculusToSteamVR::VRDriver::LeaveStandby()
{
}

std::vector<std::shared_ptr<OculusToSteamVR::IVRDevice>> OculusToSteamVR::VRDriver::GetDevices()
{
    return this->devices_;
}

std::vector<vr::VREvent_t> OculusToSteamVR::VRDriver::GetOpenVREvents()
{
    return this->openvr_events_;
}

std::chrono::milliseconds OculusToSteamVR::VRDriver::GetLastFrameTime()
{
    return this->frame_timing_;
}

bool OculusToSteamVR::VRDriver::AddDevice(std::shared_ptr<IVRDevice> device)
{
    vr::ETrackedDeviceClass openvr_device_class;
    // Remember to update this switch when new device types are added
    switch (device->GetDeviceType()) {
        case DeviceType::CONTROLLER:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
            break;
        case DeviceType::HMD:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_HMD;
            break;
        case DeviceType::TRACKER:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
            break;
        case DeviceType::TRACKING_REFERENCE:
            openvr_device_class = vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference;
            break;
        default:
            return false;
    }
    bool result = vr::VRServerDriverHost()->TrackedDeviceAdded(device->GetSerial().c_str(), openvr_device_class, device.get());
    if(result)
        this->devices_.push_back(device);
    return result;
}

OculusToSteamVR::SettingsValue OculusToSteamVR::VRDriver::GetSettingsValue(std::string key)
{
    vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None;
    int int_value = vr::VRSettings()->GetInt32(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return int_value;
    }
    err = vr::EVRSettingsError::VRSettingsError_None;
    float float_value = vr::VRSettings()->GetFloat(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return float_value;
    }
    err = vr::EVRSettingsError::VRSettingsError_None;
    bool bool_value = vr::VRSettings()->GetBool(settings_key_.c_str(), key.c_str(), &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return bool_value;
    }
    std::string str_value;
    str_value.reserve(1024);
    vr::VRSettings()->GetString(settings_key_.c_str(), key.c_str(), str_value.data(), 1024, &err);
    if (err == vr::EVRSettingsError::VRSettingsError_None) {
        return str_value;
    }
    err = vr::EVRSettingsError::VRSettingsError_None;

    return SettingsValue();
}

void OculusToSteamVR::VRDriver::Log(std::string message)
{
    std::string message_endl = message + "\n";
    vr::VRDriverLog()->Log(message_endl.c_str());
}

vr::IVRDriverInput* OculusToSteamVR::VRDriver::GetInput()
{
    return vr::VRDriverInput();
}

vr::CVRPropertyHelpers* OculusToSteamVR::VRDriver::GetProperties()
{
    return vr::VRProperties();
}

vr::IVRServerDriverHost* OculusToSteamVR::VRDriver::GetDriverHost()
{
    return vr::VRServerDriverHost();
}

//Sample tweaked from https://developer.oculus.com/downloads/package/oculus-sdk-for-windows/
bool OculusToSteamVR::VRDriver::OculusRenderLoop(
    bool retryCreate,
    std::shared_ptr<OculusToSteamVR::IVRDriver> driver,
    ovrSession session,
    ovrGraphicsLuid luid
)
{
    // Initialize these to nullptr here to handle device lost failures cleanly
    OculusTexture* pEyeRenderTexture[2] = { nullptr, nullptr };
    Camera* mainCam = nullptr;
    long long frameIndex = 0;
    int msaaRate = 4;

    ovrResult result;
    ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session);

    // Setup Device and Graphics
    // Note: the mirror window can be any size.
    if (!DIRECTX.InitDevice(426, 1, reinterpret_cast<LUID*>(&luid)))
    {
        driver->Log("Failed to initialize DirectX.");
        goto Done;
    }
    //Hide the window to prevent the user from accidentally closing it or having it in their way.
    DIRECTX.HideWindow();
    
    // Make the eye render buffers (caution if actual size < requested due to HW limits).
    ovrRecti eyeRenderViewport[2];

    for (int eye = 0; eye < 2; ++eye)
    {
        ovrSizei idealSize = ovr_GetFovTextureSize(session, (ovrEyeType)eye, hmdDesc.DefaultEyeFov[eye], 1.0f);
        pEyeRenderTexture[eye] = new OculusTexture();
        if (!pEyeRenderTexture[eye]->Init(session, idealSize.w, idealSize.h, msaaRate, true))
        {
            driver->Log("Failed to create eye texture.");
            if (retryCreate) { goto Done; }
            return false;
        }
        eyeRenderViewport[eye].Pos.x = 0;
        eyeRenderViewport[eye].Pos.y = 0;
        eyeRenderViewport[eye].Size = idealSize;
        if (!pEyeRenderTexture[eye]->TextureChain || !pEyeRenderTexture[eye]->DepthTextureChain)
        {
            driver->Log("Failed to create texture.");
            if (retryCreate) { goto Done; }
            return false;
        }
    }

    // Create camera
    mainCam = new Camera(XMVectorSet(0.0f, 0.0f, 5.0f, 0), XMQuaternionIdentity());

    // FloorLevel will give tracking poses where the floor height is 0
    ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);

    // Main loop
    while (DIRECTX.HandleMessages())
    {
        ovrSessionStatus sessionStatus;
        result = ovr_GetSessionStatus(session, &sessionStatus);
        if (OVR_FAILURE(result))
        {
            driver->Log("Connection failed.");
            return false;
        }

        if (sessionStatus.ShouldQuit)
        {
            driver->Log("OVR requested to quit.");
            // Because the application is requested to quit, should not request retry
            retryCreate = false;
            break;
        }
        /*if (sessionStatus.ShouldRecenter)
            ovr_RecenterTrackingOrigin(session);*/

        if (sessionStatus.IsVisible)
        {
            // Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyePose) may change at runtime.
            ovrEyeRenderDesc eyeRenderDesc[2];
            eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
            eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

            // Get both eye poses simultaneously, with IPD offset already included.
            ovrPosef EyeRenderPose[2];
            ovrPosef HmdToEyePose[2] =
            {
                eyeRenderDesc[0].HmdToEyePose,
                eyeRenderDesc[1].HmdToEyePose
            };

            double sensorSampleTime; // sensorSampleTime is fed into the layer later
            ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyePose, EyeRenderPose, &sensorSampleTime);

            ovrTimewarpProjectionDesc posTimewarpProjectionDesc = {};

            // Render Scene to Eye Buffers
            for (int eye = 0; eye < 2; ++eye)
            {
                // Clear and set up rendertarget
                DIRECTX.SetAndClearRenderTarget(pEyeRenderTexture[eye]->GetRTV(), pEyeRenderTexture[eye]->GetDSV());
                DIRECTX.SetViewport((float)eyeRenderViewport[eye].Pos.x, (float)eyeRenderViewport[eye].Pos.y,
                    (float)eyeRenderViewport[eye].Size.w, (float)eyeRenderViewport[eye].Size.h);

                //Get the pose information in XM format
                XMVECTOR eyeQuat = XMVectorSet(EyeRenderPose[eye].Orientation.x, EyeRenderPose[eye].Orientation.y,
                    EyeRenderPose[eye].Orientation.z, EyeRenderPose[eye].Orientation.w);
                XMVECTOR eyePos = XMVectorSet(EyeRenderPose[eye].Position.x, EyeRenderPose[eye].Position.y, EyeRenderPose[eye].Position.z, 0);

                // Get view and projection matrices for the Rift camera
                XMVECTOR CombinedPos = XMVectorAdd(mainCam->Pos, XMVector3Rotate(eyePos, mainCam->Rot));
                Camera finalCam(CombinedPos, XMQuaternionMultiply(eyeQuat, mainCam->Rot));
                XMMATRIX view = finalCam.GetViewMatrix();
                ovrMatrix4f p = ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, 0.2f, 1000.0f, ovrProjection_None);
                posTimewarpProjectionDesc = ovrTimewarpProjectionDesc_FromProjection(p, ovrProjection_None);
                XMMATRIX proj = XMMatrixSet(
                    p.M[0][0], p.M[1][0], p.M[2][0], p.M[3][0],
                    p.M[0][1], p.M[1][1], p.M[2][1], p.M[3][1],
                    p.M[0][2], p.M[1][2], p.M[2][2], p.M[3][2],
                    p.M[0][3], p.M[1][3], p.M[2][3], p.M[3][3]
                );
                XMMATRIX prod = XMMatrixMultiply(view, proj);

                // Commit rendering to the swap chain
                pEyeRenderTexture[eye]->Commit();
            }

            // Initialize our single full screen Fov layer.
            ovrLayerEyeFovDepth ld = {};
            ld.Header.Type = ovrLayerType_EyeFovDepth;
            ld.Header.Flags = 0;
            ld.ProjectionDesc = posTimewarpProjectionDesc;
            ld.SensorSampleTime = sensorSampleTime;

            for (int eye = 0; eye < 2; ++eye)
            {
                ld.ColorTexture[eye] = pEyeRenderTexture[eye]->TextureChain;
                ld.DepthTexture[eye] = pEyeRenderTexture[eye]->DepthTextureChain;
                ld.Viewport[eye] = eyeRenderViewport[eye];
                ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
                ld.RenderPose[eye] = EyeRenderPose[eye];
            }

            ovrLayerHeader* layers = &ld.Header;
            result = ovr_SubmitFrame(session, frameIndex, nullptr, &layers, 1);
            // exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
            if (!OVR_SUCCESS(result))
            {
                driver->Log("Failed to render frame.");
                goto Done;
            }

            frameIndex++;
        }
    }
    driver->Log("Render loop ended.");

    // Release resources
Done:
    driver->Log("Render loop quit.");
    delete mainCam;
    for (int eye = 0; eye < 2; ++eye)
    {
        delete pEyeRenderTexture[eye];
    }
    DIRECTX.ReleaseDevice();

    // Retry on ovrError_DisplayLost
    return retryCreate || (result == ovrError_DisplayLost);
}
