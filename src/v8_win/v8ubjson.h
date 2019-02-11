// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8UBJSON_H
#define V8UBJSON_H

#include <v8.h>
extern void _v8_internal_Print_Object(void *object);

namespace mc_v8
{
class Blob;
using BlobPtr = std::shared_ptr<Blob>;

/**
 * Convert a V8 Value to UBJSON.
 *
 * @param isolate The V8 Isolate to use.
 * @param value   The V8 value to convert.
 * @param blob    The Blob containing the resulting UBJSON content.
 * @return        MC_ERR_NOERROR if all is well, error code if not.
 */
int V82Ubj(v8::Isolate *isolate, v8::Local<v8::Value> value, BlobPtr blob);

/**
 * Convert a UBJSON blob to a V8 Value.
 *
 * @param isolate The V8 Isolate to use.
 * @param blob    The Blob containing the UBJSON to convert.
 * @param err     Output error code.
 * @return        The converted value if all is well, a blank V8 Value if not.
 */
v8::MaybeLocal<v8::Value> Ubj2V8(v8::Isolate *isolate, BlobPtr blob, int *err);

} // namespace mc_v8

#endif // V8UBJSON_H
