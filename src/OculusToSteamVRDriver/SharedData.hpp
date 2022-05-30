#pragma once

#include <OVR_CAPI.h>
#include <map>

struct SharedData
{
	ovrTrackingState oTrackingState;
	ovrInputState oInputState[2];
	unsigned int vrObjectsCount;
	std::map<int, ovrPoseStatef> vrObjectsPose;
};