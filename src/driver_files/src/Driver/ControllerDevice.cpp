#include "ControllerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::ControllerDevice::ControllerDevice(std::string serial, ControllerDevice::Handedness handedness):
    serial_(serial),
    handedness_(handedness)
{
}

std::string OculusToSteamVR::ControllerDevice::GetSerial()
{
    return this->serial_;
}

void OculusToSteamVR::ControllerDevice::Update()
{
    if (this->device_index_ == vr::k_unTrackedDeviceIndexInvalid)
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

    const int controllerIndex = this->handedness_ == Handedness::LEFT ? 0 : 1;
    const ovrSession oculusVRSession = GetDriver()->oculusVRSession;
    ovrTrackingState oculusVRTrackingState = ovr_GetTrackingState(oculusVRSession, ovr_GetPredictedDisplayTime(oculusVRSession, 0), ovrTrue);

    if (this->handedness_ == Handedness::ANY)
    {
        auto pose = this->last_pose_;
        if (oculusVRTrackingState.StatusFlags & ovrStatus_PositionTracked)
        {
            pose.vecPosition[0] = oculusVRTrackingState.HeadPose.ThePose.Position.x;
            pose.vecPosition[1] = oculusVRTrackingState.HeadPose.ThePose.Position.y;
            pose.vecPosition[2] = oculusVRTrackingState.HeadPose.ThePose.Position.z;
        }
        GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
        this->last_pose_ = pose;
        return;
    }

    //Check if we need to keep calibrating.
    if (this->did_identify_)
    {
        this->identify_anim_state_ += (GetDriver()->GetLastFrameTime().count() / 1000.f);
        if (this->identify_anim_state_ > 1.0f)
        {
            ovrPosef averageOffset = this->calibrationSampleData;
            if (this->calibrationSamples != 0)
            {
                averageOffset.Position.x = averageOffset.Position.x != 0 ? averageOffset.Position.x / this->calibrationSamples : 0;
                averageOffset.Position.y = averageOffset.Position.y != 0 ? averageOffset.Position.y / this->calibrationSamples : 0;
                averageOffset.Position.z = averageOffset.Position.z != 0 ? averageOffset.Position.z / this->calibrationSamples : 0;
                averageOffset.Orientation.w = averageOffset.Orientation.w != 0 ? averageOffset.Orientation.w / this->calibrationSamples : 0;
                averageOffset.Orientation.x = averageOffset.Orientation.x != 0 ? averageOffset.Orientation.x / this->calibrationSamples : 0;
                averageOffset.Orientation.y = averageOffset.Orientation.y != 0 ? averageOffset.Orientation.y / this->calibrationSamples : 0;
                averageOffset.Orientation.z = averageOffset.Orientation.z != 0 ? averageOffset.Orientation.z / this->calibrationSamples : 0;
            }
            else
            {
                averageOffset.Position.x =
                    averageOffset.Position.y =
                    averageOffset.Position.z =
                    averageOffset.Orientation.w =
                    averageOffset.Orientation.x =
                    averageOffset.Orientation.y =
                    averageOffset.Orientation.z =
                    0;
            }

            GetDriver()->offset = averageOffset;
            GetDriver()->Log(
                "Offset: x=" + std::to_string(averageOffset.Position.x) +
                " y=" + std::to_string(averageOffset.Position.y) +
                " z=" + std::to_string(averageOffset.Position.z)
            );

            this->did_identify_ = false;
            this->identify_anim_state_ = 0.0f;
            this->calibrationSamples = 0;
            this->calibrationSampleData.Position.x =
                this->calibrationSampleData.Position.y =
                this->calibrationSampleData.Position.z =
                this->calibrationSampleData.Orientation.w =
                this->calibrationSampleData.Orientation.x =
                this->calibrationSampleData.Orientation.y =
                this->calibrationSampleData.Orientation.z =
                0;
        }
        else
        {
            if (oculusVRTrackingState.HandStatusFlags[controllerIndex] & ovrStatus_PositionTracked)
            {
                this->calibrationSamples++;
                this->calibrationSampleData.Position.x += oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.x;
                this->calibrationSampleData.Position.y += oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.y;
                this->calibrationSampleData.Position.z += oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.z;
                this->calibrationSampleData.Orientation.w += oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.w;
                this->calibrationSampleData.Orientation.x += oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.x;
                this->calibrationSampleData.Orientation.y += oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.y;
                this->calibrationSampleData.Orientation.z += oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.z;
            }
        }
    }

    //Update the pose for this frame.
    auto pose = this->last_pose_;
    const ovrPosef offset = GetDriver()->offset;
    if (oculusVRTrackingState.HandStatusFlags[controllerIndex] & ovrStatus_PositionTracked)
    {
        pose.vecPosition[0] = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.x - offset.Position.x;
        pose.vecPosition[1] = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.y - offset.Position.y;
        pose.vecPosition[2] = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.z - offset.Position.z;

        /*GetDriver()->Log(
            "Oculus: x=" + std::to_string(oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.x) +
            " y=" + std::to_string(oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.y) +
            " z=" + std::to_string(oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Position.z) +
            " With offset: x=" + std::to_string(pose.vecPosition[0]) +
            " y=" + std::to_string(pose.vecPosition[1]) +
            " z=" + std::to_string(pose.vecPosition[2])
        );*/
    }

    if (oculusVRTrackingState.HandStatusFlags[controllerIndex] & ovrStatus_OrientationTracked)
    {
        pose.qRotation.w = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.w;
        pose.qRotation.x = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.x;
        pose.qRotation.y = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.y;
        pose.qRotation.z = oculusVRTrackingState.HandPoses[controllerIndex].ThePose.Orientation.z;
    }

    //Post pose.
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(this->device_index_, pose, sizeof(vr::DriverPose_t));
    this->last_pose_ = pose;
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

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/click", &this->trigger_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/touch", &this->trigger_touch_component_);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/trigger/value", &this->trigger_value_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/grip/touch", &this->grip_touch_component_);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/grip/value", &this->grip_value_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/grip/force", &this->grip_force_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click", &this->system_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/touch", &this->system_touch_component_);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trackpad/click", &this->trackpad_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trackpad/touch", &this->trackpad_touch_component_);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/trackpad/x", &this->trackpad_x_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/trackpad/y", &this->trackpad_y_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);

        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/click", &this->joystick_click_component_);
        GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/touch", &this->joystick_touch_component_);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/joystick/x", &this->joystick_x_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
        GetDriver()->GetInput()->CreateScalarComponent(props, "/input/joystick/y", &this->joystick_y_component_, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
    }
    
    // Set some universe ID (Must be 2 or higher)
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 2);
    
    // Set up a model "number" (not needed but good to have)
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "example_controller");

    // Set up a render model path
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "{oculus_to_steamvr}/rendermodels/example_controller");

    // Give SteamVR a hint at what hand this controller is for
    if (this->handedness_ == Handedness::LEFT) {
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
    }
    else if (this->handedness_ == Handedness::RIGHT) {
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_RightHand);
    }
    else {
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);
    }

    // Set controller profile
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{oculus_to_steamvr}/input/example_controller_bindings.json");

    // Change the icon depending on which handedness this controller is using (ANY uses right)
    std::string controller_ready_file;
    std::string controller_not_ready_file;
    if (this->handedness_ != Handedness::ANY)
    {
        std::string controller_handedness_str = this->handedness_ == Handedness::LEFT ? "left" : "right";
        controller_ready_file = "{oculus_to_steamvr}/icons/controller_ready_" + controller_handedness_str + ".png";
        controller_not_ready_file = "{oculus_to_steamvr}/icons/controller_not_ready_" + controller_handedness_str + ".png";
    }
    else
    {
        controller_ready_file = "{oculus_to_steamvr}/icons/trackingreference_ready.png";
        controller_not_ready_file = "{oculus_to_steamvr}/icons/trackingreference_not_ready.png";
    }

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, controller_ready_file.c_str());

    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, controller_not_ready_file.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, controller_not_ready_file.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, controller_not_ready_file.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, controller_not_ready_file.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, controller_not_ready_file.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, controller_not_ready_file.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, controller_not_ready_file.c_str());

    vr::DriverPose_t defaultPose = IVRDevice::MakeDefaultPose();
    defaultPose.vecPosition[0] =
        defaultPose.vecPosition[1] =
        defaultPose.vecPosition[2] =
        defaultPose.qRotation.x =
        defaultPose.qRotation.y =
        defaultPose.qRotation.z =
        0;
    defaultPose.qRotation.w = 1;
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
