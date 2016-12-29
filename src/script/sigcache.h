// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_SCRIPT_SIGCACHE_H
#define BITCOIN_SCRIPT_SIGCACHE_H

#include "script/interpreter.h"

#include <vector>

class CPubKey;

class CachingTransactionSignatureChecker : public TransactionSignatureChecker
{
private:
    bool store;

public:
    CachingTransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, bool storeIn=true) : TransactionSignatureChecker(txToIn, nInIn), store(storeIn) {}

    bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const;
};

#endif // BITCOIN_SCRIPT_SIGCACHE_H
