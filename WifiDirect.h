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

#pragma once

class WifiDirect
{
  private:
    // Locals
    char szErrbuf[PCAP_ERRBUF_SIZE];
    pcap_t *pcapDev = 0;
    int selectableFd = -1;

    // Keep some data for a loooong packet
    uint8_t packet[4096];

    uint32_t seqNum = 1;

    struct bpf_program bpfProgram;
    char program[512];

    const int ieee80211DataHeaderLength = 30;

  public:
    WifiDirect(std::string device)
    {
        // Open pcap device, i.e. our wifi device
        // 1600 = snaplen specifies the snapshot length to be set on the handle.
        // 0 = promisc specifies if the interface is to be put into promiscuous mode.
        // 20 = to_ms specifies the read timeout in milliseconds.
        pcapDev = pcap_open_live(device.c_str(), 1600, 0, 20, szErrbuf);
        if (pcapDev == 0)
        {
            throw "Could not open pcad device!";
        }

        // Check if wifi
        if (pcap_datalink(pcapDev) != DLT_IEEE802_11_RADIO)
        {
            throw "Not Wifi???? Failed...";
        }

        // Default
        SetBlocking(true);
    }

    ~WifiDirect()
    {
        // Bye bye
        pcap_close(pcapDev);
    }

    void SetBlocking(bool on)
    {
        if (on)
        {
            // Blocking device
            if (pcap_setnonblock(pcapDev, 1, szErrbuf) < 0)
            {
                throw "Could not set blocking mode.";
            }
        }
        else
        {
            // No blocking device
            if (pcap_setnonblock(pcapDev, 0, szErrbuf) < 0)
            {
                throw "Could not set non-blocking mode.";
            }
        }
    }

    void OpenForReceive(uint8_t port)
    {
        // Setup a filter that will capture DATA frames.
        // match on frametype, 1st byte of mac (ff)
        sprintf(program, "ether[0x00:2] == 0x08bf && ether[0x04:2] == 0xff%.2x", port);

        if (pcap_compile(pcapDev, &bpfProgram, program, 1, 0) == -1)
        {
            puts(program);
            puts(pcap_geterr(pcapDev));
            throw "Failed to compile pcap program...";
        }
        else
        {
            if (pcap_setfilter(pcapDev, &bpfProgram) == -1)
            {
                fprintf(stderr, "%s\n", program);
                fprintf(stderr, "%s\n", pcap_geterr(pcapDev));
                throw "Failed to set pcap filter...";
            }
            pcap_freecode(&bpfProgram);
        }

        selectableFd = pcap_get_selectable_fd(pcapDev);
    }

    int Transmit(uint8_t *data, int dataLength, uint8_t port)
    {
        // RADIOTAP
        static uint8_t radiotapHeader[] = {
            0x00, 0x00,             // <-- radiotap version
            0x0c, 0x00,             // <- radiotap header length
            0x04, 0x80, 0x00, 0x00, // <-- radiotap present flags
            0x30,                   // datarate = 24MBit
            0x00, 0x00, 0x00};

        // IEEE801.11
        static uint8_t ieeeHeader[] = {
            0x08,
            0xbf,
            0x00,
            0x00, // frame control field (2 bytes), duration (2 bytes)
            0xff,
            0x00, // <- Here we place the "port" number (inspired by wifibroadcast)
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

        int headerLength;
        int packageLength;

        //
        // HEADER
        //

        // Setup the radiotap header, i.e. copy into place
        uint8_t *pPacket = &packet[0];
        memcpy(packet, radiotapHeader, sizeof(radiotapHeader));
        pPacket += sizeof(radiotapHeader);

        // Add the 802.11 header, i.e. copy into place
        // select port
        ieeeHeader[5] = port;
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

    int Transmit(std::string data, uint8_t port)
    {
        std::vector<uint8_t> vec(data.begin(), data.end());
        return WifiDirect::Transmit(vec.data(), vec.size(), port);
    }

    int Transmit(std::vector<char> data, uint8_t port)
    {
        std::vector<uint8_t> vec(data.begin(), data.end());
        return WifiDirect::Transmit(vec.data(), vec.size(), port);
    }

    int Transmit(std::vector<uint8_t> data, uint8_t port)
    {
        return WifiDirect::Transmit(data.data(), data.size(), port);
    }

    std::vector<uint8_t> Receive()
    {
        int retval;
        uint8_t *bufferPtr = 0;
        uint8_t *bufferPtrBegin = 0;
        struct pcap_pkthdr *headerPtr = NULL;

        retval = pcap_next_ex(pcapDev, &headerPtr, (const u_char **)&bufferPtr);

        if (retval != 1)
        {
            puts(pcap_geterr(pcapDev));
            throw "Wifi device failed...";
        }

        // Get the radiotap header length
        uint16_t radioTapLength = ((uint8_t)bufferPtr[2] + ((uint8_t)bufferPtr[3] << 8));
        //std::cout << "RadioTapLength = " << radioTapLength << std::endl;

        // Skip to "payload"
        uint16_t startOfPayload = radioTapLength + ieee80211DataHeaderLength;
        bufferPtr += startOfPayload; 
        uint16_t payloadLength = headerPtr->len - startOfPayload - 14;

        //std::cout << "PACKAGE length: " << headerPtr->len << std::endl;
        bufferPtrBegin = bufferPtr;
        /*
        for (int i = 0; i < payloadLength; i++)
        {
            // Easier with printf...
            uint8_t byte = *bufferPtr;
            bufferPtr++;
            printf("%.2X ", byte);
        }
        std::cout << std::endl;
        */
        std::vector<uint8_t> data;
        std::copy(bufferPtrBegin, bufferPtrBegin + payloadLength, std::back_inserter(data));
        return data;
    }

    int GetFileDescriptor()
    {
        return selectableFd;
    }
};
