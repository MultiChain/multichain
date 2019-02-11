// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef V8JSON_SPIRIT_H
#define V8JSON_SPIRIT_H

#include "json/json_spirit.h"
#include <v8.h>

namespace mc_v8
{
/**
 * Convert a json_spirit Value to a V8 Value.
 *
 * @param isolate The V8 Isolate to use.
 * @param j       The json_spirit Value to convert.
 * @return        The converted value if all is well, a blank V8 Value if not.
 */
v8::Local<v8::Value> Jsp2V8(v8::Isolate *isolate, const json_spirit::Value &j);

/**
 * Convert a V8 Value to a json_spirit Value.
 *
 * @param isolate The V8 Isolate to use.
 * @param v       The V8 Value to convert.
 * @return        The converted value if all is well, a null json_spirit Value if not.
 */
json_spirit::Value V82Jsp(v8::Isolate *isolate, v8::Local<v8::Value> v);
} // namespace mc_v8

#endif // V8JSON_SPIRIT_H
