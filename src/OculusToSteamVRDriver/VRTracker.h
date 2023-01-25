#pragma once

#include "IVRDevice.h"
#include <openvr_driver.h>
#include <string>
#include "SOculusData.h"

namespace OculusToSteamVR_Driver
{
	class VRTracker : public IVRDevice
	{
	public:
#pragma region IVRDevice
		VRTracker(unsigned int objectIndex);
		virtual vr::EVRInitError Activate(uint32_t objectId) override;
		virtual vr::ETrackedDeviceClass GetDeviceType() override { return vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker; }
		virtual void RunFrame(SOculusData* oculusData) override;
#pragma endregion

	private:
		bool useOVRControllerData;
		unsigned int ovrTrackerOffset;
    };
}
