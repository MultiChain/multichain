// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "core/main.h"
#include "utils/util.h"
#include "utils/utilparse.h"
#include "multichain/multichain.h"
#include "structs/base58.h"
#include "version/version.h"



#ifndef CUSTOM_H
#define CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

int custom_version_info(int version);
bool custom_good_for_coin_selection(const CScript& script);
bool custom_accept_transacton(const CTransaction& tx, 
                              const CCoinsViewCache &inputs,
                              int offset,
                              bool accept,
                              std::string& reason,
                              uint32_t *replay);

void custom_set_runtime_defaults(int exe_type);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_H */

