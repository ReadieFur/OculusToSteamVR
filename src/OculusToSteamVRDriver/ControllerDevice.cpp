#include "ControllerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::ControllerDevice::ControllerDevice(unsigned int index, ControllerDevice::Handedness handedness):
    handedness(handedness)
{
    //OSC -> Oculus Steam Controller.
    serial = "OSC-" + Helpers::GetSerialSuffix(index);
    oHandType = handedness == Handedness::LEFT ? ovrHandType::ovrHand_Left : ovrHandType::ovrHand_Right;
}

std::string OculusToSteamVR::ControllerDevice::GetSerial()
{
    return serial;
}

void OculusToSteamVR::ControllerDevice::Update(SharedData* sharedBuffer)
{
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) return;

    auto events = GetDriver()->GetOpenVREvents();
    for (auto event : events)
    {
        if (event.eventType == vr::EVREventType::VREvent_Input_HapticVibration
            && event.data.hapticVibration.componentHandle == hapticComponent
            && (std::chrono::high_resolution_clock::now() > lastVibrationTime
                || event.data.hapticVibration.fDurationSeconds != currentVibration.fDurationSeconds
                || event.data.hapticVibration.fAmplitude != currentVibration.fAmplitude
                || event.data.hapticVibration.fFrequency != currentVibration.fFrequency
                )
        )
        {
            lastVibrationTime = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds{ static_cast<int>(event.data.hapticVibration.fDurationSeconds / 1000)};
            currentVibration = event.data.hapticVibration;
            sharedBuffer->cHapticEventInfo[oHandType] =
            {
                true,
                event.data.hapticVibration.fDurationSeconds,
                event.data.hapticVibration.fAmplitude,
                event.data.hapticVibration.fFrequency
            };
        }
    }

    //Setup pose for this frame.
    //auto newPose = IVRDevice::MakeDefaultPose();
    auto newPose = lastPose;
    newPose.poseIsValid = true;
    newPose.result = vr::ETrackingResult::TrackingResult_Running_OK;
    newPose.qDriverFromHeadRotation = { 1, 0, 0, 0 };
    newPose.qWorldFromDriverRotation = { 1, 0, 0, 0 };
    newPose.poseTimeOffset = 0;

    ovrPoseStatef pose = sharedBuffer->oTrackingState.HandPoses[oHandType];
    unsigned int flags = sharedBuffer->oTrackingState.HandStatusFlags[oHandType];

