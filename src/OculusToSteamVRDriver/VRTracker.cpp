#include "VRTracker.h"

#include "SharedVRProperties.h"

OculusToSteamVR_Driver::VRTracker::VRTracker(unsigned int objectIndex)
{
    //TODO: Impliment the headset as a tracker.
    //If the <objectIndex> is less than the <OBJECTS_OFFSET> then we are using the controllers.
    if (objectIndex < OBJECTS_OFFSET)
    {
        useOVRControllerData = true;
        ovrTrackerOffset = objectIndex - (CONTROLLERS_OFFSET);
    }
    else
    {
        useOVRControllerData = false;
        ovrTrackerOffset = objectIndex - (OBJECTS_OFFSET);
    }
	serialNumber = "ORT-0000000" + std::to_string(objectIndex + 5);
	modelNumber = "Oculus Rift CV1 Tracker " + std::to_string(objectIndex);
}

vr::EVRInitError OculusToSteamVR_Driver::VRTracker::Activate(uint32_t objectID)
{
	this->objectID = objectID;
    
    vr::PropertyContainerHandle_t properties = vr::VRProperties()->TrackedDeviceToPropertyContainer(objectID);

    SharedVRProperties::SetCommonProperties(properties, modelNumber, serialNumber);

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_RenderModelName_String, "{htc}vr_tracker_vive_1_0");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ModelNumber_String, "Vive Tracker Pro MV");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ManufacturerName_String, "HTC");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_HardwareRevision_String, "14");
    vr::VRProperties()->SetUint64Property(properties, vr::Prop_HardwareRevision_Uint64, 14U);
    vr::VRProperties()->SetInt32Property(properties, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_GenericTracker);
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ResourceRoot_String, "htc");

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_InputProfilePath_String, "{htc}/input/vive_tracker_profile.json");
    vr::VRProperties()->SetInt32Property(properties, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_Invalid);
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ControllerType_String, "vive_tracker");

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceOff_String, "{htc}/icons/tracker_status_off.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceSearching_String, "{htc}/icons/tracker_status_searching.gif");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{htc}/icons/tracker_status_searching_alert.gif");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceReady_String, "{htc}/icons/tracker_status_ready.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{htc}/icons/tracker_status_ready_alert.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceNotReady_String, "{htc}/icons/tracker_status_error.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceStandby_String, "{htc}/icons/tracker_status_standby.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceAlertLow_String, "{htc}/icons/tracker_status_ready_low.png");

    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasDisplayComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasCameraComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasDriverDirectModeComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(properties, vr::Prop_HasVirtualDisplayComponent_Bool, false);

    enabled = true;

    return vr::VRInitError_None;
}

//Ideally I would pass different data to be used in the update methods but this will have to do for now.
void OculusToSteamVR_Driver::VRTracker::RunFrame(SOculusData* oculusData)
{
    if (!enabled || objectID == vr::k_unTrackedDeviceIndexInvalid)
        return;

    vr::DriverPose_t pose = { 0 };
    pose.deviceIsConnected = true;
    pose.willDriftInYaw = false;
    pose.shouldApplyHeadModel = false;

    ovrPoseStatef ovrPose;
    if (useOVRControllerData)
    {
        ovrPose = oculusData->trackingState.HandPoses[ovrTrackerOffset];

        //Flags:
        //pos tracked and pos valid and rot tracked and rot valid -> running ok
        //pos not tracked and pos valid -> running out of range
		//rot not tracked and rot valid -> fallback rotation only
        //else -> out of range
        if (oculusData->trackingState.HandStatusFlags[ovrTrackerOffset] & (
            ovrStatusBits::ovrStatus_PositionTracked
            | ovrStatusBits::ovrStatus_PositionValid
            | ovrStatusBits::ovrStatus_OrientationTracked
            | ovrStatusBits::ovrStatus_OrientationValid))
        {
            pose.poseIsValid = true;
            pose.result = vr::TrackingResult_Running_OK;
        }
        else if (oculusData->trackingState.HandStatusFlags[ovrTrackerOffset] & ovrStatusBits::ovrStatus_PositionValid)
        {
			pose.poseIsValid = true;
			pose.result = vr::TrackingResult_Running_OutOfRange;
		}
		else if (oculusData->trackingState.HandStatusFlags[ovrTrackerOffset] & ovrStatusBits::ovrStatus_OrientationValid)
		{
			pose.poseIsValid = true;
			pose.result = vr::TrackingResult_Fallback_RotationOnly;
        }
        else
        {
			pose.poseIsValid = false;
            pose.result = vr::TrackingResult_Calibrating_OutOfRange;
        }
    }
    else
    {
		ovrPose = oculusData->objectPoses[ovrTrackerOffset];

        pose.poseIsValid = true;
		pose.result = vr::TrackingResult_Running_OK;
    }

	pose.qRotation.w = ovrPose.ThePose.Orientation.w;
	pose.qRotation.x = ovrPose.ThePose.Orientation.x;
	pose.qRotation.y = ovrPose.ThePose.Orientation.y;
	pose.qRotation.z = ovrPose.ThePose.Orientation.z;

	pose.vecPosition[0] = ovrPose.ThePose.Position.x;
	pose.vecPosition[1] = ovrPose.ThePose.Position.y;
	pose.vecPosition[2] = ovrPose.ThePose.Position.z;

	pose.vecAcceleration[0] = ovrPose.LinearAcceleration.x;
	pose.vecAcceleration[1] = ovrPose.LinearAcceleration.y;
	pose.vecAcceleration[2] = ovrPose.LinearAcceleration.z;

	pose.qWorldFromDriverRotation.w = 1;
	pose.qWorldFromDriverRotation.x = 0;
	pose.qWorldFromDriverRotation.y = 0;
	pose.qWorldFromDriverRotation.z = 0;

    pose.vecWorldFromDriverTranslation[0] = 0;
    pose.vecWorldFromDriverTranslation[1] = 0;
    pose.vecWorldFromDriverTranslation[2] = 0;

	pose.qDriverFromHeadRotation.w = 1;
	pose.qDriverFromHeadRotation.x = 0;
	pose.qDriverFromHeadRotation.y = 0;
	pose.qDriverFromHeadRotation.z = 0;

    pose.vecDriverFromHeadTranslation[0] = 0;
    pose.vecDriverFromHeadTranslation[1] = 0;
    pose.vecDriverFromHeadTranslation[2] = 0;
    
    pose.vecVelocity[0] = ovrPose.LinearVelocity.x;
    pose.vecVelocity[1] = ovrPose.LinearVelocity.y;
    pose.vecVelocity[2] = ovrPose.LinearVelocity.z;

	pose.poseTimeOffset = 0; //Let's let Oculus do it.

	pose.vecAngularAcceleration[0] = ovrPose.AngularAcceleration.x;
	pose.vecAngularAcceleration[1] = ovrPose.AngularAcceleration.y;
	pose.vecAngularAcceleration[2] = ovrPose.AngularAcceleration.z;

	pose.vecAngularVelocity[0] = ovrPose.AngularVelocity.x;
	pose.vecAngularVelocity[1] = ovrPose.AngularVelocity.y;
	pose.vecAngularVelocity[2] = ovrPose.AngularVelocity.z;

    vr::VRServerDriverHost()->TrackedDevicePoseUpdated(objectID, pose, sizeof(vr::DriverPose_t));
    lastPose = pose;
}
