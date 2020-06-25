// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "v8_win/callbacks.h"
#include "v8_win/v8blob.h"
#include "v8_win/v8engine.h"
#include "v8_win/v8ubjson.h"
#include "v8_win/v8utils.h"
#include <cassert>

#include <boost/foreach.hpp>
using namespace std;


namespace mc_v8
{    
  std::map<std::string, std::string> callbackNameToFixed;
  std::map<std::string, std::string> callbackFixedToName;

  std::string callbackFixedName(std::string name)
  {
      std::map<std::string, std::string>::const_iterator it=callbackNameToFixed.find(name);
      if(it == callbackNameToFixed.end())
      {
          int n=callbackNameToFixed.size();
          string fixed=strprintf("fixed%06d",n);
          callbackNameToFixed.insert(make_pair(name,fixed));
          callbackFixedToName.insert(make_pair(fixed,name));
          return name;
      }
      return it->second;
  }
    
/**
 * Call an RPC function from a V8 JS callback.
 *
 * Marshal the arguments and the return value between V8 and json_spirit using intermediate JSON strings.
 * Optionally filter the result before returning it to JS.
 *
 * @param name        The name of the RPC function.
 * @param args        The V8 arguments/return value.
 */
void CallRpcFunction(std::string name, const v8::FunctionCallbackInfo<v8::Value>& args)
{
    logger->debug("CallRpcFunction(name={}) - enter", name);

    v8::Isolate* isolate = args.GetIsolate();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolateScope(isolate);
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::Context::Scope contextScope(context);

    IFilterCallback* filterCallback = static_cast<IFilterCallback*>(args.Data().As<v8::External>()->Value());

    auto v8args = v8::Array::New(isolate, args.Length());
    for (int i = 0; i < args.Length(); ++i) {
        v8args->Set(static_cast<unsigned>(i), args[i]);
    }

    BlobPtr argsBlob = Blob::Instance("args");
    BlobPtr resultBlob = Blob::Instance("result");
    V82Ubj(isolate, v8args, argsBlob);
    logger->debug("  argsBlob={}", argsBlob->ToString());

    std::map<std::string, std::string>::const_iterator it=callbackFixedToName.find(name);
    std::string real_name=it->second;
    logger->debug("CallRpcFunction(realname={}) - enter", real_name);
    printf("%s -> %s\n",name.c_str(),real_name.c_str());
    filterCallback->UbjCallback(real_name.c_str(),
                                reinterpret_cast<Blob_t*>(argsBlob.get()),
                                reinterpret_cast<Blob_t*>(resultBlob.get()));
    logger->debug("  resultBlob={}", resultBlob->ToString());

    if (resultBlob->IsEmpty()) {
        args.GetReturnValue().SetUndefined();
    } else {
        int err;
        logger->debug("  Calling Ubj2V8");
        v8::MaybeLocal<v8::Value> mv = Ubj2V8(isolate, resultBlob, &err);
        v8::Local<v8::Value> v;
        if (mv.ToLocal(&v)) {
            if (fDebug) {
                logger->debug("  Return value:");
                _v8_internal_Print_Object(*(reinterpret_cast<v8::internal::Object **>(*v)));
            }
        } else {
            logger->warn("  Return value is empty (err={})", err);
        }
        args.GetReturnValue().Set(v);
    }

    logger->debug("CallRpcFunction - leave");
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
FILTER_FUNCTION(getfilterstream)
FILTER_FUNCTION(getfilterassetbalances)
FILTER_FUNCTION(getvariableinfo)
FILTER_FUNCTION(getvariablevalue)
FILTER_FUNCTION(getvariablehistory)
FILTER_FUNCTION(setfilterparam)
FILTER_FUNCTION(getfiltertxinput)
FILTER_FUNCTION(getlastblockinfo)
FILTER_FUNCTION(getassetinfo)
FILTER_FUNCTION(getstreaminfo)
FILTER_FUNCTION(verifypermission)
FILTER_FUNCTION(verifymessage)


FILTER_FUNCTION(fixed000000)
FILTER_FUNCTION(fixed000001)
FILTER_FUNCTION(fixed000002)
FILTER_FUNCTION(fixed000003)
FILTER_FUNCTION(fixed000004)
FILTER_FUNCTION(fixed000005)
FILTER_FUNCTION(fixed000006)
FILTER_FUNCTION(fixed000007)
FILTER_FUNCTION(fixed000008)
FILTER_FUNCTION(fixed000009)
FILTER_FUNCTION(fixed000010)
FILTER_FUNCTION(fixed000011)
FILTER_FUNCTION(fixed000012)
FILTER_FUNCTION(fixed000013)
FILTER_FUNCTION(fixed000014)
FILTER_FUNCTION(fixed000015)
FILTER_FUNCTION(fixed000016)
FILTER_FUNCTION(fixed000017)
FILTER_FUNCTION(fixed000018)
FILTER_FUNCTION(fixed000019)
FILTER_FUNCTION(fixed000020)
FILTER_FUNCTION(fixed000021)
FILTER_FUNCTION(fixed000022)
FILTER_FUNCTION(fixed000023)
FILTER_FUNCTION(fixed000024)
FILTER_FUNCTION(fixed000025)
FILTER_FUNCTION(fixed000026)
FILTER_FUNCTION(fixed000027)
FILTER_FUNCTION(fixed000028)
FILTER_FUNCTION(fixed000029)


#define FILTER_LOOKUP(name) { #name, filter_##name }

std::map<std::string, v8::FunctionCallback> callbackLookup{
    FILTER_LOOKUP(getfiltertxid),
    FILTER_LOOKUP(getfiltertransaction),
    FILTER_LOOKUP(getfilterstreamitem),
    FILTER_LOOKUP(getfilterstream),
    FILTER_LOOKUP(getfilterassetbalances),
    FILTER_LOOKUP(getvariableinfo),
    FILTER_LOOKUP(getvariablevalue),
    FILTER_LOOKUP(getvariablehistory),
    FILTER_LOOKUP(setfilterparam),
    FILTER_LOOKUP(getfiltertxinput),
    FILTER_LOOKUP(getlastblockinfo),
    FILTER_LOOKUP(getassetinfo),
    FILTER_LOOKUP(getstreaminfo),
    FILTER_LOOKUP(verifypermission),
    FILTER_LOOKUP(verifymessage),
    FILTER_LOOKUP(fixed000000),
    FILTER_LOOKUP(fixed000001),
    FILTER_LOOKUP(fixed000002),
    FILTER_LOOKUP(fixed000003),
    FILTER_LOOKUP(fixed000004),
    FILTER_LOOKUP(fixed000005),
    FILTER_LOOKUP(fixed000006),
    FILTER_LOOKUP(fixed000007),
    FILTER_LOOKUP(fixed000008),
    FILTER_LOOKUP(fixed000009),
    FILTER_LOOKUP(fixed000010),
    FILTER_LOOKUP(fixed000011),
    FILTER_LOOKUP(fixed000012),
    FILTER_LOOKUP(fixed000013),
    FILTER_LOOKUP(fixed000014),
    FILTER_LOOKUP(fixed000015),
    FILTER_LOOKUP(fixed000016),
    FILTER_LOOKUP(fixed000017),
    FILTER_LOOKUP(fixed000018),
    FILTER_LOOKUP(fixed000019),
    FILTER_LOOKUP(fixed000020),
    FILTER_LOOKUP(fixed000021),
    FILTER_LOOKUP(fixed000022),
    FILTER_LOOKUP(fixed000023),
    FILTER_LOOKUP(fixed000024),
    FILTER_LOOKUP(fixed000025),
    FILTER_LOOKUP(fixed000026),
    FILTER_LOOKUP(fixed000027),
    FILTER_LOOKUP(fixed000028),
    FILTER_LOOKUP(fixed000029),
};
// clang-format on
} // namespace mc_v8