#pragma region Offsets
    ovrQuatf inputOrientation = sharedBuffer->oTrackingState.HandPoses[oHandType].ThePose.Orientation;
    ovrQuatf correctedOrientation = Helpers::OVRQuatFMul(inputOrientation, quatOffsets);
    ovrVector3f vectorOffsets = { 0,0,0 };
    //Apply left or right offset.
    if (oHandType == ovrHand_Right) vectorOffsets = Helpers::RotateVector2(rightVectorOffsets, inputOrientation);
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

    //Inputs.
    ovrInputState oInputState = sharedBuffer->oInputState[oHandType];
    if (handedness == Handedness::LEFT)
    {
        GetDriver()->GetInput()->UpdateBooleanComponent(xButtonClickComponent, oInputState.Buttons & ovrButton_X, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(xButtonTouchComponent, oInputState.Touches & ovrTouch_X, 0);

        GetDriver()->GetInput()->UpdateBooleanComponent(yButtonClickComponent, oInputState.Buttons & ovrButton_Y, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(yButtonTouchComponent, oInputState.Touches & ovrTouch_Y, 0);

        GetDriver()->GetInput()->UpdateBooleanComponent(systemClickComponent, oInputState.Buttons & ovrButton_Enter, 0);
    }
    else if (handedness == Handedness::RIGHT)
    {
        GetDriver()->GetInput()->UpdateBooleanComponent(aButtonClickComponent, oInputState.Buttons & ovrButton_A, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(aButtonTouchComponent, oInputState.Touches & ovrTouch_A, 0);

        GetDriver()->GetInput()->UpdateBooleanComponent(bButtonClickComponent, oInputState.Buttons & ovrButton_B, 0);
        GetDriver()->GetInput()->UpdateBooleanComponent(bButtonTouchComponent, oInputState.Touches & ovrTouch_B, 0);
    }
    GetDriver()->GetInput()->UpdateScalarComponent(triggerValueComponent, oInputState.IndexTrigger[oHandType], 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(triggerClickComponent, oInputState.IndexTrigger[oHandType] >= 0.95f, 0); //Allow for a small bit of click variation.
    GetDriver()->GetInput()->UpdateBooleanComponent(triggerTouchComponent, oInputState.Touches & (handedness == Handedness::LEFT ? ovrTouch_LIndexTrigger : ovrTouch_RIndexTrigger), 0);

    GetDriver()->GetInput()->UpdateScalarComponent(gripValueComponent, oInputState.HandTrigger[oHandType], 0);

    GetDriver()->GetInput()->UpdateBooleanComponent(joystickClickComponent, oInputState.Buttons & (handedness == Handedness::LEFT ? ovrButton_LThumb : ovrButton_RThumb), 0);
    GetDriver()->GetInput()->UpdateBooleanComponent(joystickTouchComponent, oInputState.Touches & (handedness == Handedness::LEFT ? ovrTouch_LThumb : ovrTouch_RThumb), 0);
    GetDriver()->GetInput()->UpdateScalarComponent(joystickXComponent, oInputState.Thumbstick[oHandType].x, 0);
    GetDriver()->GetInput()->UpdateScalarComponent(joystickYComponent, oInputState.Thumbstick[oHandType].y, 0);

    // Post pose
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(deviceIndex, newPose, sizeof(vr::DriverPose_t));
    lastPose = newPose;
}

DeviceType OculusToSteamVR::ControllerDevice::GetDeviceType()
{
    return DeviceType::CONTROLLER;
}

OculusToSteamVR::ControllerDevice::Handedness OculusToSteamVR::ControllerDevice::GetHandedness()
{
    return handedness;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::ControllerDevice::GetDeviceIndex()
{
    return deviceIndex;
}

vr::EVRInitError OculusToSteamVR::ControllerDevice::Activate(uint32_t unObjectId)
{
    deviceIndex = unObjectId;

    GetDriver()->Log("Activating controller " + serial);

    //Get the properties handle.
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(deviceIndex);

    //Setup inputs and outputs.
    GetDriver()->GetInput()->CreateHapticComponent(props, "/output/haptic", &hapticComponent);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/click", &aButtonClickComponent);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/a/touch", &aButtonTouchComponent);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/click", &bButtonClickComponent);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/b/touch", &bButtonTouchComponent);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/click", &xButtonClickComponent);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/x/touch", &xButtonTouchComponent);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/click", &yButtonClickComponent);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/y/touch", &xButtonTouchComponent);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/click", &triggerClickComponent);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/trigger/touch", &triggerTouchComponent);
    GetDriver()->GetInput()->CreateScalarComponent(props, "/input/trigger/value", &triggerValueComponent, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

    GetDriver()->GetInput()->CreateScalarComponent(props, "/input/grip/value", &gripValueComponent, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/system/click", &systemClickComponent);

    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/click", &joystickClickComponent);
    GetDriver()->GetInput()->CreateBooleanComponent(props, "/input/joystick/touch", &joystickTouchComponent);
    GetDriver()->GetInput()->CreateScalarComponent(props, "/input/joystick/x", &joystickXComponent, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);
    GetDriver()->GetInput()->CreateScalarComponent(props, "/input/joystick/y", &joystickYComponent, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedTwoSided);

    //Set some universe ID (Must be 2 or higher).
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 31);
    vr::VRProperties()->SetStringProperty(props, vr::Prop_TrackingSystemName_String, "oculus");

    //Give SteamVR a hint at what hand this controller is for.
    if (handedness == Handedness::LEFT)
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "Oculus Touch Left");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "oculus_cv1_controller_left");
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
    }
    else if (handedness == Handedness::RIGHT)
    {
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, "Oculus Touch Right");
        GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_RenderModelName_String, "oculus_cv1_controller_right");
        GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_RightHand);
    }
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_SerialNumber_String, serial.c_str());

    //Set controller profile.
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_InputProfilePath_String, "{oculus}/input/touch_profile.json");

    //Icons.
    std::string handString = handedness == Handedness::LEFT ? "left" : "right";
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReady_String, ("{oculus}/icons/cv1_" + handString + "_controller_ready.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceOff_String, ("{oculus}/icons/cv1_" + handString + "_controller_off.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearching_String, ("{oculus}/icons/cv1_" + handString + "_controller_searching.gif").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceSearchingAlert_String, ("{oculus}/icons/cv1_" + handString + "_controller_searching_alert.gif").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceReadyAlert_String, ("{oculus}/icons/cv1_" + handString + "_controller_ready_alert.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceNotReady_String, ("{oculus}/icons/cv1_" + handString + "_controller_error.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceStandby_String, ("{oculus}/icons/cv1_" + handString + "_controller_off.png").c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_NamedIconPathDeviceAlertLow_String, ("{oculus}/icons/cv1_" + handString + "_controller_ready_low.png").c_str());

    return vr::EVRInitError::VRInitError_None;
}

void OculusToSteamVR::ControllerDevice::Deactivate()
{
    deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
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
    return lastPose;
}
