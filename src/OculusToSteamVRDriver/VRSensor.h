#pragma once

#include "IVRDevice.h"
#include <openvr_driver.h>
#include <string>
#include "SOculusData.h"

namespace OculusToSteamVR_Driver
{
	class VRSensor : public IVRDevice
	{
	public:
#pragma region IVRDevice
		VRSensor(unsigned int ovrObjectIndex);
		virtual vr::EVRInitError Activate(uint32_t objectId) override;
		virtual vr::ETrackedDeviceClass GetDeviceType() override { return vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference; }
		virtual void RunFrame(SOculusData* oculusData) override;
#pragma endregion

	private:
		unsigned int ovrSensorOffset;
	};
}
