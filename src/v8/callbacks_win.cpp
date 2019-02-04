// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8/callbacks.h"
#include "v8/v8engine.h"
#include "v8/v8utils.h"
#include "v8/v8ubjson.h"
#include "v8/v8blob.h"
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

    auto v8args = v8::Array::New(isolate, args.Length());
    for (int i = 0; i < args.Length(); ++i)
    {
        v8args->Set(static_cast<unsigned>(i), args[i]);
    }

    BlobPtr argsBlob = Blob::Instance("args");
    BlobPtr resultBlob = Blob::Instance("result");
    V82Ubj(isolate, v8args, argsBlob);
    unsigned char *result;
    size_t resultSize;

    filterCallback->UbjCallback(name.c_str(), argsBlob->Data(), &result, &resultSize);
    resultBlob->Set(result, resultSize);

    if (resultBlob->IsEmpty())
    {
        args.GetReturnValue().SetUndefined();
    }
    else
    {
        int err;
        args.GetReturnValue().Set(Ubj2V8(isolate, resultBlob, &err));
    }

    if (result != nullptr)
    {
        delete [] result;
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
