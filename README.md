MultiChain
==========

[MultiChain](http://www.multichain.com/) is an open source platform for private blockchains, which offers a rich set of features including extensive configurability, rapid deployment, permissions management, native assets and data streams. Although it is designed to enable private blockchains, MultiChain provides maximal compatibility with the bitcoin ecosystem, including the peer-to-peer protocol, transaction/block formats and [Bitcoin Core](https://bitcoin.org/en/bitcoin-core/) APIs/runtime parameters.

MultiChain is licensed under the GPLv3 open source license. We also offer commercial licenses for those wishing to integrate MultiChain into a proprietary software solution.

    Copyright (c) 2014-2017 Coin Sciences Ltd
    License: GNU General Public License version 3, see COPYING

    Portions copyright (c) 2009-2016 The Bitcoin Core developers
    Portions copyright many others - see individual files

System requirements
-------------------

These compilation instructions have been tested on Ubuntu 14.04 x64 only.

C++ compilers are memory-hungry, so it is recommended to have at least 1 GB of memory available when compiling MultiChain. With less memory, compilation may take much longer due to swapfile thrashing.


Linux Build Notes (on Ubuntu 14.04 x64)
=================

Install dependencies
--------------------

    sudo apt-get update
    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
    sudo apt-get install libboost-all-dev
    sudo apt-get install git
    sudo apt-get install software-properties-common
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install libdb4.8-dev libdb4.8++-dev

Compile MultiChain for Ubuntu (64-bit)
-----------------------------

    ./autogen.sh
    ./configure
    make

Notes
-----

* This will build `multichaind`, `multichain-cli` and `multichain-util` in the `src` directory.

* The release is built with GCC after which `strip multichaind` strings the debug symbols, which reduces the executable size by about 90%.


Windows Build Notes (on Ubuntu 14.04 x64)
=====================

Install dependencies
--------------------

    sudo apt-get update
    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils
    sudo apt-get install g++-mingw-w64-i686 mingw-w64-i686-dev g++-mingw-w64-x86-64 mingw-w64-x86-64-dev curl
    sudo apt-get install libboost-system-dev libboost-filesystem-dev libboost-chrono-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev
    sudo apt-get install git
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install libdb4.8-dev libdb4.8++-dev

Compile MultiChain for Windows (64-bit)
------------------------------

    ./autogen.sh
    cd depends
    make HOST=x86_64-w64-mingw32 -j4
    cd ..
    ./configure --prefix=`pwd`/depends/x86_64-w64-mingw32 --enable-cxx --disable-shared --enable-static --with-pic
    make

Notes
-----

* This will build `multichaind.exe`, `multichain-cli.exe` and `multitchain-util.exe` in the `src` directory.


Mac Build Notes (on MacOS Sierra)
================

Install dependencies
--------------------

    Install XCode and XCode command line tools
    Install git from git-scm
    Install brew (follow instructions on brew.sh)
    brew install autoconf automake berkeley-db4 libtool boost openssl pkg-config rename

Prepare for static linking
--------------------------
Apple does not support statically linked binaries as [documented here](https://developer.apple.com/library/content/qa/qa1118/_index.html), however, it is convenient for end-users to launch a binary without having to first install brew, a third-party system designed for developers.

To create a statically linked MultiChain which only depends on default MacOS dylibs, the following steps are taken:

1. Hide the brew boost dylibs from the build system:
    rename -e 's/.dylib/.dylib.hidden/' /usr/local/opt/boost/lib/*.dylib

2. Hide the brew berekley-db dylibs from the build system:
    rename -e 's/.dylib/.dylib.hidden/' /usr/local/opt/berkeley-db\@4/lib/*.dylib

3. Hide the brew openssl dylibs from the build system:
    rename -e 's/.dylib/.dylib.hidden/' /usr/local/opt/openssl/lib/*.dylib

The default brew cookbook for berkeley-db and boost builds static libraries, but the default cookbook for openssl only builds dylibs.

3. Tell brew to build openssl static libraries:
    brew edit openssl
        In 'def configure_args' change 'shared' to 'no-shared'
    brew install openssl --force

Compile MultiChain for Mac (64-bit)
--------------------------

    export LDFLAGS=-L/usr/local/opt/openssl/lib
    export CPPFLAGS=-I/usr/local/opt/openssl/include
    ./autogen.sh
    ./configure --with-gui=no --with-libs=no --with-miniupnpc=no
    make

Clean up
--------

    rename -e 's/.dylib.hidden/.dylib/' /usr/local/opt/berkeley-db\@4/lib/*.dylib.hidden
    rename -e 's/.dylib.hidden/.dylib/' /usr/local/opt/boost/lib/*.dylib.hidden
    rename -e 's/.dylib.hidden/.dylib/' /usr/local/opt/openssl/lib/*.dylib.hidden
    brew edit openssl
        In 'def configure_args' change 'no-shared' to 'shared'

Notes
-----

* This will build `multichaind`, `multichain-cli` and `multichain-util` in the `src` directory.

