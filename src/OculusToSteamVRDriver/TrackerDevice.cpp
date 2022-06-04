#include "TrackerDevice.hpp"
#include <Windows.h>

OculusToSteamVR::TrackerDevice::TrackerDevice(unsigned int index, OculusDeviceType oculusDeviceType):
    index(index),
    oculusDeviceType(oculusDeviceType)
{
    //OSC -> Oculus Steam Tracker.
    serial = "OST-" + Helpers::GetSerialSuffix(index);
}

std::string OculusToSteamVR::TrackerDevice::GetSerial()
{
    return serial;
}

void OculusToSteamVR::TrackerDevice::Update(SharedData* sharedBuffer)
{
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) return;

    //Setup pose for this frame.
    //auto newPose = IVRDevice::MakeDefaultPose();
    auto newPose = lastPose;
    newPose.poseIsValid = true;
    newPose.result = vr::ETrackingResult::TrackingResult_Running_OK;
    newPose.qDriverFromHeadRotation = { 1, 0, 0, 0 };
    newPose.qWorldFromDriverRotation = { 1, 0, 0, 0 };
    newPose.poseTimeOffset = 0;

    ovrPoseStatef pose;
    unsigned int flags;
    if (oculusDeviceType == HMD)
    {
        pose = sharedBuffer->oTrackingState.HeadPose;
        flags = sharedBuffer->oTrackingState.StatusFlags;
    }
    else if (oculusDeviceType == ControllerLeft)
    {
        pose = sharedBuffer->oTrackingState.HandPoses[ovrHandType::ovrHand_Left];
        flags = sharedBuffer->oTrackingState.HandStatusFlags[ovrHandType::ovrHand_Left];
    }
    else if (oculusDeviceType == ControllerRight)
    {
        pose = sharedBuffer->oTrackingState.HandPoses[ovrHandType::ovrHand_Right];
        flags = sharedBuffer->oTrackingState.HandStatusFlags[ovrHandType::ovrHand_Right];
    }
    else if (oculusDeviceType == Object)
    {
        int index = atoi(serial.substr(13).c_str()); //13 -> "oculus_object"
        if (index > sharedBuffer->vrObjectsCount) return;
        pose = sharedBuffer->vrObjects[index];
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

    //Post pose.
    GetDriver()->GetDriverHost()->TrackedDevicePoseUpdated(deviceIndex, newPose, sizeof(vr::DriverPose_t));
    lastPose = newPose;
}

DeviceType OculusToSteamVR::TrackerDevice::GetDeviceType()
{
    return DeviceType::TRACKER;
}

vr::TrackedDeviceIndex_t OculusToSteamVR::TrackerDevice::GetDeviceIndex()
{
    return deviceIndex;
}

vr::EVRInitError OculusToSteamVR::TrackerDevice::Activate(uint32_t unObjectId)
{
    deviceIndex = unObjectId;

    GetDriver()->Log("Activating tracker " + serial);

    //Get the properties handle.
    auto props = GetDriver()->GetProperties()->TrackedDeviceToPropertyContainer(deviceIndex);

    //Set some universe ID (Must be 2 or higher).
    GetDriver()->GetProperties()->SetUint64Property(props, vr::Prop_CurrentUniverseId_Uint64, 31);
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_TrackingSystemName_String, "oculus");

    // Set up a model "number" (not needed but good to have).
    std::string modelString = "Oculus ";
    if (oculusDeviceType == HMD) modelString += "HMD";
    else if (oculusDeviceType == ControllerLeft) modelString += "Touch Left";
    else if (oculusDeviceType == ControllerRight) modelString += "Touch Right";
    else if (oculusDeviceType == Object) modelString += "Object " + std::to_string(index);
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ModelNumber_String, modelString.c_str());

    //Opt out of hand selection.
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::ETrackedControllerRole::TrackedControllerRole_OptOut);
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_GenericTracker);
    GetDriver()->GetProperties()->SetInt32Property(props, vr::Prop_ControllerHandSelectionPriority_Int32, -1);

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
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_SerialNumber_String, serial.c_str());
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ManufacturerName_String, "HTC");
    GetDriver()->GetProperties()->SetStringProperty(props, vr::Prop_ResourceRoot_String, "htc");

    return vr::EVRInitError::VRInitError_None;
}

void OculusToSteamVR::TrackerDevice::Deactivate()
{
    deviceIndex = vr::k_unTrackedDeviceIndexInvalid;
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
    return lastPose;
}
