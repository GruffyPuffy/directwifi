
// C++ includes
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <string>
#include <atomic>
#include <mutex>

#include "AsyncStreamReader.h"
#include "WifiDirect.h"

int main(int argc, char *argv[])
{

	if (argc != 2)
	{
		fprintf(stderr, "Usage. tx <dev>\n\r");
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

	AsyncStreamReader reader{&std::cin, 256};

	try
	{
		while (true)
		{
			while (reader.isReady())
			{
				std::vector<char> data = reader.getLine();
				inj.Transmit(data, 0x77);
				//std::cout << "...tx-ing..." << std::endl;
			}

			if (!reader.stillRunning())
			{
				std::cout << "...eof..." << std::endl;
				throw WifiDirectException("EndOfFile detected...");
			}

			//std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	catch (WifiDirectException &caught)
	{
		std::cout << "PROBLEM: " << caught.what() << std::endl;
	}

	return (0);
}
