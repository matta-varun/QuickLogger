# QuickLogger
A fast reliable Logging Library for C++17 utilizing lock free data-structures and multi-threading. The library uses fmt-style string formatting and can reliably handle upto 80 million text logs at any point in time.

The library used a fast unbounded lock-free MPMC Queue proposed by Pedro Ramalhete (FAAArrayQueue). An implementation of the same was borrowed from Xenium which provides a collection of concurrent data structures and reclamation schemes. [Link to Xenium](https://github.com/mpoeter/xenium)

*Still under development*

# Benchmarks
The current version of the library achieved lowest latency of 180 nanoseconds per log when logging static strings and 330 nanoseconds per log when logging strings to be formatted with integers, floats and strings. The tests were done on my laptop which has "Intel(R) Core(TM) i7-8565U CPU @ 1.80GHz" CPU.

# Installation
To use QuickLogger, simply include the header file in your code and start using it! (You might want to reconfigure include paths in some header files of xenium folder for it to get working in your device, this will be fixed soon)

Detailed Documentation coming soon :)
