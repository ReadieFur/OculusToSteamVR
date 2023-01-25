#pragma once

#include <openvr_driver.h>
#include "SOculusData.h"

#define HMD_OFFSET 0
#define CONTROLLERS_OFFSET HMD_OFFSET + 1 //+1 because we can have 1 HMD.
#define SENSORS_OFFSET CONTROLLERS_OFFSET + 2 //+2 becuase we can have 2 controllers.
#define OBJECTS_OFFSET SENSORS_OFFSET + MAX_OVR_SENSORS

namespace OculusToSteamVR_Driver
{
	class SharedVRProperties
	{
	public:
		static void SetCommonProperties(vr::PropertyContainerHandle_t properties, std::string modelNumber, std::string serialNumber);
	};
}
