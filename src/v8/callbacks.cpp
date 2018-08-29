// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "callbacks.h"
#include "v8engine.h"
#include "v8utils.h"
#include "utils/util.h"
#include "rpc/rpcserver.h"
#include <cassert>

namespace mc_v8
{

/**
 * Signature of a function to remove non-deterministic or sensitive elements from RPC function output.
 *
 * @param value The RPC function output value. Transform this value in-place.
 */
typedef void (*fnSanitize)(json_spirit::Value& value);

/**
 * Call an RPC function from a V8 JS callback.
 *
 * Marshal the arguments and the return value between V8 and json_spirit using intermediate JSON strings.
 * Optionally filter the result before returning it to JS.
 *
 * @param rpcFunction The RPC function to call.
 * @param args        The V8 arguments/return value.
 * @param sanitize    An optional function to transform the RPC function result before returning it to JS.
 */
void CallRpcFunction(rpcfn_type rpcFunction, const v8::FunctionCallbackInfo<v8::Value>& args, fnSanitize sanitize =
        nullptr)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::Context::Scope contextScope(context);

    auto args_array = v8::Array::New(isolate, args.Length());
    for (int i = 0; i < args.Length(); ++i)
    {
        args_array->Set(i, args[i]);
    }
    v8::Local<v8::String> argsJson = v8::JSON::Stringify(context, args_array).ToLocalChecked();
    std::string argsString = V82String(isolate, argsJson);
    json_spirit::Value params;
    json_spirit::read_string(argsString, params);

    json_spirit::Value result;
    try
    {
        result = rpcFunction(params.get_array(), false);
    } catch (...)
    {
        args.GetReturnValue().SetUndefined();
        return;
    }

    if (result.is_null())
    {
        args.GetReturnValue().SetUndefined();
        return;
    }

    if (sanitize != nullptr)
    {
        sanitize(result);
    }

    std::string resultString = json_spirit::write_string(result, false);
    v8::Local<v8::String> resultJson = String2V8(isolate, resultString);
    args.GetReturnValue().Set(v8::JSON::Parse(context, resultJson).ToLocalChecked());
}

#define FILTER_FUNCTION(name)                                           \
    void filter_##name(const v8::FunctionCallbackInfo<v8::Value>& args) \
    {                                                                   \
        CallRpcFunction(name, args);                                    \
    }

#define FILTER_FUNCTION_SANITIZE(name, sanitize)                        \
    void filter_##name(const v8::FunctionCallbackInfo<v8::Value>& args) \
    {                                                                   \
        CallRpcFunction(name, args, sanitize);                          \
    }

FILTER_FUNCTION(getfiltertxid)
FILTER_FUNCTION(getfiltertransaction)
FILTER_FUNCTION(setfilterparam)
FILTER_FUNCTION(gettxout)
FILTER_FUNCTION(getblock)
FILTER_FUNCTION(getlastblockinfo)
FILTER_FUNCTION(listassets)
FILTER_FUNCTION(liststreams)
FILTER_FUNCTION(listpermissions)

} // namespace mc_v8

