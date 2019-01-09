// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#ifndef MULTICHAIN_DBCONST_H
#define MULTICHAIN_DBCONST_H

class CDBConstEnv
{
public:
    mutable CCriticalSection cs_dbwrap;

    CDBConstEnv(){};
    ~CDBConstEnv(){};

    
    enum VerifyResult { VERIFY_OK,
                        RECOVER_OK,
                        RECOVER_FAIL };
    
    typedef std::pair<std::vector<unsigned char>, std::vector<unsigned char> > KeyValPair;
};


#endif /* DBCONST_H */

