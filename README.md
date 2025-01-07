# nwork
I/O Completion Ports (IOCP) on Windows and epoll on Linux provides a convenient basis for high performance network programming. The basic idea is that you have a bunch of worker threads that wait for network data (or other I/O events) and when something happens, it can be handled immediately. 
So far so good. But what if you want to use the same pool of worker threads for something else as well? It would be great to use the same threads for more general concurrent programming and not just for handling I/O events. Luckily both IOCP and epoll allows this. Both are quite low level in their nature
and will allow you to post basically anything to be picked up by a worker thread. *nwork* is a simple library that wraps this functionality.

## Details
In the ```extra/``` sub-directory you'll find the excellent [concurrentqueue]([https://github.com/cameron314/concurrentqueue/] library, which is used by the Linux version of *nwork*. Epoll is essentially just a fancy semaphore and you need to bring your own queue data structure. IOCP, on the other hand, comes with its
own queue. 

## Usage
First of all you'll need git and cmake to acquire and build the project. Then you can run:

```
git clone https://github.com/demogorgon1/nwork.git
cd nwork
mkdir build
cd nwork
cmake ..
cmake --build .
```

If succesfull, this will build a static library and tests.

Optionally you can include *nwork* directly in your cmake build system using FetchContent:

```
FetchContent_Declare(nwork
  GIT_REPOSITORY https://github.com/demogorgon1/nwork.git
)
FetchContent_MakeAvailable(nwork)
```

Use ```nwork::nwork``` to link your target with *nwork*.

If you're not using CMake, you can just add the ```lib```, ```include```, ```extra``` subdirectories to your project. 
