#include "TrackingReferenceDevice.hpp"
#include <Windows.h>

OculusToSteamVR::TrackingReferenceDevice::TrackingReferenceDevice(unsigned int index):
    index(index)
{
    //OSC -> Oculus Steam Reference.
    serial = "OSR-" + Helpers::GetSerialSuffix(index);
}

std::string OculusToSteamVR::TrackingReferenceDevice::GetSerial()
{
    return serial;
}

void OculusToSteamVR::TrackingReferenceDevice::Update(SharedData* sharedBuffer)
{
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) return;

    // Setup pose for this frame
    //auto newPose = IVRDevice::MakeDefaultPose();
    auto newPose = lastPose;
    newPose.poseIsValid = true;
    newPose.result = vr::ETrackingResult::TrackingResult_Running_OK;
    newPose.qDriverFromHeadRotation = { 1, 0, 0, 0 };
    newPose.qWorldFromDriverRotation = { 1, 0, 0, 0 };
    newPose.poseTimeOffset = 0;

    ovrPosef pose = sharedBuffer->trackingRefrences[index].LeveledPose;
    unsigned int flags = sharedBuffer->trackingRefrences[index].TrackerFlags;

    newPose.vecPosition[0] = pose.Position.x;
    newPose.vecPosition[1] = pose.Position.y;
    newPose.vecPosition[2] = pose.Position.z;

    newPose.qRotation.w = pose.Orientation.w;
    newPose.qRotation.x = pose.Orientation.x;
    newPose.qRotation.y = pose.Orientation.y;
    newPose.qRotation.z = pose.Orientation.z;

    if (!(flags & ovrTrackerFlags::ovrTracker_PoseTracked))
    {
        newPose.poseIsValid = false;
        newPose.result = vr::ETrackingResult::TrackingResult_Running_OutOfRange;
    }

    if (!(flags & ovrTrackerFlags::ovrTracker_Connected))
    {
        newPose.poseIsValid = false;
        newPose.deviceIsConnected = false;
        newPose.result = vr::ETrackingResult::TrackingResult_Running_OutOfRange;
    }

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(deviceIndex, newPose, sizeof(vr::DriverPose_t));
    lastPose = newPose;
}

DeviceType OculusToSteamVR::TrackingReferenceDevice::GetDeviceType()
{
    return DeviceType::TRACKING_REFERENCE;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::TrackingReferenceDevice::GetDeviceIndex()
{
    return deviceIndex;
}

vr::EVRInitError OculusToSteamVR::TrackingReferenceDevice::Activate(uint32_t unObjectId)
{
    deviceIndex = unObjectId;

    GetDriver()->Log("Activating tracking reference " + serial);

    //Get the properties handle.
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(deviceIndex);

    //Set some universe ID (Must be 2 or higher).
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 31);
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_TrackingSystemName_String, "oculus");

    //Set up a model "number" (not needed but good to have).
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, (std::string("Oculus Sensor ") + std::to_string(index)).c_str());

    //Set up a render model path.
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "rift_camera");

    //Set the icons.
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{oculus}/icons/cv1_camera_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{oculus}/icons/cv1_camera_off.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus}/icons/cv1_camera_searching.gif");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus}/icons/cv1_camera_searching_alert.gif");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus}/icons/cv1_camera_ready_alert.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus}/icons/cv1_camera_error.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus}/icons/cv1_camera_off.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus}/icons/cv1_camera_ready_alert.png");

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_SerialNumber_String, serial.c_str());

    return vr::EVRInitError::VRInitError_None;
}

void OculusToSteamVR::TrackingReferenceDevice::Deactivate()
{
    deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
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
    return lastPose;
}
