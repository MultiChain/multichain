// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

/**
 * Money parsing/formatting utilities.
 */
#ifndef BITCOIN_UTILMONEYSTR_H
#define BITCOIN_UTILMONEYSTR_H

#include <stdint.h>
#include <string>

#include "structs/amount.h"

std::string FormatMoney(const CAmount& n, bool fPlus=false);
bool ParseMoney(const std::string& str, CAmount& nRet);
bool ParseMoney(const char* pszIn, CAmount& nRet);

#endif // BITCOIN_UTILMONEYSTR_H
