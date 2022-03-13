#include "ControllerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::ControllerDevice::ControllerDevice(std::string serial, Handedness handedness):
    serial_(serial),
    handedness_(handedness)
{
}

std::string OculusToSteamVR::ControllerDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::ControllerDevice::Enable()
{
    if (enabled) { return; }
    GetDriver()->Log("Enabling controller " + this->serial_);
    enabled = true;
    auto pose = this->last_pose_;
    pose.deviceIsConnected = true;
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

void OculusToSteamVR::ControllerDevice::Disable()
{
    if (!enabled) { return; }
    GetDriver()->Log("Disabling controller " + this->serial_);
    enabled = false;
    auto pose = this->last_pose_;
    pose.deviceIsConnected = false;
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

void OculusToSteamVR::ControllerDevice::Update()
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
                this->did_identify_ = true;
            }
        }
        //}
    }

    //Check if we need to keep calibrating.
    if (this->did_identify_)
    {
        this->identify_anim_state_ += (GetDriver()->GetLastFrameTime().count() / 1000.f);
        if (this->identify_anim_state_ > 1.0f)
        {
            this->did_identify_ = false;
            this->identify_anim_state_ = 0.0f;
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

    //Input: https://developer.oculus.com/documentation/native/pc/dg-input-touch-buttons/
    ovrInputState inputState;
    if (OVR_SUCCESS(ovr_GetInputState(oculusVRSession, this->handedness_ == Handedness::LEFT ? ovrControllerType_LTouch : ovrControllerType_RTouch, &inputState)))
    {
        if (this->handedness_ == Handedness::LEFT)
        {
            GetDriver()->GetInput()->UpdateBooleanComponent(this->x_button_click_component_, inputState.Buttons& ovrButton_X, 0);
            GetDriver()->GetInput()->UpdateBooleanComponent(this->x_button_touch_component_, inputState.Touches& ovrTouch_X, 0);

            GetDriver()->GetInput()->UpdateBooleanComponent(this->y_button_click_component_, inputState.Buttons& ovrButton_Y, 0);
            GetDriver()->GetInput()->UpdateBooleanComponent(this->y_button_touch_component_, inputState.Touches& ovrTouch_Y, 0);

            GetDriver()->GetInput()->UpdateBooleanComponent(this->system_click_component_, inputState.Buttons& ovrButton_Enter, 0);
        }
        else if (this->handedness_ == Handedness::RIGHT)
        {
            GetDriver()->GetInput()->UpdateBooleanComponent(this->a_button_click_component_, inputState.Buttons& ovrButton_A, 0);
            GetDriver()->GetInput()->UpdateBooleanComponent(this->a_button_touch_component_, inputState.Touches& ovrTouch_A, 0);

            GetDriver()->GetInput()->UpdateBooleanComponent(this->b_button_click_component_, inputState.Buttons& ovrButton_B, 0);
            GetDriver()->GetInput()->UpdateBooleanComponent(this->b_button_touch_component_, inputState.Touches& ovrTouch_B, 0);
        }

        GetDriver()->GetInput()->UpdateScalarComponent(this->trigger_value_component_, inputState.IndexTrigger[controllerIndex], 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(this->trigger_click_component_, inputState.IndexTrigger[controllerIndex] >= 0.9f, 0); //Allow for a small bit of click variation.
        GetDriver()->GetInput()->UpdateBooleanComponent(this->trigger_touch_component_, inputState.Touches& (this->handedness_ == Handedness::LEFT ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger), 0);
    
        GetDriver()->GetInput()->UpdateScalarComponent(this->grip_value_component_, inputState.HandTrigger[controllerIndex], 0);

        GetDriver()->GetInput()->UpdateBooleanComponent(this->joystick_click_component_, inputState.Buttons& (this->handedness_ == Handedness::LEFT ? ovrButton_LThumb : ovrButton_RThumb), 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(this->joystick_touch_component_, inputState.Touches& (this->handedness_ == Handedness::LEFT ? ovrTouch_LThumb : ovrTouch_RThumb), 0);
        GetDriver()->GetInput()->UpdateScalarComponent(this->joystick_x_component_, inputState.Thumbstick[controllerIndex].x, 0);
        GetDriver()->GetInput()->UpdateScalarComponent(this->joystick_y_component_, inputState.Thumbstick[controllerIndex].y, 0);
    }

    //Post pose.
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
}

DeviceType OculusToSteamVR::ControllerDevice::GetDeviceType()
{
    return DeviceType::CONTROLLER;
}

OculusToSteamVR::Handedness OculusToSteamVR::ControllerDevice::GetHandedness()
{
    return this->handedness_;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::ControllerDevice::GetDeviceIndex()
{
    return this->device_index_;
}

vr::EVRInitError OculusToSteamVR::ControllerDevice::Activate(uint32_t unObjectId)
{
    this->device_index_ = unObjectId;

    GetDriver()->Log("Activating controller " + this->serial_);

    // Get the properties handle
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    if (this->handedness_ != Handedness::ANY)
    {
        // Setup inputs and outputs
        GetDriver()->GetInput()->CreateHapticComponent(props, "/output/haptic", &this->haptic_component_);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/click", &this->a_button_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/touch", &this->a_button_touch_component_);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/click", &this->b_button_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/touch", &this->b_button_touch_component_);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/click", &this->x_button_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/touch", &this->x_button_touch_component_);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/click", &this->y_button_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/touch", &this->x_button_touch_component_);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/click", &this->trigger_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/touch", &this->trigger_touch_component_);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/trigger/value", &this->trigger_value_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/grip/value", &this->grip_value_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click", &this->system_click_component_);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/click", &this->joystick_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/touch", &this->joystick_touch_component_);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/joystick/x", &this->joystick_x_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/joystick/y", &this->joystick_y_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
    }
    
    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "oculus_touch");

    const ovrHmdType oculusHMDType = ovr_GetHmdDesc(GetDriver()->oculusVRSession).Type;
    std::string oculusHMDString;
    switch (oculusHMDType)
    {
    case ovrHmdType::ovrHmd_CV1:
        oculusHMDString = "cv1";
        break;
    case ovrHmdType::ovrHmd_Quest:
        oculusHMDString = "quest";
        break;
    case ovrHmdType::ovrHmd_Quest2:
        oculusHMDString = "quest2";
        break;
    case ovrHmdType::ovrHmd_RiftS:
        oculusHMDString = "rifts";
        break;
    default:
        oculusHMDString = "cv1"; //Default to CV1 resources.
        break;
    }

    // Give SteamVR a hint at what hand this controller is for
    if (this->handedness_ == Handedness::LEFT)
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, ("oculus_" + oculusHMDString + "_controller_left").c_str());
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
    }
    else if (this->handedness_ == Handedness::RIGHT)
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, ("oculus_" + oculusHMDString + "_controller_right").c_str());
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_RightHand);
    }
    else
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "{oculus_to_steamvr}/rendermodels/example_controller");
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);
    }

    // Set controller profile
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{oculus}/input/touch_profile.json");

    //Icons
    std::string handString = this->handedness_ == Handedness::LEFT ? "left" : "right";
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_ready.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_off.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_searching.gif").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_searching_alert.gif").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_ready_alert.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_error.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_off.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, ("{oculus}/icons/" + oculusHMDString + "_" + handString + "_controller_ready_low.png").c_str());

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

void OculusToSteamVR::ControllerDevice::Deactivate()
{
    this->device_index_ = vr::k_unTrackedDeviceIndexInvalid;
}

void OculusToSteamVR::ControllerDevice::EnterStandby()
{
}

void* OculusToSteamVR::ControllerDevice::GetComponent(const char* pchComponentNameAndVersion)
{
    return nullptr;
}

void OculusToSteamVR::ControllerDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
    if (unResponseBufferSize >= 1)
        pchResponseBuffer[0] = 0;
}

vr::DriverPose_t OculusToSteamVR::ControllerDevice::GetPose()
{
    return last_pose_;
}
