// unicornOSC.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "unicorn.h"
// Include unicorn lib.
#pragma comment(lib, "Unicorn.lib")

#define OSCPKT_OSTREAM_OUTPUT
#define PORT 7000
#include "oscpkt.hh"
#include "udp.hh" 

#define FRAME_LENGTH			1				// The number of samples acquired per get data call.
#define TESTSIGNAL_ENABLED		FALSE			// Flag to enable or disable testsignal.

// Function declarations.
//-------------------------------------------------------------------------------------
void HandleError(int errorCode);
void PrintErrorMessage(int errorCode);

// Main. Program entry point.
//-------------------------------------------------------------------------------------
int main()
{
	oscpkt::UdpSocket sock;
	sock.connectTo("localhost", PORT);
	if (!sock.isOk()) {
		std::cerr << "Error connection to port " << PORT << ": " << sock.errorMessage() << "\n";
		return 0;
	}
	oscpkt::PacketWriter pw;
	oscpkt::Message msg;

	std::cout << "Unicorn Streaming to OSC port " << PORT << std::endl;
	std::cout << "Press CTRL+C to quit." << std::endl;
	std::cout << "----------------------------------" << std::endl << std::endl;

	// Variable to store error codes.
	int errorCode = UNICORN_ERROR_SUCCESS;

	// Structure that holds the handle for the current session.
	UNICORN_HANDLE deviceHandle = 0;


	try
	{
		// Get available devices.
		//-------------------------------------------------------------------------------------

		// Get number of available devices.
		unsigned int availableDevicesCount = 0;
		errorCode = UNICORN_GetAvailableDevices(NULL, &availableDevicesCount, TRUE);
		HandleError(errorCode);

		if (availableDevicesCount < 1)
		{
			std::cout << "No device available. Please pair with a Unicorn device first.";
			errorCode = UNICORN_ERROR_GENERAL_ERROR;
			HandleError(errorCode);
		}

		//Get available device serials.
		UNICORN_DEVICE_SERIAL *availableDevices = new UNICORN_DEVICE_SERIAL[availableDevicesCount];
		errorCode = UNICORN_GetAvailableDevices(availableDevices, &availableDevicesCount, TRUE);
		HandleError(errorCode);

		//Print available device serials.
		std::cout << "Available devices:" << std::endl;
		for (unsigned int i = 0; i<availableDevicesCount; i++)
		{
			std::cout << "#" << i << ": " << availableDevices[i] << std::endl;
		}
		
		unsigned int deviceId;
		if (availableDevicesCount > 1) {
			// Request device selection.
			std::cout << "\nSelect device by ID #";
			std::cin >> deviceId;
			if (deviceId >= availableDevicesCount || deviceId < 0)
				errorCode = UNICORN_ERROR_GENERAL_ERROR;

			HandleError(errorCode);
		}
		else {
			// Just one device connected.
			deviceId = 0;
		}


		// Open selected device.
		//-------------------------------------------------------------------------------------
		std::cout << "Trying to connect to '" << availableDevices[deviceId] << "'." << std::endl;
		UNICORN_HANDLE deviceHandle;
		errorCode = UNICORN_OpenDevice(availableDevices[deviceId], &deviceHandle);

		HandleError(errorCode);
		std::cout << "Connected to '" << availableDevices[deviceId] << "'." << std::endl;
		std::cout << "Device Handle: " << deviceHandle << std::endl;

		float* acquisitionBuffer = NULL;

		// Initialize acquisition members.
		//-------------------------------------------------------------------------------------
		unsigned int numberOfChannelsToAcquire = 0;
		errorCode = UNICORN_GetNumberOfAcquiredChannels(deviceHandle, &numberOfChannelsToAcquire);
		HandleError(errorCode);

		UNICORN_AMPLIFIER_CONFIGURATION configuration;
		errorCode = UNICORN_GetConfiguration(deviceHandle, &configuration);
		HandleError(errorCode);

		int samplingRate = UNICORN_SAMPLING_RATE;

		// Print acquisition configuration
		std::cout << std::endl;
		std::cout << "Acquisition Configuration:" << std::endl;
		std::cout << "Sampling Rate: " << samplingRate << "Hz" << std::endl;
		std::cout << "Frame Length: " << FRAME_LENGTH << std::endl;
		std::cout << "Number Of Acquired Channels: " << numberOfChannelsToAcquire << std::endl;

		// Allocate memory for the acquisition buffer.
		int acquisitionBufferLength = numberOfChannelsToAcquire * FRAME_LENGTH;
		acquisitionBuffer = new float[acquisitionBufferLength];

		try
		{
			// Start data acquisition.
			//-------------------------------------------------------------------------------------
			errorCode = UNICORN_StartAcquisition(deviceHandle, TESTSIGNAL_ENABLED);
			HandleError(errorCode);
			std::cout << std::endl << "Data acquisition started." << std::endl;

			// Limit console update rate to max. 25Hz or slower to prevent acquisition timing issues. 
			int consoleUpdateRate = (int)((samplingRate / FRAME_LENGTH) / 25.0f);
			if (consoleUpdateRate == 0)
				consoleUpdateRate = 1;

			unsigned int i = 0;
			// Acquisition loop.
			//-------------------------------------------------------------------------------------
			while(sock.isOk())
			{
				i++;
				// Receives the configured number of samples from the Unicorn device and writes it to the acquisition buffer.
				errorCode = UNICORN_GetData(deviceHandle, FRAME_LENGTH, acquisitionBuffer, acquisitionBufferLength * sizeof(float));
				HandleError(errorCode);
				// The channels aquired are:
				/*	EEG 1|2|3|4|5|6|7|8
					Accelerometer X|Y|Z
					Gyroscope X|Y|Z
					Counter
					Battery Level
					Validation Indicator	*/

				// Write data to console.
				std::cout << acquisitionBuffer[0] << std::endl;// (const char*)acquisitionBuffer;// , acquisitionBufferLength * sizeof(float));
				msg.init("/unicornEEG");
				for (unsigned int i = 0; i < 8; i++) {
					msg.pushFloat(acquisitionBuffer[i]);
				}
				pw.init().addMessage(msg);
				bool ok = sock.sendPacket(pw.packetData(), pw.packetSize());
				if (!ok)
					std::cerr << ok;
				// Update console to indicate that the data acquisition is running.
				if (i%consoleUpdateRate == 0)
					std::cout << ".";
			}

			// Stop data acquisition.
			//-------------------------------------------------------------------------------------
			errorCode = UNICORN_StopAcquisition(deviceHandle);
			HandleError(errorCode);
			std::cout << std::endl << "Data acquisition stopped." << std::endl;
		}
		catch (int errorCode)
		{
			// Write error message to console if something goes wrong.
			PrintErrorMessage(errorCode);
		}
		catch (...)
		{
			// Write error message to console if something goes wrong.
			std::cout << std::endl << "An unknown error occurred." << std::endl;
		}

		// Free memory of the acquisition buffer if necessary.
		if (acquisitionBuffer != NULL)
		{
			delete[] acquisitionBuffer;
			acquisitionBuffer = NULL;
		}

		// Free memory of the device buffer if necessary.
		if (availableDevices != NULL)
		{
			delete[] availableDevices;
			availableDevices = NULL;
		}

		// Close device.
		//-------------------------------------------------------------------------------------
		errorCode = UNICORN_CloseDevice(&deviceHandle);
		HandleError(errorCode);
		std::cout << "Disconnected from Unicorn." << std::endl;
	}
	catch (int errorCode)
	{
		// Write error message to console if something goes wrong.
		PrintErrorMessage(errorCode);
	}
	catch (...)
	{
		// Write error message to console if something goes wrong.
		std::cout << std::endl << "An unknown error occurred." << std::endl;
	}

	std::cout << std::endl << "Press ENTER to terminate the application.";
	std::cin.clear();
	std::cin.ignore();
	getchar();
	return 0;
}

// The method throws an exception and forwards the error code if something goes wrong.
//-------------------------------------------------------------------------------------
void HandleError(int errorCode)
{
	if (errorCode != UNICORN_ERROR_SUCCESS)
	{
		throw errorCode;
	}
}

// The method prints an error messag to the console according to the error code.
//-------------------------------------------------------------------------------------
void PrintErrorMessage(int errorCode)
{
	std::cout << std::endl << "An error occurred. Error Code: " << errorCode << " - ";
	std::cout << UNICORN_GetLastErrorText();
	std::cout << std::endl;
}