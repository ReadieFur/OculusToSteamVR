#include "VRDriver.hpp"
#include <Driver/HMDDevice.hpp>
#include <Driver/TrackerDevice.hpp>
#include <Driver/ControllerDevice.hpp>
#include <Driver/TrackingReferenceDevice.hpp>
#include <thread>
#include "OculusTexture.hpp"
#include <OVR_Math.h>
#include <typeinfo>

vr::EVRInitError OculusToSteamVR::VRDriver::Init(vr::IVRDriverContext* pDriverContext)
{
    //Perform driver context initialisation
    if (vr::EVRInitError init_error = vr::InitServerDriverContext(pDriverContext); init_error != vr::EVRInitError::VRInitError_None) {
        return init_error;
    }

    active = true;
    Log("Activating OculusToSteamVR...");

    ovrInitParams initParams = { ovrInit_RequestVersion | ovrInit_FocusAware, OVR_MINOR_VERSION, NULL, 0, 0 };
    ovrResult oculusVRResult;
    //Try to initialize up to 3 times.
    for (int i = 0; i < 3; i++)
    {
        oculusVRResult = ovr_Initialize(&initParams);
        if (OVR_FAILURE(oculusVRResult))
        {
            //https://developer.oculus.com/reference/libovr/v32/o_v_r_error_code_8h/
            Log("Failed to initalize OVR. Code " + std::to_string(oculusVRResult) + ". Attempt " + std::to_string(i + 1) + "/3.");
            if (i == 2) { return vr::VRInitError_Driver_Failed; }
        }
        else { break; }
        Sleep(1000); //If oculus failed because it's busy, give it a moment to complete it's tasks.
    }
    ovrGraphicsLuid oculusVRLuid;
    for (int i = 0; i < 3; i++)
    {
        oculusVRResult = ovr_Create(&oculusVRSession, &oculusVRLuid);
        if (OVR_FAILURE(oculusVRResult))
        {
            //https://developer.oculus.com/reference/libovr/v32/o_v_r_error_code_8h/
            Log("Failed to initalize OVR. Code " + std::to_string(oculusVRResult) + ". Attempt " + std::to_string(i + 1) + "/3.");
            if (i == 2) { return vr::VRInitError_Driver_Failed; }
        }
        else { break; }
        Sleep(1000);
    }
    oculusVRInitialized = true;

    const ovrHmdDesc oculusHMDDesc = ovr_GetHmdDesc(oculusVRSession);

    //Set oculus tracking type.
    ovr_SetTrackingOriginType(oculusVRSession, ovrTrackingOrigin_EyeLevel);
    ovrPosef oculusVROrigin;
    oculusVROrigin.Orientation.w = 1;
    oculusVROrigin.Orientation.x = oculusVROrigin.Orientation.y = oculusVROrigin.Orientation.z = 0;
    oculusVROrigin.Position.x = oculusVROrigin.Position.y = oculusVROrigin.Position.z = 0;
    ovr_SpecifyTrackingOrigin(oculusVRSession, oculusVROrigin);

    //Load settings.
    leftOffset = rightOffset = oculusVROrigin; //Set defaults.
    //Position.
    rightOffset.Translation.x = GetSettingsFloat("offset_px", 0);
    rightOffset.Translation.y = GetSettingsFloat("offset_py", 0);
    rightOffset.Translation.z = GetSettingsFloat("offset_pz", 0);
    leftOffset.Translation.x = rightOffset.Translation.x;
    leftOffset.Translation.y = rightOffset.Translation.y;
    leftOffset.Translation.z = rightOffset.Translation.z;
    //Rotation.
    eulerAnglesOffset =
    {
        GetSettingsFloat("offset_rx", 0),
        GetSettingsFloat("offset_ry", 0),
        GetSettingsFloat("offset_rz", 0)
    };
    rightOffset.Rotation = OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(
        eulerAnglesOffset.x,
        eulerAnglesOffset.y,
        eulerAnglesOffset.z
    ));
    leftOffset.Rotation = OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(
        eulerAnglesOffset.x,
        -eulerAnglesOffset.y,
        -eulerAnglesOffset.z
    ));
    Log(
        "Loaded offset:" +
        std::string(" px=") + std::to_string(rightOffset.Translation.x) +
        std::string(" py=") + std::to_string(rightOffset.Translation.y) +
        std::string(" pz=") + std::to_string(rightOffset.Translation.z) +
        //std::string(" rw=") + std::to_string(rightOffset.Rotation.w) +
        std::string(" rx=") + std::to_string(eulerAnglesOffset.x) +
        std::string(" ry=") + std::to_string(eulerAnglesOffset.y) +
        std::string(" rz=") + std::to_string(eulerAnglesOffset.z)
    );

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

    //Add controllers.
    //this->AddDevice(std::make_shared<ControllerDevice>("oculus_to_steamvr_controller_ref", ControllerDevice::Handedness::ANY));
    this->AddDevice(std::make_shared<ControllerDevice>(std::string(oculusHMDDesc.SerialNumber) + "_Controller_Right", Handedness::RIGHT));
    this->AddDevice(std::make_shared<ControllerDevice>(std::string(oculusHMDDesc.SerialNumber) + "_Controller_Left", Handedness::LEFT));

    //Add trackers (the controllers but disabled by default).
    this->AddDevice(std::make_shared<TrackerDevice>(std::string(oculusHMDDesc.SerialNumber) + "_Controller_Right_Tracker", Handedness::RIGHT));
    this->AddDevice(std::make_shared<TrackerDevice>(std::string(oculusHMDDesc.SerialNumber) + "_Controller_Left_Tracker", Handedness::LEFT));

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
        this->AddDevice(std::make_shared<TrackingReferenceDevice>(std::string(oculusHMDDesc.SerialNumber) + "_Tracking_Reference_" + std::to_string(i), steamVRPose));
    }

    //This happens too soon.
    vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None;
    bool _inControllerMode = vr::VRSettings()->GetBool(settings_key_.c_str(), "in_controller_mode", &err);
    _inControllerMode = err != vr::EVRSettingsError::VRSettingsError_None ? true : _inControllerMode;
    for (auto& device : this->devices_)
    {
        //Deactivate the devices based on the input mode.
        if (device->GetDeviceType() == DeviceType::CONTROLLER && !_inControllerMode) { device->Disable(); }
        else if (device->GetDeviceType() == DeviceType::TRACKER && _inControllerMode) { device->Disable(); }
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

    //Calibration
    ovrInputState inputState;
    if (OVR_SUCCESS(ovr_GetInputState(oculusVRSession, ovrControllerType_Touch, &inputState)))
    {
        bool calibrationKeysPressed = inputState.Buttons == ovrButton_Enter + ovrButton_A;
        if (calibrationButtonTime < 1.0f) { calibrationButtonTime += GetDriver()->GetLastFrameTime().count() / 1000.f; }
        //Do not toggle the calibration state if the buttons were pressed within the past second.
        if (calibrationKeysPressed && calibrationButtonTime >= 1.0f)
        {
            if (isManuallyCalibrating)
            {
                //Exiting calibration mode.
                //Save settings.
                vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None; //Ignore all errors for now.
                vr::VRSettings()->SetFloat(settings_key_.c_str(), "offset_px", rightOffset.Translation.x, &err);
                vr::VRSettings()->SetFloat(settings_key_.c_str(), "offset_py", rightOffset.Translation.y, &err);
                vr::VRSettings()->SetFloat(settings_key_.c_str(), "offset_pz", rightOffset.Translation.z, &err);
                vr::VRSettings()->SetFloat(settings_key_.c_str(), "offset_rx", eulerAnglesOffset.x, &err);
                vr::VRSettings()->SetFloat(settings_key_.c_str(), "offset_ry", eulerAnglesOffset.y, &err);
                vr::VRSettings()->SetFloat(settings_key_.c_str(), "offset_rz", eulerAnglesOffset.z, &err);
                Log(
                    "Offset set to:" +
                    std::string(" px=") + std::to_string(rightOffset.Translation.x) +
                    std::string(" py=") + std::to_string(rightOffset.Translation.y) +
                    std::string(" pz=") + std::to_string(rightOffset.Translation.z) +
                    //std::string(" rw=") + std::to_string(rightOffset.Rotation.w) +
                    std::string(" rx=") + std::to_string(eulerAnglesOffset.x) +
                    std::string(" ry=") + std::to_string(eulerAnglesOffset.y) +
                    std::string(" rz=") + std::to_string(eulerAnglesOffset.z)
                );
            }
            isCalibratingPosition = true; //Default to always start with position calibration.
            isManuallyCalibrating = !isManuallyCalibrating;
            calibrationButtonTime = 0.0f;
        }
        //If calibration toggle buttons are pressed, don't move the controllers.
        if (isManuallyCalibrating && (calibrationButtonTime >= 1.0f || !calibrationKeysPressed))
        {
            //Check if we should switch between position and rotation calibration modes.
            if (inputState.IndexTrigger[0] >= 0.9f || inputState.IndexTrigger[1] >= 0.9f) { isCalibratingPosition = !isCalibratingPosition; }

            //I am not using the thumbsticks becuase they are the most likley componenets to be damanged and have drift (as they are in my case).
            if (isCalibratingPosition) //Position
            {
                //Check if the user wants to reset the values to the default.
                if (inputState.Buttons & ovrButton_LThumb)
                {
                    rightOffset.Translation.x =
                        rightOffset.Translation.y =
                        rightOffset.Translation.z =
                        0;
                }
                //Check if the user wants to reset the values to the last saved values.
                if (inputState.Buttons & ovrButton_RThumb)
                {
                    vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None;
                    rightOffset.Translation.x = GetSettingsFloat("offset_px", 0);
                    rightOffset.Translation.y = GetSettingsFloat("offset_py", 0);
                    rightOffset.Translation.z = GetSettingsFloat("offset_pz", 0);
                    leftOffset.Translation.x = rightOffset.Translation.x;
                    leftOffset.Translation.y = rightOffset.Translation.y;
                    leftOffset.Translation.z = rightOffset.Translation.z;
                }

                const float distanceToAdd = 0.001f;
                if (inputState.Buttons & ovrButton_X) { rightOffset.Translation.y += distanceToAdd; } //Up
                if (inputState.Buttons & ovrButton_Y) { rightOffset.Translation.y += -distanceToAdd; } //Down
                if (inputState.Buttons & ovrButton_A) { rightOffset.Translation.x += -distanceToAdd; } //Left
                if (inputState.Buttons & ovrButton_B) { rightOffset.Translation.x += distanceToAdd; } //Right
                if (inputState.HandTrigger[1] >= 0.9f) { rightOffset.Translation.z += distanceToAdd; } //Forward
                if (inputState.HandTrigger[0] >= 0.9f) { rightOffset.Translation.z += -distanceToAdd; } //Back
                //leftOffset.Translation = rightOffset.Translation; //Translation should be the same on both sides.
            }
            else //Rotation
            {
                if (inputState.Buttons & ovrButton_LThumb)
                {
                    leftOffset.Rotation.w = rightOffset.Rotation.w = 1;
                    leftOffset.Rotation.x =
                        leftOffset.Rotation.y =
                        leftOffset.Rotation.z =
                        rightOffset.Rotation.x =
                        rightOffset.Rotation.y =
                        rightOffset.Rotation.z =
                        0;
                    eulerAnglesOffset = { 0, 0, 0 };
                }
                if (inputState.Buttons & ovrButton_RThumb)
                {
                    eulerAnglesOffset =
                    {
                        GetSettingsFloat("offset_rx", 0),
                        GetSettingsFloat("offset_ry", 0),
                        GetSettingsFloat("offset_rz", 0)
                    };
                    rightOffset.Rotation = OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(
                        eulerAnglesOffset.x,
                        eulerAnglesOffset.y,
                        eulerAnglesOffset.z
                    ));
                    leftOffset.Rotation = OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(
                        eulerAnglesOffset.x,
                        -eulerAnglesOffset.y,
                        -eulerAnglesOffset.z
                    ));
                }

                const float rotationToAdd = 0.01f;
                if (inputState.Buttons & ovrButton_X) //RX+
                {
                    eulerAnglesOffset.x += rotationToAdd;
                    rightOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(rotationToAdd, 0.0f, 0.0f));
                    leftOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(rotationToAdd, 0.0f, 0.0f));
                }
                if (inputState.Buttons & ovrButton_Y) //RX-
                {
                    eulerAnglesOffset.x -= rotationToAdd;
                    rightOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(-rotationToAdd, 0.0f, 0.0f));
                    leftOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(-rotationToAdd, 0.0f, 0.0f));
                }
                if (inputState.Buttons & ovrButton_A) //RY+
                {
                    eulerAnglesOffset.y += rotationToAdd;
                    rightOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, rotationToAdd, 0.0f));
                    leftOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, -rotationToAdd, 0.0f));
                }
                if (inputState.Buttons & ovrButton_B) //RY-
                {
                    eulerAnglesOffset.y -= rotationToAdd;
                    rightOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, -rotationToAdd, 0.0f));
                    leftOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, rotationToAdd, 0.0f));
                }
                if (inputState.HandTrigger[1] >= 0.9f) //RZ+
                {
                    eulerAnglesOffset.z += rotationToAdd;
                    rightOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, 0.0f, rotationToAdd));
                    leftOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, 0.0f, -rotationToAdd));
                }
                if (inputState.HandTrigger[0] >= 0.9f) //RZ-
                {
                    eulerAnglesOffset.z -= rotationToAdd;
                    rightOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, 0.0f, -rotationToAdd));
                    leftOffset.Rotation *= OVR::Quat<float>::FromRotationVector(OVR::Vector3<float>(0.0f, 0.0f, rotationToAdd));
                }
            }
        }

        //Toggle device mode.
        if (modeToggleButtonTime < 1.0f) { modeToggleButtonTime += GetDriver()->GetLastFrameTime().count() / 1000.f; }
        if (inputState.Buttons == ovrButton_Enter + ovrButton_RThumb && modeToggleButtonTime >= 1.0f)
        {
            for (auto& device : this->devices_)
            {
                if (device->GetDeviceType() == DeviceType::CONTROLLER && inControllerMode) { device->Disable(); }
                else if (device->GetDeviceType() == DeviceType::CONTROLLER && !inControllerMode) { device->Enable(); }
                else if (device->GetDeviceType() == DeviceType::TRACKER && inControllerMode) { device->Enable(); }
                else if (device->GetDeviceType() == DeviceType::TRACKER && !inControllerMode) { device->Disable(); }
            }
            inControllerMode = !inControllerMode;
            vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None; //Ignore all errors for now.
            vr::VRSettings()->SetBool(settings_key_.c_str(), "in_controller_mode", inControllerMode, &err);
            modeToggleButtonTime = 0.0f;
        }
    }

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

float OculusToSteamVR::VRDriver::GetSettingsFloat(std::string key, float defaultValue)
{
    vr::EVRSettingsError err = vr::EVRSettingsError::VRSettingsError_None;
    float value = vr::VRSettings()->GetFloat(settings_key_.c_str(), key.c_str(), &err);
    return err != vr::EVRSettingsError::VRSettingsError_None ? defaultValue : value;
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
