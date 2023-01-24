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
		VRTracker(unsigned int ovrObjectIndex);
		virtual vr::EVRInitError Activate(uint32_t objectId) override;
		virtual vr::ETrackedDeviceClass GetDeviceType() override { return vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker; }
		virtual void RunFrame(SOculusData* oculusData) override;
#pragma endregion

		void UseOVRControllerData(bool useOVRControllerData);

	private:
		bool useOVRControllerData = false;
		unsigned int ovrTrackerOffset;
    };
}
