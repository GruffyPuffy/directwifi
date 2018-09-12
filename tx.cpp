// C legacy includes
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include <pcap.h>
#include <endian.h>
#include <fcntl.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

// C++ includes
#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <string>
#include <atomic>
#include <mutex>

#include "AsyncStreamReader.h"

class Injector
{
  private:
	// Locals
	char szErrbuf[PCAP_ERRBUF_SIZE];
	pcap_t *pcapDev = 0;

	// Keep some data for a loooong packet
	uint8_t packet[4096];
	int headerLength;
	int packageLength;

	uint32_t seqNum = 1;

  public:
	Injector(std::string device)
	{
		// Open pcap device, i.e. our wifi device
		// 100 = snaplen specifies the snapshot length to be set on the handle.
		// 0 = promisc specifies if the interface is to be put into promiscuous mode.
		// 20 = to_ms specifies the read timeout in milliseconds.
		pcapDev = pcap_open_live(device.c_str(), 100, 0, 20, szErrbuf);
		if (pcapDev == 0)
		{
			throw "Could not open pcad device!";
		}
		// No blocking device
		pcap_setnonblock(pcapDev, 0, szErrbuf);
	}

	~Injector()
	{
		// Bye bye
		pcap_close(pcapDev);
	}

	int Transmit(uint8_t *data, int dataLength)
	{
		// RADIOTAP
		static uint8_t radiotapHeader[] = {
			0x00, 0x00,				// <-- radiotap version
			0x0c, 0x00,				// <- radiotap header length
			0x04, 0x80, 0x00, 0x00, // <-- radiotap present flags
			0x30,					// datarate = 24MBit
			0x00, 0x00, 0x00};

		// IEEE801.11
		static uint8_t ieeeHeader[] = {
			0x08,
			0xbf,
			0x00,
			0x00, // frame control field (2 bytes), duration (2 bytes)
			0xff,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00, // mac
			0x13,
			0x22,
			0x33,
			0x44,
			0x55,
			0x66, // mac
			0x13,
			0x22,
			0x33,
			0x44,
			0x55,
			0x66, // mac
			0x00,
			0x00, // IEEE802.11 seqnum
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
		};
		//
		// HEADER
		//

		// Setup the radiotap header, i.e. copy into place
		uint8_t *pPacket = &packet[0];
		memcpy(packet, radiotapHeader, sizeof(radiotapHeader));
		pPacket += sizeof(radiotapHeader);

		// Add the 802.11 header, i.e. copy into place
		memcpy(pPacket, ieeeHeader, sizeof(ieeeHeader));
		pPacket += sizeof(ieeeHeader);

		headerLength = pPacket - packet;
		std::cout << "HEADER length: " << headerLength << std::endl;

		//
		// DATA
		//
		memcpy(pPacket, data, dataLength);
		pPacket += dataLength;
		packageLength = pPacket - packet;
		std::cout << "PACKAGE length: " << packageLength << std::endl;

		for (int i = 0; i < packageLength; i++)
		{
			// Easier with printf...
			printf("%.2X ", packet[i]);
		}
		std::cout << std::endl;

		//
		// INJECT
		//
		int numSent = pcap_inject(pcapDev, packet, packageLength);
		if (numSent != packageLength)
		{
			pcap_perror(pcapDev, "Failed to inject!");
			throw "Failed to inject!";
		}

		return packageLength;
	}

	int Transmit(std::string data)
	{
		std::vector<uint8_t> vec(data.begin(), data.end());
		return Injector::Transmit(vec.data(), vec.size());
	}

	int Transmit(std::vector<char> data)
	{
		std::vector<uint8_t> vec(data.begin(), data.end());
		return Injector::Transmit(vec.data(), vec.size());
	}

	int Transmit(std::vector<uint8_t> data)
	{
		return Injector::Transmit(data.data(), data.size());
	}
};

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
	Injector inj(deviceName);

	// Wait for things to settle...
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::cout << "Device opened." << std::endl;

	AsyncStreamReader reader{&std::cin, 1024};

	while (true)
	{

		// Ask the AsyncStreamReader reader if it has input ready for us. This is a
		// thread-safe function. In fact, all of AsyncStreamReader is thread safe.
		while (reader.isReady())
		{
			std::vector<char> data = reader.getLine();
			//std::string data = reader.getLine();
			//std::cout << "Data in: " << data << std::endl;
			inj.Transmit(data);
		}

		// Check if the reader is running. If it's running, we'll need to continue
		// to check for input until it no longer runs. It will "run" until it has
		// no more input for us and can't get anymore.
		if (reader.stillRunning())
		{
			std::cout << "...still waiting..." << std::endl;
		}
		else
		{
			// eof = end of file. If you were to pipe input into the program via
			// unix pipes (cat filename | ./nonblocking), std::cin would receive
			// input via the pipe. When the pipe no longer has any input, it sends
			// the eof character. AsynStreamReader will "run" until all of the
			// input is consumed and it hits eof. If you run it without pipes,
			// (such as via the normal console), you'll never reach eof in
			// std::cin unless you send it directly via control-D.
			std::cout << "...eof..." << std::endl;
		}

		// Tell the thread to sleep for a little bit.
		//std::this_thread::sleep_for(std::chrono::seconds(1));
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	return (0);
}
