#include "TrackerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::TrackerDevice::TrackerDevice(std::string serial, OculusDeviceType oculusDeviceType):
    serial_(serial),
    oculus_device_type_(oculusDeviceType)
{
}

std::string OculusToSteamVR::TrackerDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::TrackerDevice::Update(SharedData* sharedBuffer)
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
        return;

    // Check if this device was asked to be identified
    auto events = GetDriver()->GetOpenVREvents();
    for (auto event : events)
    {
        // Note here, event.trackedDeviceIndex does not necissarily equal this->device_index_, not sure why, but the component handle will match so we can just use that instead
        //if (event.trackedDeviceIndex == this->device_index_) {
        if (event.eventType == vr::EVREventType::VREvent_Input_HapticVibration)
        {
            if (event.data.hapticVibration.componentHandle == this->haptic_component_) this->did_vibrate_ = true;
        }
        //}
    }

    // Check if we need to keep vibrating
    if (this->did_vibrate_)
    {
        this->vibrate_anim_state_ += (GetDriver()->GetLastFrameTime().count()/1000.f);
        if (this->vibrate_anim_state_ > 1.0f)
        {
            this->did_vibrate_ = false;
            this->vibrate_anim_state_ = 0.0f;
        }
    }

    //Setup pose for this frame.
    //auto newPose = IVRDevice::MakeDefaultPose();
    auto newPose = this->last_pose_;
    newPose.poseIsValid = true;
    newPose.result = vr::ETrackingResult::TrackingResult_Running_OK;

    ovrPoseStatef pose;
    unsigned int flags;
    if (oculus_device_type_ == HMD)
    {
        pose = sharedBuffer.oTrackingState.HeadPose;
        flags = sharedBuffer.oTrackingState.StatusFlags;
    }
    else if (oculus_device_type_ == Controller_Left)
    {
        pose = sharedBuffer.oTrackingState.HandPoses[ovrHandType::ovrHand_Left];
        flags = sharedBuffer.oTrackingState.HandStatusFlags[ovrHandType::ovrHand_Left];
    }
    else if (oculus_device_type_ == Controller_Right)
    {
        pose = sharedBuffer.oTrackingState.HandPoses[ovrHandType::ovrHand_Right];
        flags = sharedBuffer.oTrackingState.HandStatusFlags[ovrHandType::ovrHand_Right];
    }
    else if (oculus_device_type_ == Object)
    {
        int index = atoi(this->serial_.substr(13).c_str()); //13 -> "oculus_object"
        if (index > sharedBuffer.vrObjects.size()) return;
        pose = sharedBuffer.vrObjects[index];
        flags = ovrStatusBits_::ovrStatus_PositionValid | ovrStatusBits_::ovrStatus_OrientationValid;
    }
    else return;

    //Position.
    newPose.vecPosition[0] = pose.ThePose.Position.x;
    newPose.vecPosition[1] = pose.ThePose.Position.y;
    newPose.vecPosition[2] = pose.ThePose.Position.z;
    if (!(flags & ovrStatus_PositionTracked))
    {
        newPose.poseIsValid = false;
        newPose.result = vr::ETrackingResult::TrackingResult_Fallback_RotationOnly;
    }

    //Rotation.
    newPose.qRotation.w = pose.ThePose.Orientation.w;
    newPose.qRotation.x = pose.ThePose.Orientation.x;
    newPose.qRotation.y = pose.ThePose.Orientation.y;
    newPose.qRotation.z = pose.ThePose.Orientation.z;
    if (!(flags & ovrStatus_OrientationTracked))
    {
        newPose.poseIsValid = false;
        if (flags & ovrStatus_PositionTracked) newPose.result = vr::ETrackingResult::TrackingResult_Running_OutOfRange;
    }

    //Misc.
    /*newPose.vecVelocity[0] = pose.LinearVelocity.x;
    newPose.vecVelocity[1] = pose.LinearVelocity.y;
    newPose.vecVelocity[2] = pose.LinearVelocity.z;
    newPose.vecAcceleration[0] = pose.LinearAcceleration.x;
    newPose.vecAcceleration[1] = pose.LinearAcceleration.y;
    newPose.vecAcceleration[2] = pose.LinearAcceleration.z;
    newPose.vecAngularAcceleration[0] = pose.AngularAcceleration.x;
    newPose.vecAngularAcceleration[1] = pose.AngularAcceleration.y;
    newPose.vecAngularAcceleration[2] = pose.AngularAcceleration.z;
    newPose.vecAngularVelocity[0] = pose.AngularVelocity.x;
    newPose.vecAngularVelocity[1] = pose.AngularVelocity.y;
    newPose.vecAngularVelocity[2] = pose.AngularVelocity.z;*/
    newPose.qDriverFromHeadRotation = { 1, 0, 0, 0 };
    newPose.qWorldFromDriverRotation = { 1, 0, 0, 0 };
    newPose.poseTimeOffset = 0;

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, newPose, sizeof(vr::DriverPose_t));
    this->last_pose_ = newPose;
}

DeviceType OculusToSteamVR::TrackerDevice::GetDeviceType()
{
    return DeviceType::TRACKER;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::TrackerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError OculusToSteamVR::TrackerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracker " + this->serial_);

    //Get the properties handle.
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    //Set some universe ID (Must be 2 or higher).
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 31);

    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "Vive Tracker Pro MV");

    //Opt out of hand selection.
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);
    vr::VRProperties()->SetInt32Property(props, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_GenericTracker);
    vr::VRProperties()->SetInt32Property(props, vr::Prop_ControllerHandSelectionPriority_Int32, -1);

    //Set controller profile (not used I don't believe but without this OVR gets angry).
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{htc}/input/vive_tracker_profile.json");

    //Set a render model.
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "{htc}/rendermodels/vr_tracker_vive_1_0");

    //Icons.
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{htc}/icons/tracker_status_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{htc}/icons/tracker_status_off.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{htc}/icons/tracker_status_searching.gif");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{htc}/icons/tracker_status_searching_alert.gif");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{htc}/icons/tracker_status_ready_alert.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{htc}/icons/tracker_status_error.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{htc}/icons/tracker_status_standby.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{htc}/icons/tracker_status_ready_low.png");

    //https://github.com/SDraw/driver_kinectV2/blob/5931c33c41b726765cca84c0bbffb3a3f86efde8/driver_kinectV2/CTrackerVive.cpp
    vr::VRProperties()->SetStringProperty(props, vr::Prop_TrackingSystemName_String, "oculus");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_SerialNumber_String, (std::string("LHR-CB0CD0"/*0"*/) + std::to_string(unObjectId)).c_str());
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ManufacturerName_String, "HTC");
    vr::VRProperties()->SetStringProperty(props, vr::Prop_ResourceRoot_String, "htc");

    return vr::EVRInitError::VRInitError_None;
}

void OculusToSteamVR::TrackerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void OculusToSteamVR::TrackerDevice::EnterStandby()
{
}

void* OculusToSteamVR::TrackerDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    return nullptr;
}

void OculusToSteamVR::TrackerDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t OculusToSteamVR::TrackerDevice::GetPose()
{
    return last_pose_;
}
