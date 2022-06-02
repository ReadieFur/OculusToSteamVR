#include "TrackingReferenceDevice.hpp"
#include <Windows.h>

OculusToSteamVR::TrackingReferenceDevice::TrackingReferenceDevice(std::string serial, unsigned int index):
    serial_(serial),
    index_(index)
{
    this->random_angle_rad_ = fmod(rand() / 10000.f, 2 * 3.14159f);
}

std::string OculusToSteamVR::TrackingReferenceDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::TrackingReferenceDevice::Update(SharedData* sharedBuffer)
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
        return;

    // Setup pose for this frame
    //auto newPose = IVRDevice::MakeDefaultPose();
    auto newPose = this->last_pose_;
    newPose.poseIsValid = true;
    newPose.deviceIsConnected = true;
    newPose.result = vr::ETrackingResult::TrackingResult_Running_OK;

    ovrPosef pose = sharedBuffer->trackingRefrences[this->index_].LeveledPose;
    unsigned int flags = sharedBuffer->trackingRefrences[this->index_].TrackerFlags;

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
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, newPose, sizeof(vr::DriverPose_t));
    this->last_pose_ = newPose;
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

    //Get the properties handle.
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    //Set some universe ID (Must be 2 or higher).
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 31);
    //vr::VRProperties()->SetStringProperty(props, vr::Prop_TrackingSystemName_String, "oculus");

    //Set up a model "number" (not needed but good to have).
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "oculus_trackingreference");

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
