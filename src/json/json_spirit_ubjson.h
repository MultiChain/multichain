// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef JSON_SPIRIT_UBJSON_H
#define JSON_SPIRIT_UBJSON_H

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"
#include "multichain/multichain.h"

using namespace std;
using namespace json_spirit;

#ifdef __cplusplus
extern "C" {
#endif

int ubjson_write(Value json_value,mc_Script *lpScript);
Value ubjson_read(const unsigned char *elem,size_t elem_size,int *err);


#ifdef __cplusplus
}
#endif

#endif /* JSON_SPIRIT_UBJSON_H */

