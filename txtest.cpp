
// C++ includes
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <string>
#include <atomic>
#include <mutex>

#include "WifiDirect.h"

int main(int argc, char *argv[])
{

	if (argc != 2)
	{
		fprintf(stderr, "Usage. txtest <dev>\n\r");
		return (0);
	}

	// Get more attention...
	setpriority(PRIO_PROCESS, 0, -10);

	std::string deviceName(argv[1]);
	WifiDirect inj(deviceName);

	inj.SetBlocking(true);

	// Wait for things to settle...
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::cout << "Device opened." << std::endl;

	int sequenceNo = 0;

	try
	{
		while (true)
		{
			std::string data = "Hello from directwifi - seqNo = " + std::to_string(sequenceNo) + "\r\n";
			std::cout << "Txing: " << data << std::endl;
			inj.Transmit(data, 0x77);

			// Wait for things to settle...
			//std::this_thread::sleep_for(std::chrono::seconds(1));
			std::this_thread::sleep_for(std::chrono::milliseconds(300));

			sequenceNo++;
		}
	}
	catch (WifiDirectException &caught)
	{
		std::cout << "PROBLEM: " << caught.what() << std::endl;
	}

	return (0);
}
