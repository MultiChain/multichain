Building MultiChain
=====================

System requirements
--------------------

These instructions have been tested on Ubuntu 14.04 x64 only.

C++ compilers are memory-hungry. It is recommended to have at least 1 GB of memory available when compiling MultiChain. With 512MB of memory or less compilation will take much longer due to swap thrashing.


Linux Build Notes (currently Ubuntu only)
=====================

Install dependencies
----------------------------------------------

    sudo apt-get update
    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
    sudo apt-get install libboost-all-dev
    sudo apt-get install git
    sudo apt-get install software-properties-common
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install libdb4.8-dev libdb4.8++-dev

Compile MultiChain for Ubuntu (64-bit)
---------------------

    ./autogen.sh
    ./configure
    make

Notes
-----

This will build `multichaind`, `multichain-cli` and `multitchain-util` in the `src` directory.

The release is built with GCC and then `strip multichaind` to strip the debug symbols, which reduces the executable size by about 90%.


Windows Build Notes (cross compilation on Ubuntu)
=====================

Install dependencies
----------------------------------------------

    sudo apt-get update
    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
    sudo apt-get install g++-mingw-w64-i686 mingw-w64-i686-dev g++-mingw-w64-x86-64 mingw-w64-x86-64-dev curl
    sudo apt-get install libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev
    sudo apt-get install git
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install libdb4.8-dev libdb4.8++-dev

Compile MultiChain for Windows (64-bit)
---------------------

    ./autogen.sh
    cd depends
    make HOST=x86_64-w64-mingw32 -j4
    cd ..
    ./configure --prefix=`pwd`/depends/x86_64-w64-mingw32 --enable-cxx --disable-shared --enable-static --with-pic
    make

Notes
-----

This will build `multichaind.exe`, `multichain-cli.exe` and `multitchain-util.exe` in the `src` directory.

