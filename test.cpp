#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <string>
#include <atomic>
#include <mutex>

#include "AsyncStreamReader.h"


int main() {

    // Instantiate the class and initialize using "universal initialization syntax"
    // See http://herbsutter.com/2013/05/09/gotw-1-solution/ for some serious
    // details.
    //
    // We're providing the memory address (by adding & in front of the variable) of
    // the global variable std::cin. AsyncStreamReader takes a std::istream* as an
    // argument to the constructor and std::cin is exactly that: an std::istream.
    //
    // Conveniently, because we allow an arbitrary stream to be passed in, you could
    // also use an std::fstream or std::stringstream as well.
    //
    // See http://en.cppreference.com/w/cpp/io/basic_fstream
    // and http://en.cppreference.com/w/cpp/io/basic_stringstream
    //
    // Note that both of those derive from std::istream.
    AsyncStreamReader reader{&std::cin};

    while (true) {

        // Ask the AsyncStreamReader reader if it has input ready for us. This is a
        // thread-safe function. In fact, all of AsyncStreamReader is thread safe.
        while (reader.isReady()) {
            // If there's input, go ahead and grab it and then print it out.
            std::cout << "input received: " << reader.getLine() << std::endl;
        }

        // Check if the reader is running. If it's running, we'll need to continue
        // to check for input until it no longer runs. It will "run" until it has
        // no more input for us and can't get anymore.
        if (reader.stillRunning()) {
            std::cout << "still waiting..." << std::endl;
        } else {
            // eof = end of file. If you were to pipe input into the program via
            // unix pipes (cat filename | ./nonblocking), std::cin would receive
            // input via the pipe. When the pipe no longer has any input, it sends
            // the eof character. AsynStreamReader will "run" until all of the
            // input is consumed and it hits eof. If you run it without pipes,
            // (such as via the normal console), you'll never reach eof in
            // std::cin unless you send it directly via control-D.
            std::cout << "eof" << std::endl;
        }

        // Tell the thread to sleep for a little bit.
        //std::this_thread::sleep_for(std::chrono::seconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}