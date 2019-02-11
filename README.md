MultiChain
==========

[MultiChain](http://www.multichain.com/) is an open source platform for private blockchains, which offers a rich set of features including extensive configurability, rapid deployment, permissions management, native assets and data streams. Although it is designed to enable private blockchains, MultiChain provides maximal compatibility with the bitcoin ecosystem, including the peer-to-peer protocol, transaction/block formats and [Bitcoin Core](https://bitcoin.org/en/bitcoin-core/) APIs/runtime parameters.

    Copyright (c) 2014-2019 Coin Sciences Ltd
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
    sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config libssl-dev git python python-pip
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get install -y libdb4.8-dev libdb4.8++-dev
    sudo pip install pathlib2

MultiChain requires Boost version no later than 1.65.

    sudo apt-get install -y libboost1.65-all-dev

Clone MultiChain
----------------

    git clone https://github.com/MultiChain/multichain.git

Prepare to download or build V8
-------------------

    cd multichain
    set MULTICHAIN_HOME=$(pwd)
    mkdir v8build
    cd v8build
    
You can use pre-built headers and binaries of Google's V8 JavaScript engine by downloading and expanding [linux-v8.tar.gz](https://github.com/MultiChain/multichain-binaries/raw/master/linux-v8.tar.gz) in the current directory. If, on the other hand, you prefer to build the V8 component yourself, please follow the instructions in [V8.md](/V8.md/).

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

Please see the instructions in [mac.md](/mac.md/) to build MultiChain for use with MacOS.
