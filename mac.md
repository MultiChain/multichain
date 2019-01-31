# Mac Build Notes (on MacOS Sierra)

## Install dependencies

Install XCode and XCode command line tools

Install git from git-scm

Install brew (follow instructions on brew.sh)

    brew install autoconf automake berkeley-db4 libtool boost@1.57 openssl pkg-config rename python@2
    brew link boost@1.57 --force

If another Boost version was already installed, then do this:

    brew uninstall boost
    brew install boost@1.57
    brew link boost@1.57 --force
	
The following command may be necessary if XCode was not used yet:

    sudo xcode-select -s /Applications/Xcode.app

## Prepare for static linking

Apple does not support statically linked binaries as [documented here](https://developer.apple.com/library/content/qa/qa1118/_index.html), however, it is convenient for end-users to launch a binary without having to first install brew, a third-party system designed for developers.

To create a statically linked MultiChain which only depends on default MacOS dylibs, the following steps are taken:

1. Hide the brew boost dylibs from the build system:

        rename -e 's/.dylib/.dylib.hidden/' /usr/local/opt/boost\@1.57/lib/*.dylib

2. Hide the brew berekley-db dylibs from the build system:

        rename -e 's/.dylib/.dylib.hidden/' /usr/local/opt/berkeley-db\@4/lib/*.dylib

3. Hide the brew openssl dylibs from the build system:

        rename -e 's/.dylib/.dylib.hidden/' /usr/local/opt/openssl/lib/*.dylib

The default brew cookbook for berkeley-db and boost builds static libraries, but the default cookbook for openssl only builds dylibs.

3. Tell brew to build openssl static libraries:

        brew edit openssl
        
    In 'def configure_args' change 'shared' to 'no-shared'
    
        brew install openssl --force
        export LDFLAGS=-L/usr/local/opt/openssl/lib
        export CPPFLAGS=-I/usr/local/opt/openssl/include

## Clone MultiChain

    git clone https://github.com/MultiChain/multichain.git


## Prepare to build V8

    cd multichain
    set MULTICHAIN_HOME=$(pwd)
    mkdir v8build
    cd v8build

## Build Google's V8 JavaScript engine locally

Please use the instructions in [V8_mac.md](/V8_mac.md/) to build and install V8 for use by MultiChain.


## Compile MultiChain for Mac (64-bit)

    cd $MULTICHAIN_HOME
    ./autogen.sh
    ./configure --with-gui=no --with-libs=no --with-miniupnpc=no
    make

## Clean up

    rename -e 's/.dylib.hidden/.dylib/' /usr/local/opt/berkeley-db\@4/lib/*.dylib.hidden
    rename -e 's/.dylib.hidden/.dylib/' /usr/local/opt/boost/lib/*.dylib.hidden
    rename -e 's/.dylib.hidden/.dylib/' /usr/local/opt/openssl/lib/*.dylib.hidden
    brew edit openssl
    
In 'def configure_args' change 'no-shared' to 'shared'

## Notes

* This will build `multichaind`, `multichain-cli` and `multichain-util` in the `src` directory.
