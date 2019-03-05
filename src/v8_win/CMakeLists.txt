cmake_minimum_required(VERSION 3.0)
project(multichain-v8)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost REQUIRED COMPONENTS thread filesystem)

include(ExternalProject)
ExternalProject_Add(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.2.1
    EXCLUDE_FROM_ALL TRUE
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    )
#UPDATE_COMMAND  ""
#CONFIGURE_COMMAND ""
#BUILD_COMMAND ""
#INSTALL_COMMAND ""
ExternalProject_Get_Property(spdlog INSTALL_DIR)
set(SPDLOG_INCLUDE_DIR ${INSTALL_DIR}/include)
#find_package(spdlog REQUIRED PATHS ${INSTALL_DIR})
message(STATUS "SPDLOG_INCLUDE_DIR=${SPDLOG_INCLUDE_DIR}")

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." REALPATH)
get_filename_component(V8_ROOT_DIR "v8build/v8" REALPATH BASE_DIR ${PROJECT_ROOT})
set(V8_INCLUDE_DIR ${V8_ROOT_DIR}/include)
set(V8_LIBRARIES_DIR ${V8_ROOT_DIR}/out.gn/x64.release/obj)
message(STATUS "V8_INCLUDE_DIR=${V8_INCLUDE_DIR}")

set(HEADERS
    callbacks.h
    v8blob.h
    v8engine.h
    v8filter.h
    v8ubjson.h
    v8utils.h
    )

set(SOURCES
    callbacks_win.cpp
    v8blob.cpp
    v8engine.cpp
    v8filter.cpp
    v8ubjson.cpp
    v8utils.cpp
    v8_win.cpp
    multichain-v8.rc
    )

string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})

include_directories(
    ${PROJECT_ROOT}/src
    ${V8_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    )

link_directories(${V8_LIBRARIES_DIR} ${V8_LIBRARIES_DIR}/third_party/icu ${Boost_LIBRARY_DIRS})
add_library(${CMAKE_PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
#add_dependencies(${CMAKE_PROJECT_NAME} spdlog)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
    HAVE_DECL_STRNLEN=1
    _SILENCE_FPOS_SEEKPOS_DEPRECATION_WARNING
    _CRT_NONSTDC_NO_WARNINGS
    )
target_link_libraries(${CMAKE_PROJECT_NAME}
    v8_libbase
    v8_libplatform
    v8_base_0
    v8_base_1
    v8_external_snapshot
    v8_libsampler
    v8_data
    icuuc
    icui18n
    winmm
    dbghelp
    shlwapi
    )
