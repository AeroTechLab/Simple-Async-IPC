# Simple Async IPC

Basic multi-platform implementation for [IPC Interface](https://github.com/AeroTechLab/IPC-Interface), using different data transports:

- [TCP](https://pt.wikipedia.org/wiki/Transmission_Control_Protocol) [Network Sockets](https://en.wikipedia.org/wiki/Network_socket) for local and remote request-reply 
- [UDP](https://pt.wikipedia.org/wiki/User_Datagram_Protocol) [Network Sockets](https://en.wikipedia.org/wiki/Network_socket) for local and remote client-server and publisher-subscriber
- [Shared Memory](https://en.wikipedia.org/wiki/Shared_memory) for only local communication

## Usage

On a terminal, get the [GitHub code repository](https://github.com/AeroTechLab/Simple-Async-IPC) with:

    $ git clone https://github.com/AeroTechLab/Simple-Async-IPC [<my_project_folder>]

This implementation depends on [IPC Interface](https://github.com/AeroTechLab/IPC-Interface) project, which is added as [git submodules](https://git-scm.com/docs/git-submodule).

To add those repositories to your sources, navigate to the root project folder and clone them with:

    $ cd <my_project_folder>
    $ git submodule update --init

With dependencies set, you can now build the library to a separate build directory with [CMake](https://cmake.org/):

    $ mkdir build && cd build
    $ cmake [-DIP_NETWORK_LEGACY=true] .. 
    $ make

For building it manually e.g. with [GCC](https://gcc.gnu.org/) in a system without **CMake** available, the following shell command (from project directory) would be required:

    $ gcc ipc.c ipc_base_ip.c ipc_base_shm.c -I. -Iinterface -shared -fPIC -o libasyncipc.{so,dll}
