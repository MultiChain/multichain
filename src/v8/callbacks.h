// Copyright (c) 2014-2018 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_CALLBACKS_H_
#define MULTICHAIN_CALLBACKS_H_

#include <v8.h>

namespace mc_v8
{

void filter_getfiltertxid(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_getrawtransaction(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_gettxout(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_listassets(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_liststreams(const v8::FunctionCallbackInfo<v8::Value>& args);
void filter_listpermissions(const v8::FunctionCallbackInfo<v8::Value>& args);

} // namespace mc_v8

#endif /* MULTICHAIN_CALLBACKS_H_ */
