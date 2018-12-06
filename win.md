# Windows Build Notes (on Windows and Ubuntu 16.04, x64)

Building MultiChain for Windows requires Working with two separate development environments:

  - Visual Studio 2017 on Native Windows
  - GCC MinGW cross compiler on Linux

In different stages of the build, build artifacts from one environment need to be available to the other environment. If possible, sharing a single physical file system is most helpful. Otherwise, files need to be copied between the two file systems (e.g. via SSH).

## Prerequisites on Windows

  - Visual Studio 2017 (community edition acceptable)
  - Python 2.7
  - Git
  - CMake
  - Boost, with binaries for release,multithreading,static (suffix is -mt-s)

## Prerequisites on Linux

  - libtool autotools-dev automake pkg-config git
  - g++-mingw-w64-x86-64 mingw-w64-x86-64-dev

## Build Instructions

### Prepare a build area on the Linux machine

  - Change directory (`cd`) to the location where you want to build MultiChain.
  - For easier reference, set a variable to this location:

        ROOT_DIR=$(pwd)

  - Clone the MultiChain repository:

        git clone https://github.com/MultiChain/multichain.git

###Prepare a build area on the Windows machine

  - Change directory (`cd`) to the location where you want to build MultiChain.
  - For easier reference, set a variable to this location:

        set ROOT_DIR=%CD%

  - Clone the MultiChain repository:

        git clone https://github.com/MultiChain/multichain.git

### Build the Google V8 JavaScript engine

        cd %ROOT_DIR%\multichain

  - Follow the instructions in [v8_win.md](v8_win.md) to fetch, configure and build Google's V8 JavaScript engine.

  - Build an additional library required by MultiChain:

      - Copy the following files to a location accessible to the Linux machine:

            %ROOT_DIR%\v8build\v8\out.gn\x64.release\*.bin
            %ROOT_DIR%\v8build\v8\out.gn\x64.release\*.dat

      - On the Linux machine, `cd` to the location of these files, and execute the following commands:

            objs=()
            for f in *.bin *.dat; do
                objcopy -B i386 -I binary -O elf64-x86-64 $f ${f%.*}.obj
                objs+=("${f%.*}.obj")
            done
            x86_64-w64-mingw32-ar rvs v8_data.lib ${objs[@]}

      - Copy the library `v8_data.lib` back to the Windows-accessible location at:

            %ROOT_DIR%\v8build\v8\out.gn\x64.release\obj

      - On the Windows machine, build the MultiChain V8 interface DLL:

            cd %ROOT_DIR%\src\v8_win
            mkdir build
            cd build
            cmake -G "Visual Studio 15 2017 Win64" ..
            cmake --build . --config Release

      - Copy the export library `%ROOT_DIR%\src\v8_win\build\Release\multichain_v8.lib` to the equivalent directory accessible to the Linux machine.
      -  Copy the DLL `%ROOT_DIR%\src\v8_win\build\Release\multichain_v8.dll` to `%ROOT_DIR%\src`.

### Build Multichain on the Linux machine

        cd $ROOT_DIR/multichain
        ./autogen.sh
        cd depends
        make HOST=x86_64-w64-mingw32
        cd ..
        ./configure --prefix=$(pwd)/depends/x86_64-w64-mingw32 --enable-cxx --disable-shared --enable-static --with-pic
        make
    
This will build `multichaind.exe`, `multichain-cli.exe` and `multitchain-util.exe` in the `src` directory.
