#pragma once

#include "IVRDevice.h"
#include <openvr_driver.h>
#include <string>

namespace OculusToSteamVR_Driver
{
	class VRTracker : public IVRDevice
	{
	public:
		VRTracker(unsigned int objectIndex);

#pragma region IVRDevice
        virtual vr::EVRInitError Activate(uint32_t objectId) override;
		virtual vr::ETrackedDeviceClass GetDeviceType() override { return vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker; }
		virtual std::string GetSerial() override { return serialNumber; }
#pragma endregion

	private:
		std::string serialNumber;
		std::string modelNumber;
    };
}
