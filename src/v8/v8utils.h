// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8UTILS_H_
#define V8UTILS_H_

#include "utils/util.h"
#include <boost/filesystem.hpp>
#include <fstream>
#include <unistd.h>
#include <v8.h>

namespace fs = boost::filesystem;

namespace mc_v8
{
/**
 * Convert a V8 Value to an std::string.
 *
 * @param isolate The v8::Isolate environment to use.
 * @param value   The V8 Value to convert.
 * @return        The equivalent std::string.
 */
inline std::string V82String(v8::Isolate *isolate, v8::Local<v8::Value> value)
{
    v8::String::Utf8Value v(isolate, value);
    return v.length() > 0 ? *v : std::string();
}

/**
 * Convert an std::string to a V8 Value.
 *
 * @param isolate The v8::Isolate environment to use.
 * @param str     The std::string to convert.
 * @return        The equivalent V8 Value.
 */
inline v8::Local<v8::String> String2V8(v8::Isolate *isolate, std::string str)
{
    return v8::String::NewFromUtf8(isolate, str.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
}

/**
 * Get a directory for multichain temporary files.
 */
inline fs::path GetTemporaryPidDirectory()
{
    return GetDataDir() / std::to_string(getpid());
}

/**
 * Write a blob to a binary file.
 *
 * @param filename The name of the file to write.
 * @param data     The data array to write.
 * @param size     The number of bytes to write.
 */
inline void WriteBinaryFile(fs::path filename, char *data, std::streamsize size)
{
    std::ofstream ofs(filename.string(), std::fstream::out | std::fstream::binary);
    ofs.write(data, size);
    ofs.close();
}
} // namespace mc_v8

#endif /* V8UTILS_H_ */
