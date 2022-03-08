#include "VRDriver.hpp"
#include <Driver/HMDDevice.hpp>
#include <Driver/TrackerDevice.hpp>
#include <Driver/ControllerDevice.hpp>
#include <Driver/TrackingReferenceDevice.hpp>

vr::EVRInitError OculusToSteamVR::VRDriver::Init(vr::IVRDriverContext* pDriverContext)
{
    //Perform driver context initialisation
    if (vr::EVRInitError init_error = vr::InitServerDriverContext(pDriverContext); init_error != vr::EVRInitError::VRInitError_None) {
        return init_error;
    }

    Log("Activating OculusToSteamVR...");

    ovrResult oculusVRResult = ovr_Initialize(nullptr);
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
    Log("Shutting down ExampleDriver...");
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
    ovr_RecenterTrackingOrigin(oculusVRSession);
    ovr_GetSessionStatus(oculusVRSession, &oculusVRSessionStatus);
    if (oculusVRSessionStatus.ShouldQuit) { Cleanup(); }
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
