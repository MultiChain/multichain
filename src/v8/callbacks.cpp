// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "callbacks.h"
#include "v8engine.h"
#include "v8utils.h"
#include "v8isolatemanager.h"
#include "utils/util.h"
#include "rpc/rpcserver.h"
#include "rpc/rpcprotocol.h"
#include <cassert>

namespace mc_v8
{

void filter_mcprint(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::Context::Scope contextScope(context);

    if (args.Length() < 1)
    {
        LogPrint("JS", "JS Error: Too few arguments to the print function.\n");
        return;
    }
    if (!args[0]->IsString())
    {
        LogPrint("JS", "JS Error: First argument to the print function must be a string.\n");
        return;
    }

    LogPrint("JS", (V82String(isolate, args[0]) + "\n").c_str());
}

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
 * @param name        The name of the RPC function.
 * @param rpcFunction The RPC function to call.
 * @param args        The V8 arguments/return value.
 * @param sanitize    An optional function to transform the RPC function result before returning it to JS.
 */
void CallRpcFunction(std::string name, rpcfn_type rpcFunction, const v8::FunctionCallbackInfo<v8::Value>& args,
        fnSanitize sanitize = nullptr)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::Context::Scope contextScope(context);

    IsolateData& isolateData = V8IsolateManager::Instance()->GetIsolateData(isolate);
    json_spirit::Object callbackData;

    auto args_array = v8::Array::New(isolate, args.Length());
//    for (int i = 0; i < args.Length(); ++i)
//    {
//        args_array->Set(i, args[i]);
//    }
//    v8::Local<v8::String> argsJson = v8::JSON::Stringify(context, args_array).ToLocalChecked();
//    std::string argsString = V82String(isolate, argsJson);
//    json_spirit::Value params;
//    json_spirit::read_string(argsString, params);
    json_spirit::Array params;
    for (int i = 0; i < args.Length(); ++i)
    {
        params.push_back(V82Jsp(isolate, args[i]));
    }
    if (isolateData.withCallbackLog)
    {
        callbackData.push_back(json_spirit::Pair("method", name));
        callbackData.push_back(json_spirit::Pair("params", params));
    }

    bool ok = true;
    json_spirit::Value result;
    try
    {
        result = rpcFunction(params, false);

        if (isolateData.withCallbackLog)
        {
            bool success = true;
            if (result.type() == json_spirit::obj_type)
            {
                auto obj = result.get_obj();
                auto it = std::find_if(obj.begin(), obj.end(), [](const json_spirit::Pair& pair) -> bool
                {
                    return pair.name_ == "code";
                });
                success = (it == obj.end());
            }
            callbackData.push_back(json_spirit::Pair("success", success));
            callbackData.push_back(json_spirit::Pair(success ? "result" : "error", result));
        }
    } catch (json_spirit::Object& e)
    {
        args.GetReturnValue().SetUndefined();
        if (isolateData.withCallbackLog)
        {
            callbackData.push_back(json_spirit::Pair("success", false));
            callbackData.push_back(json_spirit::Pair("error", e));
        }
        ok = false;
    } catch (std::exception& e)
    {
        args.GetReturnValue().SetUndefined();
        if (isolateData.withCallbackLog)
        {
            callbackData.push_back(json_spirit::Pair("success", false));
            callbackData.push_back(json_spirit::Pair("result", e.what()));
        }
        ok = false;
    }

    if (ok)
    {
        if (result.is_null())
        {
            args.GetReturnValue().SetUndefined();
            ok = false;
        }

        if (sanitize != nullptr)
        {
            sanitize(result);
        }

//        std::string resultString = json_spirit::write_string(result, false);
//        v8::Local<v8::String> resultJson = String2V8(isolate, resultString);
//        args.GetReturnValue().Set(v8::JSON::Parse(context, resultJson).ToLocalChecked());
        args.GetReturnValue().Set(Jsp2V8(isolate, result));
    }

    if (isolateData.withCallbackLog)
    {
        isolateData.callbacks.push_back(callbackData);
    }
}

#define FILTER_FUNCTION(name)                                           \
    void filter_##name(const v8::FunctionCallbackInfo<v8::Value>& args) \
    {                                                                   \
        CallRpcFunction(#name, name, args);                             \
    }

#define FILTER_FUNCTION_SANITIZE(name, sanitize)                        \
    void filter_##name(const v8::FunctionCallbackInfo<v8::Value>& args) \
    {                                                                   \
        CallRpcFunction(#name, name, args, sanitize);                   \
    }

FILTER_FUNCTION(getfiltertxid)
FILTER_FUNCTION(getfiltertransaction)
FILTER_FUNCTION(setfilterparam)
FILTER_FUNCTION(getfiltertxinput)
FILTER_FUNCTION(getlastblockinfo)
FILTER_FUNCTION(getassetinfo)
FILTER_FUNCTION(getstreaminfo)
FILTER_FUNCTION(verifypermission)
FILTER_FUNCTION(verifymessage)

} // namespace mc_v8

