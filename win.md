# Windows Build Notes (on Windows and Ubuntu 16.04, x64)

Building MultiChain for Windows requires working with two separate development environments:

-   Visual Studio 2017 on Native Windows (for building Google's V8 JavaScript engine)
-   GCC MinGW cross compiler on Linux (for building the MultiChain project)

In different stages of the build, build artifacts from one environment need to be available to the other environment. If possible, sharing a single physical file system is most helpful. Otherwise, files need to be copied between the two file systems (e.g. *scp* on Linux, *pscp* from Putty or WinSCP on Windows).

The ideal system combination is WSL (Windows Subsystem for Linux) on Windows 10 (Creator edition or later).

In the reminder of these instructions we assume that the the following variables have the associated meaning:


| Variable         | Meaning                                                |
| ---------------- | -------------------------------------------------------|
| `MULTICHAIN_HOME`| Root folder of MultiChain                              |
| `LINUX_FS`       | Location in Linux where Windows files can be copied to |

Variables on Windows are referenced by `%VAR%`, and on Linux by `$VAR` or `${VAR}`.

## Prerequisites on Windows

-   [Visual Studio 2017](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15) (community edition acceptable)
-   [Python 2.7](https://www.python.org/ftp/python/2.7.15/python-2.7.15.amd64.msi)
-   [Git](https://github.com/git-for-windows/git/releases/download/v2.19.1.windows.1/Git-2.19.1-64-bit.exe)
-   [CMake](https://github.com/Kitware/CMake/releases/download/v3.13.1/cmake-3.13.1-win64-x64.msi)
-   [Boost](https://sourceforge.net/projects/boost/files/boost-binaries/1.65.0/boost_1_65_0-msvc-11.0-64.exe/download), with binaries for release,multithreading,static (suffix is -mt-s)

## Prerequisites on Linux

        sudo apt update
        sudo apt install -y libtool autotools-dev automake pkg-config git python
        sudo apt install -y g++-mingw-w64-x86-64 mingw-w64-x86-64-dev

## Build Instructions

**Note**: *If sources on the Windows system are accesible from Linux, you can ignore the stage "Prepare a build area on the Linux machine"*.

### Prepare a build area on the *Linux* machine

On Linux:

-   In the location where you want to build MultiChain:

        git clone https://github.com/MultiChain/multichain.git
        cd multichain
        export MULTICHAIN_HOME=$(pwd)

### Prepare a build area on the *Windows* machine

On Windows:

-   In the location where you want to build MultiChain:

        git clone https://github.com/MultiChain/multichain.git
        cd multichain
        set MULTICHAIN_HOME=%CD%

### Build the Google V8 JavaScript engine

On Windows:

        cd %MULTICHAIN_HOME%
        mkdir v8build
        cd v8build

-   Follow the instructions in [v8_win.md](V8_win.md) to fetch, configure and build Google's V8 JavaScript engine.

-   To facilitate building an additional library required by MultiChain, copy the following files to the Linux machine, in the same rlative folder:

        %MULTICHAIN_HOME%\v8build\v8\out.gn\x64.release\*.bin -> $MULTICHAIN_HOME/v8build/v8/out.gn/x64.release
        %MULTICHAIN_HOME%\v8build\v8\out.gn\x64.release\*.dat -> $MULTICHAIN_HOME/v8build/v8/out.gn/x64.release

On Linux:

    sudo easy_install pip
    pip install pathlib2
    cd $RELEASE
    python $MULTICHAIN_HOME/depends/v8_data_lib.py -o win32

<!--
        cd $MULTICHAIN_HOME/v8build/v8/out.gn/x64.release
        objs=()
        for f in *.bin *.dat; do
            objcopy -B i386 -I binary -O elf64-x86-64 $f ${f%.*}.obj
            objs+=("${f%.*}.obj")
        done
        x86_64-w64-mingw32-ar rvs v8_data.lib ${objs[@]}
-->        

-   Copy `v8_data.lib` to `%MULTICHAIN_HOME%\v8build\v8\out.gn\x64.release\obj` on Windows.

On Windows:

            cd %MULTICHAIN_HOME%\src\v8_win
            mkdir build
            cd build
            cmake -G "Visual Studio 15 2017 Win64" ..
            cmake --build . --config Release --target spdlog
            cmake --build . --config Release

-   Copy `%MULTICHAIN_HOME%\src\v8_win\build\Release\multichain_v8.lib` to `$MULTICHAIN_HOME/src/v8_win/build/Release` on Linux.
-   Copy `%MULTICHAIN_HOME%\src\v8_win\build\Release\multichain_v8.dll` to `%MULTICHAIN_HOME%\src` (on local Windows).

### Build MultiChain and test that it is alive

On Linux:

        cd $MULTICHAIN_HOME
        ./autogen.sh
        cd depends
        make HOST=x86_64-w64-mingw32
        cd ..
        ./configure --prefix=$(pwd)/depends/x86_64-w64-mingw32 --enable-cxx --disable-shared --enable-static --with-pic
        make

-   This will build `multichaind.exe`, `multichain-cli.exe` and `multitchain-util.exe` in the `src` directory[<sup>1</sup>](#f1).
-   Copy `src/multichaind.exe`, `src/multichain-cli.exe` and `src/multitchain-util.exe` to `%MULTICHAIN_HOME%\src` on Windows.

On Windows:

        cd %MULTICHAIN_HOME%
        src\multichaind.exe --help

-   If all went well, you should see the expected help text from the program.

<a class="anchor" id="f1"></a>1. If you have more than one CPU on your machine, you can speed things up using the `-j #` flag on the `make` command.
