#include "VRSensor.h"

#include "SharedVRProperties.h"

OculusToSteamVR_Driver::VRSensor::VRSensor(unsigned int ovrObjectIndex)
{
    ovrSensorOffset = ovrObjectIndex - SENSORS_OFFSET;
    serialNumber = "OSR-0000000" + std::to_string(ovrObjectIndex + 5);
    modelNumber = "Oculus Rift CV1 Sensor " + std::to_string(ovrObjectIndex);
}

vr::EVRInitError OculusToSteamVR_Driver::VRSensor::Activate(uint32_t objectID)
{
	this->objectID = objectID;

    vr::PropertyContainerHandle_t properties = vr::VRProperties()->TrackedDeviceToPropertyContainer(this->objectID);

    SharedVRProperties::SetCommonProperties(properties, modelNumber, serialNumber);

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_ModelNumber_String, "Oculus CV1 Sensor");
    
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_RenderModelName_String, "rift_camera");

    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceReady_String, "{oculus}/icons/cv1_camera_ready.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceOff_String, "{oculus}/icons/cv1_camera_off.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus}/icons/cv1_camera_searching.gif");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus}/icons/cv1_camera_searching_alert.gif");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus}/icons/cv1_camera_ready_alert.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus}/icons/cv1_camera_error.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus}/icons/cv1_camera_off.png");
    vr::VRProperties()->SetStringProperty(properties, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus}/icons/cv1_camera_ready_alert.png");

	enabled = true;

    return vr::VRInitError_None;
}

void OculusToSteamVR_Driver::VRSensor::RunFrame(SOculusData* oculusData)
{
    if (!enabled || objectID == vr::k_unTrackedDeviceIndexInvalid)
        return;

	vr::DriverPose_t pose = { 0 };
	ovrTrackerPose ovrPose = oculusData->sensorData[ovrSensorOffset];
    
    pose.deviceIsConnected = ((ovrPose.TrackerFlags & ovrTrackerFlags::ovrTracker_Connected) == ovrTrackerFlags::ovrTracker_Connected);
	pose.poseIsValid = ((ovrPose.TrackerFlags & ovrTrackerFlags::ovrTracker_PoseTracked) == ovrTrackerFlags::ovrTracker_PoseTracked);
	if (!pose.deviceIsConnected)
		pose.result = vr::TrackingResult_Uninitialized;
	else if (!pose.poseIsValid)
		pose.result = vr::TrackingResult_Running_OutOfRange;
	else
		pose.result = vr::TrackingResult_Running_OK;

	pose.qRotation.w = ovrPose.Pose.Orientation.w;
	pose.qRotation.x = ovrPose.Pose.Orientation.x;
	pose.qRotation.y = ovrPose.Pose.Orientation.y;
	pose.qRotation.z = ovrPose.Pose.Orientation.z;

	pose.vecPosition[0] = ovrPose.Pose.Position.x;
	pose.vecPosition[1] = ovrPose.Pose.Position.y;
	pose.vecPosition[2] = ovrPose.Pose.Position.z;

	pose.qWorldFromDriverRotation.w = 1;
	pose.qWorldFromDriverRotation.x = 0;
	pose.qWorldFromDriverRotation.y = 0;
	pose.qWorldFromDriverRotation.z = 0;

	pose.qDriverFromHeadRotation.w = 1;
    pose.qDriverFromHeadRotation.x = 0;
	pose.qDriverFromHeadRotation.y = 0;
	pose.qDriverFromHeadRotation.z = 0;

	vr::VRServerDriverHost()->TrackedDevicePoseUpdated(objectID, pose, sizeof(vr::DriverPose_t));
	lastPose = pose;
}
