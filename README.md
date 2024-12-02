# ProcessWrapper
The build in process wrapper in C++ like System.Diagnostics.Process in C# 

# Introduction
In c++ win32, redirect output, error from child process requires a lot of efforts
(of course we can use heavy lib like boost or CLR to call C# process)
So i create the process wrapper in order to create, calling, redirect output from child process in easy way

# Idea Behind
The approch is using Anonymous Pipes for exchanging data between child process and parent process.

Create 3 pipes, one pipe for stdout, one pipe for stderror and one pipe for stdin

`ReadLineDataOut` for reading data synchronous, passing `std::function` for reading data asynchronous
# How to use
All the examples in main.cpp

# How to run code
Visual studio 2019(v142), x64, C++20, Window SDK 10.0 or above

# Constraints
When write to stdin pipe, then read to stdout pipe immediately after that, there will be no available data in pipe, so need sleep about 40ms(may vary) to make sure data available in pipe. Any better solution will be appreciated
