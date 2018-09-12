
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
        fprintf(stderr, "Usage. rx <dev>\n\r");
        return (0);
    }

    // Get more attention...
    setpriority(PRIO_PROCESS, 0, -10);

    std::string deviceName(argv[1]);
    WifiDirect rec(deviceName);

    rec.OpenForReceive(0x77);

    // Wait for things to settle...
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Device opened." << std::endl;

    while (true)
    {
        std::vector<uint8_t> data;

        fd_set readset;
        struct timeval to;

        to.tv_sec = 0;
        to.tv_usec = 1e5;

        FD_ZERO(&readset);
        FD_SET(rec.GetFileDescriptor(), &readset);

        int n = select(30, &readset, NULL, NULL, &to);

        if (FD_ISSET(rec.GetFileDescriptor(), &readset))
        {
            rec.Receive(data);
            for (int i = 0; i < data.size(); i++)
            {
                // Easier with printf...
                printf("%.2X ", data[i]);
            }
            std::cout << std::endl;
        }
    }

    std::cout << "...tick..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return (0);
}
