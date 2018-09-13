cmake_minimum_required(VERSION 3.5.1)

project (directwifi)

set(CMAKE_CXX_STANDARD 11)
find_package (Threads)

### The TX executable (requires libpcap-dev installed)
add_executable(tx tx.cpp)
target_link_libraries(tx pcap ${CMAKE_THREAD_LIBS_INIT})


### The RX executable
add_executable(rx rx.cpp)
target_link_libraries(rx pcap ${CMAKE_THREAD_LIBS_INIT})
