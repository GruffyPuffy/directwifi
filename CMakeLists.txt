cmake_minimum_required(VERSION 3.10)

project (directwifi)

set(CMAKE_CXX_STANDARD 11)
find_package (Threads)

### The TX executable (requires libpcap-dev installed)
add_executable(tx tx.cpp)
target_link_libraries(tx pcap ${CMAKE_THREAD_LIBS_INIT})


### The Test (my small tests) executable
#add_executable(mytest test.cpp)
#target_link_libraries(mytest ${CMAKE_THREAD_LIBS_INIT})
