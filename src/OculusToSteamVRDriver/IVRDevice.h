#pragma once

#include <openvr_driver.h>
#include <string>

namespace OculusToSteamVR_Driver
{
	class IVRDevice : public vr::ITrackedDeviceServerDriver
	{
	public:
#pragma region ITrackedDeviceServerDriver
		virtual vr::EVRInitError Activate(uint32_t objectId) override = 0;
		virtual void Deactivate() override { objectID = vr::k_unTrackedDeviceIndexInvalid; }
		virtual void EnterStandby() override {}
		virtual void* GetComponent(const char* pchComponentNameAndVersion) override { return nullptr; }
		virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override {}
		virtual vr::DriverPose_t GetPose() override { return lastPose; }
#pragma endregion

		virtual vr::ETrackedDeviceClass GetDeviceType() = 0;
		virtual std::string GetSerial() = 0;

	protected:
		vr::TrackedDeviceIndex_t objectID = vr::k_unTrackedDeviceIndexInvalid;
		vr::DriverPose_t lastPose;
	};
}
