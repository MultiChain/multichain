# Prerequisites

Building Google's V8 on Windows can only be done inside a Windows command prompt (`cmd.exe`), and not in PowerShell or any Unix-like shell (e.g. Cygwin, MSYS, Git Bash, WSL).

It requires the following software:

-   [Visual Studio 2017](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&rel=15) (community edition acceptable)
-   [Python 2.7](https://www.python.org/ftp/python/2.7.15/python-2.7.15.amd64.msi)
-   [Git](https://github.com/git-for-windows/git/releases/download/v2.19.1.windows.1/Git-2.19.1-64-bit.exe)

The steps described in this document have to be executed in a shell configured for Visual Studio 64 bit (<u>Start</u> / <u>Visual Studio 2017</u> / <u>x64 Native Tools Command Prompt</u>).

# Fetching, Building and Installing V8

The following instructions fetch the Google V8 JavaScript engine to your local machine, configure it to create static libraries, and builds them.

MultiChain uses V8 version 6.8, and requires at least 4 GB of RAM to build in a reasonable time. It will not build at all with less than 2 GB RAM.

## Get Google's `depot_tools`

Google's [`depot_tools`](http://dev.chromium.org/developers/how-tos/install-depot-tools) are used to manage Git checkouts and the build system.

-   Download [https://storage.googleapis.com/chrome-infra/depot\_tools.zip]() and expand the archive to the current directory.
-   Edit the PATH environment variable and insert the full path of `depot_tools` **before** the installed `python.exe` and `git.exe`.

## Fetch V8

The following commands check out V8 and select the branch used by MultiChain. Please note that this step downloads about 2 GB of data, and can take a long time (30 minutes or more).

    gclient
    fetch v8
    cd v8
    git checkout 6.8.290

## Configure and build V8

The V8 build system currently uses a proprietary version of the Ninja build system, called GN. It is part of the `depot_tools` installed earlier.

-   Execute `where python` to make sure that `depot_tools\python.bat` indeed comes *before* `python.exe`
-   Edit the file `BUILD.gn` and remove all lines containing `exe_and_shlib_deps` (8 occurrences)
-   Execute `python tools\dev\v8gen.py x64.release`
-   Execute `set RELEASE=out.gn\x64.release`
-   Edit the file `%RELEASE%\args.gn` to have the following content:

`args.gn`:

    is_debug = false
    target_cpu = "x64"
    v8_static_library = true
    is_component_build = false
    is_clang = false
    v8_enable_object_print = true
    treat_warnings_as_errors = false

-   Execute `gn gen %RELEASE%`
-   Execute `ninja -C %RELEASE% v8 d8`
