// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/callbacks.h"
#include "v8_win/v8engine.h"
#include "v8_win/v8json_spirit.h"
#include "v8_win/v8utils.h"
#include <cassert>

namespace mc_v8
{
/**
 * Call an RPC function from a V8 JS callback.
 *
 * Marshal the arguments and the return value between V8 and json_spirit using intermediate JSON strings.
 * Optionally filter the result before returning it to JS.
 *
 * @param name        The name of the RPC function.
 * @param args        The V8 arguments/return value.
 */
void CallRpcFunction(std::string name, const v8::FunctionCallbackInfo<v8::Value> &args)
{
    v8::Isolate *isolate = args.GetIsolate();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::Context::Scope contextScope(context);

    IFilterCallback *filterCallback = static_cast<IFilterCallback *>(args.Data().As<v8::External>()->Value());

    json_spirit::Array params;
    for (int i = 0; i < args.Length(); ++i)
    {
        params.push_back(V82Jsp(isolate, args[i]));
    }

    json_spirit::Value result;
    filterCallback->JspCallback(name, params, result);
    if (result.is_null())
    {
        args.GetReturnValue().SetUndefined();
    }
    else
    {
        args.GetReturnValue().Set(Jsp2V8(isolate, result));
    }
}

// clang-format off
#define FILTER_FUNCTION(name)                                           \
    void filter_##name(const v8::FunctionCallbackInfo<v8::Value> &args) \
    {                                                                   \
        CallRpcFunction(#name, args);                                   \
    }

FILTER_FUNCTION(getfiltertxid)
FILTER_FUNCTION(getfiltertransaction)
FILTER_FUNCTION(getfilterstreamitem)
FILTER_FUNCTION(getfilterassetbalances)
FILTER_FUNCTION(setfilterparam)
FILTER_FUNCTION(getfiltertxinput)
FILTER_FUNCTION(getlastblockinfo)
FILTER_FUNCTION(getassetinfo)
FILTER_FUNCTION(getstreaminfo)
FILTER_FUNCTION(verifypermission)
FILTER_FUNCTION(verifymessage)

#define FILTER_LOOKUP(name) { #name, filter_##name }

std::map<std::string, v8::FunctionCallback> callbackLookup{
    FILTER_LOOKUP(getfiltertxid),
    FILTER_LOOKUP(getfiltertransaction),
    FILTER_LOOKUP(getfilterstreamitem),
    FILTER_LOOKUP(getfilterassetbalances),
    FILTER_LOOKUP(setfilterparam),
    FILTER_LOOKUP(getfiltertxinput),
    FILTER_LOOKUP(getlastblockinfo),
    FILTER_LOOKUP(getassetinfo),
    FILTER_LOOKUP(getstreaminfo),
    FILTER_LOOKUP(verifypermission),
    FILTER_LOOKUP(verifymessage)
};
// clang-format on
} // namespace mc_v8
