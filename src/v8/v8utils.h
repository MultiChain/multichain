// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8UTILS_H_
#define V8UTILS_H_

#include <fstream>
#include <v8.h>
#include <boost/filesystem.hpp>
#include <unistd.h>

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
inline std::string V82String(v8::Isolate* isolate, v8::Local<v8::Value> value)
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
inline v8::Local<v8::String> String2V8(v8::Isolate* isolate, std::string str)
{
    return v8::String::NewFromUtf8(isolate, str.c_str(), v8::NewStringType::kNormal).ToLocalChecked();
}

inline fs::path GetTemporaryPidDirectory()
{
    return fs::path("/tmp") / "multichain" / std::to_string(getpid());
}

inline void WriteBinaryFile(fs::path filename, char* data, size_t size)
{
    std::ofstream ofs(filename.string(), std::fstream::out | std::fstream::binary);
    ofs.write(data, size);
    ofs.close();
}
} // namespace mc_v8

#endif /* V8UTILS_H_ */
