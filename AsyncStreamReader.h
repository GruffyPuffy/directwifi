#include <iostream>
#include <chrono>
#include <thread>
#include <queue>
#include <string>
#include <atomic>
#include <mutex>

#pragma once

//
// Code adapted from Victor Robertson - https://gist.github.com/vmrob
// Changed to read "blocks in binary form"
//

class AsyncStreamReader
{
  public:
    // This constructor uses an initializer list for initializing _stream.
    // See http://en.cppreference.com/w/cpp/language/initializer_list
    AsyncStreamReader(std::istream *stream, int bufSize)
        : _stream(stream), _bufSize(bufSize)
    {
        // we can't put this in an initializer list because std::atomic_bool
        // can't be initialized via constructor.
        _eof = false;

        // We probably shouldn't initialize thread in the initializer list
        // because during the run of the initializer list, the class hasn't
        // fully been constructed. We don't want to spin off another thread
        // that uses this object while we're still constructing it!
        //
        // By the time the body of the constructor is executed, we do have a
        // valid object, so we can spin off the thread now.
        //
        // The thread itself is constructed using a lambda function using
        // capture by reference. We have to capture `this` by reference because
        // we're actually executing a member function of the class.
        // See http://en.cppreference.com/w/cpp/language/lambda for info on
        // lambdas.
        //
        // Note that as soon as the thread is constructed, the lambda is run.
        // We'll have another thread [probably] running before this constructor
        // returns.
        _thread = std::thread([&] { threadEntry(); });
    }

    bool isReady()
    {
        // lock_guard is an excellent helper object. It uses the RAII idiom
        // (http://en.cppreference.com/w/cpp/language/raii) in the context of
        // mutexes. Basically, it's a guaranteed way to call _mutex.unlock()
        // before this function returns--even if _queue.empty() or anything else
        // in the function throws.
        std::lock_guard<std::mutex> lk(_mutex);
        return !_queue.empty();
    }

    std::vector<char> getLine()
    {
        std::lock_guard<std::mutex> lk(_mutex);
        if (_queue.empty())
        {
            // {} is essentially an empty initializer list. It's implicitly
            // convertible to std::string and essentially represents an empty
            // string. It's the same as saying `return "";` or
            // `return std::string()`
            return {};
        }
        // auto is magical. The compiler knows what type results from
        // `std::move(_queue.front())` (even if you don't) and will make sure
        // the type of line is correct.
        //
        // Herb Sutter, one of the top dogs in the C++ world basically says
        // "Almost Always Auto". Really good read:
        // http://herbsutter.com/2013/08/12/gotw-94-solution-aaa-style-almost-always-auto/
        //
        // std::move is a bit trickier. You'll kind of have to know what rvalue
        // references are for this to make sense. Essentially, _queue.front()
        // returns a reference to the front of the queue which is a std::string.
        // When you assign it like this, you're actually using std::string's
        // move assignment operator which works on rvalues. What you need to
        // know is that if you excluded the std::move, you would be copying the
        // first item in the queue which is somewhat wasteful. By "moving" it,
        // you avoid a lot of overhead.
        //
        // See http://www.bogotobogo.com/cplusplus/C11/5_C11_Move_Semantics_Rvalue_Reference.php
        // and http://en.cppreference.com/w/cpp/language/value_category
        auto line = std::move(_queue.front());
        _queue.pop();
        return line;
    }

    inline bool stillRunning()
    {
        return !_eof || isReady();
    }

  private:
    // nullptr is excellent. Always use it over NULL or "0" when you can.
    std::istream *_stream = nullptr;

    // The wanted buffer size (i.e. data package size that pops out of the queue).
    int _bufSize = 256;

    // std::atomic_bool is a typedef for std::atomic<bool>. std::atomic is a
    // class that allows you to perform some atomic operations on variables in a
    // thread safe context. For atomic_bool, this essentially amounts to having
    // a variable that can be safely read from and written to without using a
    // mutex.
    std::atomic_bool _eof;

    // mutexes are synchronization primitives that I don't think I could explain
    // very well in the context of a single comment. You should probably read up
    // on concurrency to get a better grasp of these.
    // A quick google search indicates there's a lot of info on the topic:
    // http://www.cplusplusconcurrencyinaction.com/
    std::mutex _mutex;

    // just a regular old fifo queue.
    std::queue<std::vector<char>> _queue;

    // threads! http://en.cppreference.com/w/cpp/thread/thread
    std::thread _thread;

    // The function that we run in a separate thread.
    void threadEntry()
    {
        while (!std::cin.eof())
        {
            std::vector<char> data(_bufSize);
            std::cin.read(&data.front(), _bufSize);

            int numRead = std::cin.gcount();
            data.resize(numRead);
            if (data.size() != 0)
            {
                std::lock_guard<std::mutex> lk(_mutex); // keep _queue thread safe!
                _queue.push(std::move(data));           // Another usage of std::move
            }
        }

        // While technically eof probably isn't the only thing that can happen
        // when this loop exits, we're essentially just saying that we can't
        // read anymore.
        _eof = true;
    }
};
