# Mac Build Notes (macOS 10.12+)

## Install XCode and XCode command line tools

-   Test if XCode command line tools are installed:
        
        xcode-select -p
        
-   If the command does not print the location of the XCode command-line tools successfully:

        xcode-select --install
        
-   Select XCode location:

        sudo xcode-select -s <path/to/Xcode.app>

-   Install brew (follow instructions on [brew.sh](https://brew.sh/))

## Clone MultiChain
Install git from git-scm, then

    git clone https://github.com/MultiChain/multichain.git


## Prepare to download or build V8

    cd multichain
    set MULTICHAIN_HOME=$(pwd)
    mkdir v8build
    cd v8build

    
You can use the pre-built headers and binaries of Google's V8 JavaScript engine by downloading and expanding [macos-v8.tar.gz](https://github.com/MultiChain/multichain-binaries/raw/master/macos-v8.tar.gz) in the current directory. If, on the other hand, you prefer to build the V8 component yourself, please follow the instructions in [V8_mac.md](/V8_mac.md/).

## Install dependencies

    brew install autoconf automake libevent libtool boost pkg-config rename nasm

## Compile MultiChain for Mac (64-bit)

    cd $MULTICHAIN_HOME
    ./autogen.sh
    ./configure --with-gui=no --with-libs=no --with-miniupnpc=no
    make

## Notes

* This will build `multichaind`, `multichain-cli` and `multichain-util` in the `src` directory.
