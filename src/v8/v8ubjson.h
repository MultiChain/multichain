// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8UBJSON_H
#define V8UBJSON_H

#include "v8/v8blob.h"
#include <v8.h>

namespace mc_v8
{
class Blob;

int V82Ubj(v8::Isolate *isolate, v8::Local<v8::Value> value, BlobPtr blob);
v8::Local<v8::Value> Ubj2V8(v8::Isolate *isolate, BlobPtr blob, int *err);

} // namespace mc_v8

#endif // V8UBJSON_H
