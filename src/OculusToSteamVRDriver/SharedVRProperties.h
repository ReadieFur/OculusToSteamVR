#pragma once

#include <openvr_driver.h>
#include "SOculusData.h"

#define SENSORS_OFFSET 3
#define OBJECTS_OFFSET SENSORS_OFFSET + MAX_OVR_SENSORS

namespace OculusToSteamVR_Driver
{
	class SharedVRProperties
	{
	public:
		static void SetCommonProperties(vr::PropertyContainerHandle_t properties, std::string modelNumber, std::string serialNumber);
	};
}
