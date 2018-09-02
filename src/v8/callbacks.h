// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_CALLBACKS_H_
#define MULTICHAIN_CALLBACKS_H_

#include <v8.h>

namespace mc_v8
{

void filter_getfiltertxid(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_getfiltertransaction(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_setfilterparam(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_getfiltertxinput(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_getblock(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_getlastblockinfo(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_getassetinfo(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_getstreaminfo(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_verifypermission(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_verifymessage(const v8::FunctionCallbackInfo<v8::Value>& args);

} // namespace mc_v8

#endif /* MULTICHAIN_CALLBACKS_H_ */
