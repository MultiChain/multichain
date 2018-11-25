cmake_minimum_required(VERSION 3.0)
project(v8_win)

set(OPENSSL_ROOT_DIR "c:\\Users\\zvi\\Downloads\\OpenSSL")
set(OPENSSL_USE_STATIC_LIBS TRUE)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost REQUIRED COMPONENTS thread filesystem system)
find_package(OpenSSL REQUIRED)

get_filename_component(PROJECT_ROOT "../.." REALPATH)

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
)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

include_directories(${PROJECT_ROOT}/src ${V8_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
link_directories(${V8_LIBRARIES_DIR} ${V8_LIBRARIES_DIR}/third_party/icu ${Boost_LIBRARY_DIRS})
add_library(${CMAKE_PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_PROJECT_NAME}_EXPORTS
    _WIN32_WINNT=0x0501
    _SILENCE_FPOS_SEEKPOS_DEPRECATION_WARNING
    HAVE_DECL_STRNLEN=1
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
