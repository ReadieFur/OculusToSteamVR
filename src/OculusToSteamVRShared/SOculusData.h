#pragma once

#include <OVR_CAPI_D3D.h> //Oculus SDK.

#define UDP_SEND_RATE 90
#define MAX_OVR_SENSORS 4
#define MAX_OVR_OBJECTS 4

struct SOculusData
{
	uint8_t sensorCount;
	ovrTrackerPose sensorData[MAX_OVR_SENSORS];
	ovrTrackingState trackingState;
	ovrInputState inputState;
	uint8_t objectCount;
	ovrPoseStatef objectPoses[MAX_OVR_OBJECTS];
};
