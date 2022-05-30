#include "HMDDevice.hpp"
#include <Windows.h>

OculusToSteamVR::HMDDevice::HMDDevice(std::string serial):serial_(serial)
{
}

std::string OculusToSteamVR::HMDDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::HMDDevice::Update(ovrPosef pose)
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
        return;
    
    // Setup pose for this frame
    auto newPose = IVRDevice::MakeDefaultPose();

    newPose.vecPosition[0] = pose.Position.x;
    newPose.vecPosition[1] = pose.Position.y;
    newPose.vecPosition[2] = pose.Position.z;
    newPose.qRotation.w = pose.Orientation.w;
    newPose.qRotation.x = pose.Orientation.x;
    newPose.qRotation.y = pose.Orientation.y;
    newPose.qRotation.z = pose.Orientation.z;

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, newPose, sizeof(vr::DriverPose_t));
    this->last_pose_ = newPose;
}

DeviceType OculusToSteamVR::HMDDevice::GetDeviceType()
{
    return DeviceType::HMD;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::HMDDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError OculusToSteamVR::HMDDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating HMD " + this->serial_);

    // Load settings values
    // Could probably make this cleaner with making a wrapper class
    try
    {
        int window_x = std::get<int>(GetDriver()->GetSettingsValue("window_x"));
        if (window_x > 0)
            this->window_x_ = window_x;
    }
    catch (const std::bad_variant_access&) {}; // Wrong type or doesnt exist

    try
    {
        int window_y = std::get<int>(GetDriver()->GetSettingsValue("window_y"));
        if (window_y > 0)
            this->window_x_ = window_y;
    }
    catch (const std::bad_variant_access&) {}; // Wrong type or doesnt exist

    try
    {
        int window_width = std::get<int>(GetDriver()->GetSettingsValue("window_width"));
        if (window_width > 0)
            this->window_width_ = window_width;
    }
    catch (const std::bad_variant_access&) {}; // Wrong type or doesnt exist

    try
    {
        int window_height = std::get<int>(GetDriver()->GetSettingsValue("window_height"));
        if (window_height > 0)
            this->window_height_ = window_height;
    }
    catch (const std::bad_variant_access&) {}; // Wrong type or doesnt exist

    // Get the properties handle
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);

    // Set the IPD to be whatever steam has configured
    GetDriver()->GetProperties()->SetFloatProperty(props, vr::Prop_UserIpdMeters_Float, vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_IPD_Float));

    // Set the display FPS
    GetDriver()->GetProperties()->SetFloatProperty(props, vr::Prop_DisplayFrequency_Float, 90.f);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "EXAMPLE_HMD_DEVICE");

    // Set up icon paths
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{oculus_to_steamvr}/icons/hmd_ready.png");

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{oculus_to_steamvr}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus_to_steamvr}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus_to_steamvr}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus_to_steamvr}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus_to_steamvr}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus_to_steamvr}/icons/hmd_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus_to_steamvr}/icons/hmd_not_ready.png");

    return vr::EVRInitError::VRInitError_None;
}

void OculusToSteamVR::HMDDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void OculusToSteamVR::HMDDevice::EnterStandby()
{
}

void* OculusToSteamVR::HMDDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    if (!_stricmp(pchComponentNameAndVersion, vr::IVRDisplayComponent_Version))
    {
        return static_cast<vr::IVRDisplayComponent*>(this);
    }
    return nullptr;
}

void OculusToSteamVR::HMDDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t OculusToSteamVR::HMDDevice::GetPose()
{
    return this->last_pose_;
}

void OculusToSteamVR::HMDDevice::GetWindowBounds(int32_t* pnX, int32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
    *pnX = this->window_x_;
    *pnY = this->window_y_;
    *pnWidth = this->window_width_;
    *pnHeight = this->window_height_;
}

bool OculusToSteamVR::HMDDevice::IsDisplayOnDesktop()
{
    return true;
}

bool OculusToSteamVR::HMDDevice::IsDisplayRealDisplay()
{
    return false;
}

void OculusToSteamVR::HMDDevice::GetRecommendedRenderTargetSize(uint32_t* pnWidth, uint32_t* pnHeight)
{
    *pnWidth = this->window_width_;
    *pnHeight = this->window_height_;
}

void OculusToSteamVR::HMDDevice::GetEyeOutputViewport(vr::EVREye eEye, uint32_t* pnX, uint32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
    *pnY = 0;
    *pnWidth = this->window_width_ / 2;
    *pnHeight = this->window_height_;

    if (eEye == vr::EVREye::Eye_Left) *pnX = 0;
    else *pnX = this->window_width_ / 2;
}

void OculusToSteamVR::HMDDevice::GetProjectionRaw(vr::EVREye eEye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom)
{
    *pfLeft = -1;
    *pfRight = 1;
    *pfTop = -1;
    *pfBottom = 1;
}

vr::DistortionCoordinates_t OculusToSteamVR::HMDDevice::ComputeDistortion(vr::EVREye eEye, float fU, float fV)
{
    vr::DistortionCoordinates_t coordinates;
    coordinates.rfBlue[0] = fU;
    coordinates.rfBlue[1] = fV;
    coordinates.rfGreen[0] = fU;
    coordinates.rfGreen[1] = fV;
    coordinates.rfRed[0] = fU;
    coordinates.rfRed[1] = fV;
    return coordinates;
}