#pragma once

#include <openvr_driver.h>
#include <memory>
#include <chrono>
#include <vector>
#include "DataReceiver.h"
#include "SOculusData.h"
#include "IVRDevice.h"

namespace OculusToSteamVR_Driver
{
	class Driver : public vr::IServerTrackedDeviceProvider
	{
	public:
#pragma region IServerTrackedDeviceProvider
        virtual vr::EVRInitError Init(vr::IVRDriverContext* pDriverContext) override;
        virtual void Cleanup() override;
        virtual inline const char* const* GetInterfaceVersions() override { return vr::k_InterfaceVersions; };
        virtual void RunFrame() override;
        virtual bool ShouldBlockStandbyMode() override { return false; }
        virtual void EnterStandby() override {}
        virtual void LeaveStandby() override {}
#pragma endregion

        bool AddDevice(std::shared_ptr<IVRDevice> device);

    private:
        std::vector<std::shared_ptr<IVRDevice>> devices;
        DataReceiver* dataReceiver = nullptr;
        SOculusData lastOculusData;
	};
}
