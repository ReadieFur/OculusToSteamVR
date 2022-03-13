#include "TrackerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::TrackerDevice::TrackerDevice(std::string serial, Handedness handedness):
    serial_(serial),
    handedness_(handedness)
{
}

std::string OculusToSteamVR::TrackerDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::TrackerDevice::Enable()
{
    if (enabled) { return; }
    GetDriver()->Log("Enabling tracker " + this->serial_);
    enabled = true;
    auto pose = this->last_pose_;
    pose.deviceIsConnected = true;
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

void OculusToSteamVR::TrackerDevice::Disable()
{
    if (!enabled) { return; }
    GetDriver()->Log("Disabling tracker " + this->serial_);
    enabled = false;
    auto pose = this->last_pose_;
    pose.deviceIsConnected = false;
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

void OculusToSteamVR::TrackerDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid || !enabled)
        return;

    // Check if this device was asked to be identified
    auto events = GetDriver()->GetOpenVREvents();
    for (auto event : events) {
        // Note here, event.trackedDeviceIndex does not necissarily equal this->device_index_, not sure why, but the component handle will match so we can just use that instead
        //if (event.trackedDeviceIndex == this->device_index_) {
        if (event.eventType == vr::EVREventType::VREvent_Input_HapticVibration) {
            if (event.data.hapticVibration.componentHandle == this->haptic_component_) {
                this->did_vibrate_ = true;
            }
        }
        //}
    }

    // Check if we need to keep vibrating
    if (this->did_vibrate_) {
        this->vibrate_anim_state_ += (GetDriver()->GetLastFrameTime().count()/1000.f);
        if (this->vibrate_anim_state_ > 1.0f) {
            this->did_vibrate_ = false;
            this->vibrate_anim_state_ = 0.0f;
        }
    }

    const int controllerIndex = this->handedness_ == Handedness::LEFT ? 0 : 1;
    const ovrSession oculusVRSession = GetDriver()->oculusVRSession;
    ovrTrackingState oculusVRTrackingState = ovr_GetTrackingState(oculusVRSession, ovr_GetPredictedDisplayTime(oculusVRSession, 0), ovrTrue);
    auto pose = this->last_pose_;

    if (this->handedness_ == Handedness::ANY)
    {
        //Position
        if (oculusVRTrackingState.StatusFlags & ovrStatus_PositionTracked)
        {
            pose.vecPosition[0] = oculusVRTrackingState.HeadPose.ThePose.Position.x - GetDriver()->rightOffset.Translation.x;
            pose.vecPosition[1] = oculusVRTrackingState.HeadPose.ThePose.Position.y - GetDriver()->rightOffset.Translation.y;
            pose.vecPosition[2] = oculusVRTrackingState.HeadPose.ThePose.Position.z - GetDriver()->rightOffset.Translation.z;
        }
        else
        {
            pose.poseIsValid = false;
            pose.result = vr::ETrackingResult::TrackingResult_Fallback_RotationOnly;
        }
        //Rotation
        if (oculusVRTrackingState.StatusFlags & ovrStatus_OrientationTracked)
        {
            pose.qRotation.w = oculusVRTrackingState.HeadPose.ThePose.Orientation.w;
            pose.qRotation.x = oculusVRTrackingState.HeadPose.ThePose.Orientation.x;
            pose.qRotation.y = oculusVRTrackingState.HeadPose.ThePose.Orientation.y;
            pose.qRotation.z = oculusVRTrackingState.HeadPose.ThePose.Orientation.z;
        }
        else
        {
            pose.poseIsValid = false;
            if (oculusVRTrackingState.StatusFlags & ovrStatus_PositionTracked) { pose.result = vr::ETrackingResult::TrackingResult_Running_OutOfRange; }
        }
        GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
        this->last_pose_ = pose;
        return;
    }

    //Set default values.
    pose.poseIsValid = true;
    pose.result = vr::ETrackingResult::TrackingResult_Running_OK;

    //Position
    if (oculusVRTrackingState.HandStatusFlags[controllerIndex] & ovrStatus_PositionTracked)
    {
        //Translation should be the same on both sides.
        pose.vecPosition[0] = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.x - GetDriver()->rightOffset.Translation.x;
        pose.vecPosition[1] = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.y - GetDriver()->rightOffset.Translation.y;
        pose.vecPosition[2] = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.z - GetDriver()->rightOffset.Translation.z;
    }
    else
    {
        pose.poseIsValid = false;
        pose.result = vr::ETrackingResult::TrackingResult_Fallback_RotationOnly;
    }

    //Rotation
    if (oculusVRTrackingState.HandStatusFlags[controllerIndex] & ovrStatus_OrientationTracked)
    {
        OVR::Posef oculusVRPose = oculusVRTrackingState.HandPoses[controllerIndex].ThePose; //I didn't know how to cast this to OVR::Posef directly so I have assigned it to a variable here.
        OVR::Quatf poseWithOffset = oculusVRPose.Rotation * (this->handedness_ == Handedness::LEFT ? GetDriver()->leftOffset : GetDriver()->rightOffset).Rotation;
        pose.qRotation.w = poseWithOffset.w;
        pose.qRotation.x = poseWithOffset.x;
        pose.qRotation.y = poseWithOffset.y;
        pose.qRotation.z = poseWithOffset.z;
    }
    else
    {
        pose.poseIsValid = false;
        if (oculusVRTrackingState.HandStatusFlags[controllerIndex] & ovrStatus_PositionTracked) { pose.result = vr::ETrackingResult::TrackingResult_Running_OutOfRange; }
    }

    //Post pose.
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType OculusToSteamVR::TrackerDevice::GetDeviceType()
{
    return DeviceType::TRACKER;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::TrackerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

//I originally was going to use the same oculus assets for the icons and models here but to avoid confusion I will use the regular Vive tracker icons.
vr::EVRInitError OculusToSteamVR::TrackerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating tracker " + this->serial_);

    // Get the properties handle
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "oculus_touch_tracker");

    // Opt out of hand selection
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);

    // Set controller profile (not used I don't believe but without this OVR gets angry).
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{htc}/input/vive_tracker_profile.json");

    // Set a render model.
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "{htc}/rendermodels/vr_tracker_vive_1_0");

    //Icons
    std::string handString = this->handedness_ == Handedness::LEFT ? "left" : "right";
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, "{htc}/icons/tracker_status_ready.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, "{htc}/icons/tracker_status_off.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, "{htc}/icons/tracker_status_searching.gif");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{htc}/icons/tracker_status_searching_alert.gif");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{htc}/icons/tracker_status_ready_alert.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, "{htc}/icons/tracker_status_error.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, "{htc}/icons/tracker_status_standby.png");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, "{htc}/icons/tracker_status_ready_low.png");

    vr::DriverPose_t defaultPose = IVRDevice::MakeDefaultPose();
    defaultPose.vecPosition[0] =
        defaultPose.vecPosition[1] =
        defaultPose.vecPosition[2] =
        defaultPose.qRotation.x =
        defaultPose.qRotation.y =
        defaultPose.qRotation.z =
        0;
    defaultPose.qRotation.w = 1;
    defaultPose.deviceIsConnected = enabled;
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, defaultPose, sizeof(vr::DriverPose_t));
    this->last_pose_ = defaultPose;

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
