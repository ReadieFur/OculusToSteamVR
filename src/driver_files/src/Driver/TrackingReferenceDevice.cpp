#include "TrackingReferenceDevice.hpp"
#include <Windows.h>

OculusToSteamVR::TrackingReferenceDevice::TrackingReferenceDevice(std::string serial):
    serial_(serial)
{
}

std::string OculusToSteamVR::TrackingReferenceDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::TrackingReferenceDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
        return;

    vr::DriverPose_t defaultPose = IVRDevice::MakeDefaultPose();
    defaultPose.vecPosition[0] =
        defaultPose.vecPosition[1] =
        defaultPose.vecPosition[2] =
        defaultPose.qRotation.w =
        defaultPose.qRotation.x =
        defaultPose.qRotation.y =
        defaultPose.qRotation.z =
        0;
    this->last_pose_ = defaultPose;
}

DeviceType OculusToSteamVR::TrackingReferenceDevice::GetDeviceType()
{
    return DeviceType::TRACKING_REFERENCE;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::TrackingReferenceDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError OculusToSteamVR::TrackingReferenceDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracking reference " + this->serial_);

    // Get the properties handle
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "example_trackingreference");

    // Set up a render model path
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "locator");

    // Set the icons
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{oculus_to_steamvr}/icons/trackingreference_ready.png");

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{oculus_to_steamvr}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus_to_steamvr}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus_to_steamvr}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus_to_steamvr}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus_to_steamvr}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus_to_steamvr}/icons/trackingreference_not_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus_to_steamvr}/icons/trackingreference_not_ready.png");

    vr::DriverPose_t defaultPose = IVRDevice::MakeDefaultPose();
    defaultPose.vecPosition[0] =
        defaultPose.vecPosition[1] =
        defaultPose.vecPosition[2] =
        defaultPose.qRotation.w =
        defaultPose.qRotation.x =
        defaultPose.qRotation.y =
        defaultPose.qRotation.z =
        0;
    this->last_pose_ = defaultPose;

    return vr::EVRInitError::VRInitError_None;
}

void OculusToSteamVR::TrackingReferenceDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void OculusToSteamVR::TrackingReferenceDevice::EnterStandby()
{
}

void* OculusToSteamVR::TrackingReferenceDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    return nullptr;
}

void OculusToSteamVR::TrackingReferenceDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t OculusToSteamVR::TrackingReferenceDevice::GetPose()
{
    return last_pose_;
}
