# Prerequisites

Building Google's V8 on Windows can only be done inside a Windows command prompt (`cmd.exe`), and not in PowerShell or any Unix-like shell (e.g. Cygwin, MSYS, Git Bash, WSL).

It requires the following software:

-   Visual Studio 2017 - the community edition is fine
-   Python 2.7
-   Git

# Fetching, Building and Installing V8

The following instructions fetch the Google V8 JavaScript engine to your local machine, configure it to create static libraries, and builds them.

MultiChain uses V8 version 6.8, and requires at least 4 GB of RAM to build in a reasonable time. It will not build at all with less than 2 GB RAM.

## Get Google's `depot_tools`

Google's [`depot_tools`](http://dev.chromium.org/developers/how-tos/install-depot-tools) are used to manage Git checkouts and the build system.

-   Download [https://storage.googleapis.com/chrome-infra/depot\_tools.zip]() and expand the archive to the current directory.
-   Edit the PATH environment variable and insert the full path of `depot_tools` **before** the installed `python.exe`.
-   Execute `where python` to make sure that `depot_tools\python.bat` indeed comes *before* `python.exe`.

## Fetch V8

The following commands check out V8 and select the branch used by MultiChain. Please note that this step downloads about 2 GB of data, and can take a long time (30 minutes or more).

    gclient
    fetch v8
    cd v8
    git checkout 6.8.290

## Configure and build V8

The V8 build system currently uses a proprietary version of the Ninja build system, called GN. It is part of the `depot_tools` installed earlier.

-   Edit the file `BUILD.gn` and remove all lines containing `exe_and_shlib_deps`
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

The folowing steps have to be executed in a shell configured for Visual Studio 64 bit (<u>Start</u> / <u>Visual Studio 2017</u> / <u>x64 Native Tools Command Prompt</u>). There is no harm in performing all the instructions in this document inside such a command shell.

-   Execute `gn gen %RELEASE%`
-   Execute `ninja -C %RELEASE% v8`
