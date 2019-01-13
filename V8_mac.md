# Fetching, Building and Installing V8 (on MacOS Sierra)

The following instructions fetch the Google V8 JavaScript engine to your local machine and configure it to create static libraries in the location where the MultiChain build system expects to find them.

MultiChain uses V8 version 6.8, and requires at least 4 GB of RAM to build in a reasonable time. It will not build at all with less than 2 GB RAM.

## Clone Google's depot_tools

Google's [depot_tools](http://dev.chromium.org/developers/how-tos/install-depot-tools) are used by the Google build system to manage Git checkouts.

    git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git
    export PATH=${PATH}:$(pwd)/depot_tools

## Fetch V8

The following commands check out V8 and select the branch used by MultiChain. Please note that this step downloads about 2 GB of data, and can take a long time (30 minutes or more).

    gclient
    fetch v8
    cd v8
    git checkout 6.8.290

## Configure and build V8

The V8 build system currently uses a proprietary version of the Ninja build system, called GN. It is part of the `depot_tools` installed earlier.

    find . -name BUILD.gn -exec sed -i bak '/exe_and_shlib_deps/d' {} \;
    tools/dev/v8gen.py x64.release
    RELEASE=out.gn/x64.release
    cat > $RELEASE/args.gn << END
    is_debug = false
    target_cpu = "x64"
    is_component_build = false
    v8_static_library = true
    use_custom_libcxx = false
    use_custom_libcxx_for_host = false
    END

The selected release of the V8 sources requires relaxing two compiler checks to prevent compilation errors.

-   Open the file `build/config/compiler/BUILD.gn` in your favorite editor.

-   Locate the folllowing lines (currently at **1464**):

        if (is_clang) {
          cflags += [
          
-   Add the following two lines to **the end** of the block and save the file:

        "-Wno-defaulted-function-deleted",
        "-Wno-null-pointer-arithmetic",

Build the V8 libraries:

    gn gen $RELEASE
    ninja -C $RELEASE v8 d8

Create an additional library embedding the V8 initial snapshot blobs:

    brew install python
    pip install pathlib2
    cd $RELEASE
    python $MULTICHAIN_HOME/depends/v8_data_lib.py
