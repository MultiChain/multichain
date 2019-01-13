MultiChain
==========

[MultiChain](http://www.multichain.com/) is an open source platform for private blockchains, which offers a rich set of features including extensive configurability, rapid deployment, permissions management, native assets and data streams. Although it is designed to enable private blockchains, MultiChain provides maximal compatibility with the bitcoin ecosystem, including the peer-to-peer protocol, transaction/block formats and [Bitcoin Core](https://bitcoin.org/en/bitcoin-core/) APIs/runtime parameters.

    Copyright (c) 2014-2017 Coin Sciences Ltd
    License: GNU General Public License version 3, see COPYING

    Portions copyright (c) 2009-2016 The Bitcoin Core developers
    Portions copyright many others - see individual files

System requirements
-------------------

These compilation instructions have been tested on Ubuntu 16.04 x64 (xenial) and Ubuntu 18.04 x64 (bionic) only.

C++ compilers are memory-hungry, so it is recommended to have at least 1 GB of memory available when compiling MultiChain. With less memory, compilation may take much longer due to swapfile thrashing.


Linux Build Notes (on Ubuntu 16.04 x64 or later)
=================

Install dependencies
--------------------

    sudo apt-get update
    sudo apt-get install -y software-properties-common
    sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config libssl-dev git python
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get install -y libdb4.8-dev libdb4.8++-dev

MultiChain requires Boost version no later than 1.65.

    sudo apt-get install -y libboost1.65-all-dev

Clone MultiChain
----------------

    git clone https://github.com/MultiChain/multichain.git

Prepare to build V8
-------------------

    cd multichain
    set MULTICHAIN_HOME=$(pwd)
    mkdir v8build
    cd v8build

Build Google's V8 JavaScript engine locally
-------------------------

Please use the instructions in [V8.md](/V8.md/) to build and install V8 for use by MultiChain.

Compile MultiChain for Ubuntu (64-bit)
-----------------------------

    cd $MULTICHAIN_HOME
    ./autogen.sh
    ./configure
    make

Notes
-----

* This will build `multichaind`, `multichain-cli` and `multichain-util` in the `src` directory.

* The release is built with GCC after which `strip multichaind` strings the debug symbols, which reduces the executable size by about 90%.


Windows Build Notes
=====================

Please see the instructions in [win.md](/win.md/) to build MultiChain for use with Windows.


Mac Build Notes (on MacOS Sierra)
================

Install dependencies
--------------------

    Install XCode and XCode command line tools
    Install git from git-scm
    Install brew (follow instructions on brew.sh)
    brew install autoconf automake berkeley-db4 libtool boost openssl pkg-config rename

on MacOS High Sierra

    brew uninstall boost
    brew install boost@1.57
    brew link boost@1.57 --force

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
