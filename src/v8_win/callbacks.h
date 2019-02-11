// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_CALLBACKS_H_
#define MULTICHAIN_CALLBACKS_H_

#include <map>
#include <v8.h>

namespace mc_v8
{
extern std::map<std::string, v8::FunctionCallback> callbackLookup;

} // namespace mc_v8

#endif /* MULTICHAIN_CALLBACKS_H_ */
