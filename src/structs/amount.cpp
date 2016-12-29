// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "structs/amount.h"

#include "utils/tinyformat.h"

CFeeRate::CFeeRate(const CAmount& nFeePaid, size_t nSize)
{
    if (nSize > 0)
        nSatoshisPerK = nFeePaid*1000/nSize;
    else
        nSatoshisPerK = 0;
}

CAmount CFeeRate::GetFee(size_t nSize) const
{
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
