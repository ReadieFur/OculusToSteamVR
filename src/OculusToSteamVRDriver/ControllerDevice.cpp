#include "ControllerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::ControllerDevice::ControllerDevice(std::string serial, ControllerDevice::Handedness handedness):
    serial_(serial),
    handedness_(handedness)
{
    this->oHandType_ = handedness == Handedness::LEFT ? ovrHandType::ovrHand_Left : ovrHandType::ovrHand_Right;
}

std::string OculusToSteamVR::ControllerDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::ControllerDevice::Update(SharedData sharedBuffer)
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

    ovrPoseStatef pose = sharedBuffer.oTrackingState.HandPoses[oHandType_];
    unsigned int flags = sharedBuffer.oTrackingState.HandStatusFlags[oHandType_];

#pragma region Offsets
    ovrQuatf inputOrientation = sharedBuffer.oTrackingState.HandPoses[oHandType_].ThePose.Orientation;
    ovrQuatf correctedOrientation = Helpers::OVRQuatFMul(inputOrientation, quatOffsets);
    ovrVector3f vectorOffsets = { 0,0,0 };
    //Apply left or right offset.
    if (oHandType_ == ovrHand_Right) vectorOffsets = Helpers::RotateVector2(rightVectorOffsets, inputOrientation);
    else
    {
        ovrVector3f leftVectorOffset = rightVectorOffsets;
        leftVectorOffset.x = -leftVectorOffset.x;
        vectorOffsets = Helpers::RotateVector2(leftVectorOffset, inputOrientation);
    }
#pragma endregion

    //Position.
    newPose.vecPosition[0] = pose.ThePose.Position.x + vectorOffsets.x;
    newPose.vecPosition[1] = pose.ThePose.Position.y + vectorOffsets.y;
    newPose.vecPosition[2] = pose.ThePose.Position.z + vectorOffsets.z;
    if (!(flags & ovrStatus_PositionTracked))
    {
        newPose.poseIsValid = false;
        newPose.result = vr::ETrackingResult::TrackingResult_Fallback_RotationOnly;
    }

    //Rotation.
    newPose.qRotation.w = correctedOrientation.w;
    newPose.qRotation.x = correctedOrientation.x;
    newPose.qRotation.y = correctedOrientation.y;
    newPose.qRotation.z = correctedOrientation.z;
    if (!(flags & ovrStatus_OrientationTracked))
    {
        newPose.poseIsValid = false;
        if (flags & ovrStatus_PositionTracked) newPose.result = vr::ETrackingResult::TrackingResult_Running_OutOfRange;
    }

    //Misc.
    newPose.vecVelocity[0] = pose.LinearVelocity.x;
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
    newPose.vecAngularVelocity[2] = pose.AngularVelocity.z;
    newPose.qDriverFromHeadRotation = { 1, 0, 0, 0 };
    newPose.qWorldFromDriverRotation = { 1, 0, 0, 0 };
    newPose.poseTimeOffset = 0;

    //Inputs.
    ovrInputState oInputState = sharedBuffer.oInputState[oHandType_];
    if (this->handedness_ == Handedness::LEFT)
    {
        GetDriver()->GetInput()->UpdateBooleanComponent(this->x_button_click_component_, oInputState.Buttons & ovrButton_X, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(this->x_button_touch_component_, oInputState.Touches & ovrTouch_X, 0);

        GetDriver()->GetInput()->UpdateBooleanComponent(this->y_button_click_component_, oInputState.Buttons & ovrButton_Y, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(this->y_button_touch_component_, oInputState.Touches & ovrTouch_Y, 0);

        GetDriver()->GetInput()->UpdateBooleanComponent(this->system_click_component_, oInputState.Buttons & ovrButton_Enter, 0);
    }
    else if (this->handedness_ == Handedness::RIGHT)
    {
        GetDriver()->GetInput()->UpdateBooleanComponent(this->a_button_click_component_, oInputState.Buttons & ovrButton_A, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(this->a_button_touch_component_, oInputState.Touches & ovrTouch_A, 0);

        GetDriver()->GetInput()->UpdateBooleanComponent(this->b_button_click_component_, oInputState.Buttons & ovrButton_B, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(this->b_button_touch_component_, oInputState.Touches & ovrTouch_B, 0);
    }
    GetDriver()->GetInput()->UpdateScalarComponent(this->trigger_value_component_, oInputState.IndexTrigger[oHandType_], 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(this->trigger_click_component_, oInputState.IndexTrigger[oHandType_] >= 0.9f, 0); //Allow for a small bit of click variation.
    GetDriver()->GetInput()->UpdateBooleanComponent(this->trigger_touch_component_, oInputState.Touches & (this->handedness_ == Handedness::LEFT ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger), 0);

    GetDriver()->GetInput()->UpdateScalarComponent(this->grip_value_component_, oInputState.HandTrigger[oHandType_], 0);

    GetDriver()->GetInput()->UpdateBooleanComponent(this->joystick_click_component_, oInputState.Buttons & (this->handedness_ == Handedness::LEFT ? ovrButton_LThumb : ovrButton_RThumb), 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(this->joystick_touch_component_, oInputState.Touches & (this->handedness_ == Handedness::LEFT ? ovrTouch_LThumb : ovrTouch_RThumb), 0);
    GetDriver()->GetInput()->UpdateScalarComponent(this->joystick_x_component_, oInputState.Thumbstick[oHandType_].x, 0);
    GetDriver()->GetInput()->UpdateScalarComponent(this->joystick_y_component_, oInputState.Thumbstick[oHandType_].y, 0);

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, newPose, sizeof(vr::DriverPose_t));
    this->last_pose_ = newPose;
}

DeviceType OculusToSteamVR::ControllerDevice::GetDeviceType()
{
    return DeviceType::CONTROLLER;
}

OculusToSteamVR::ControllerDevice::Handedness OculusToSteamVR::ControllerDevice::GetHandedness()
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

    //Get the properties handle.
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(this->device_index_);

    //Setup inputs and outputs.
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

    //Set some universe ID (Must be 2 or higher).
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 31);

    //Set up a model "number" (not needed but good to have).
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "oculus_touch");

    //Give SteamVR a hint at what hand this controller is for.
    if (this->handedness_ == Handedness::LEFT)
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "oculus_cv1_controller_left");
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
    }
    else if (this->handedness_ == Handedness::RIGHT)
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "oculus_cv1_controller_right");
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_RightHand);
    }
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "{htc}/rendermodels/vr_tracker_vive_1_0");

    //Set controller profile.
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{oculus}/input/touch_profile.json");

    //Icons.
    std::string handString = this->handedness_ == Handedness::LEFT ? "left" : "right";
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, ("{oculus}/icons/cv1_" + handString + "_controller_ready.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, ("{oculus}/icons/cv1_" + handString + "_controller_off.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, ("{oculus}/icons/cv1_" + handString + "_controller_searching.gif").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, ("{oculus}/icons/cv1_" + handString + "_controller_searching_alert.gif").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, ("{oculus}/icons/cv1_" + handString + "_controller_ready_alert.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, ("{oculus}/icons/cv1_" + handString + "_controller_error.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, ("{oculus}/icons/cv1_" + handString + "_controller_off.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, ("{oculus}/icons/cv1_" + handString + "_controller_ready_low.png").c_str());

    vr::VRProperties()->SetStringProperty(props, vr::Prop_TrackingSystemName_String, "oculus");

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
