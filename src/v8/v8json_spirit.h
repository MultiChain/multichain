#ifndef V8JSON_SPIRIT_H
#define V8JSON_SPIRIT_H

#include "json/json_spirit.h"
#include <v8.h>

namespace mc_v8
{
v8::Local<v8::Value> Jsp2V8(v8::Isolate *isolate, const json_spirit::Value &j);
json_spirit::Value V82Jsp(v8::Isolate *isolate, v8::Local<v8::Value> v);
} // namespace mc_v8

#endif // V8JSON_SPIRIT_H
