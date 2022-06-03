#pragma once

#include <Windows.h>
#include <OVR_CAPI.h>

struct HapticEventInfo
{
	bool shouldVibrate;
	float duration;
	float amplitude;
	float frequency;
};

struct SharedData
{
	std::chrono::steady_clock::time_point clientTime;
	//std::string logBuffer; //Broken.
	ovrTrackingState oTrackingState;
	ovrInputState oInputState[2];
	unsigned int vrObjectsCount;
	ovrPoseStatef vrObjects[4]; //For now we will only support up to 4 objects. (Im not too sure how to resize the shared memeory so a vector wouldn't work).
	unsigned int trackingRefrencesCount;
	ovrTrackerPose trackingRefrences[4];
	HapticEventInfo cHapticEventInfo[2]; //For the controllers only as of now.
};