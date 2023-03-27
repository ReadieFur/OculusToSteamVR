//Program.cpp: This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>

#include "OculusToSteamVRClient.h"

int main(int argc, char** argsv)
{
	OculusToSteamVRClient::Main(argc, argsv);

#ifdef _DEBUG
	std::cout << "DEBUG: Press any key to exit..." << std::endl;
	std::cin.get();
#endif

	return 0;
}
