// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "structs/amount.h"

#include "utils/tinyformat.h"

/* We want to fix a single transaction fee for every transaction in our blockchain 
 * Because of that, we will consider here every transaction as being of the same size
 */

// Unique-fee start
constexpr int canonical_transaction_size = 1;
// Unique-fee end


CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nSize)
{
    // Unique-fee start
    nSize = canonical_transaction_size;    
    // Unique-fee end

    if (nSize > 0)
        nSatoshisPerK = nFeePaid*1000/nSize;
    else
        nSatoshisPerK = 0;
}

CAmount CFeeRate::GetFee(size_t nSize) const
{
    // Unique-fee start
    nSize = canonical_transaction_size;
    // Unique-fee end 
    
    CAmount nFee = nSatoshisPerK*nSize / 1000;

    if (nFee == 0 && nSatoshisPerK > 0)
        nFee = nSatoshisPerK;

    return nFee;
}

std::string CFeeRate::ToString() const
{
/* MCHN START */    
    if(COIN == 0)
    {
        return strprintf("%d.%08d BTC/kB", nSatoshisPerK, nSatoshisPerK);        
    }
/* MCHN END */    
    return strprintf("%d.%08d BTC/kB", nSatoshisPerK / COIN, nSatoshisPerK % COIN);
}
