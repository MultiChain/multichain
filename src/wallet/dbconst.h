// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#ifndef MULTICHAIN_DBCONST_H
#define MULTICHAIN_DBCONST_H

// Codes consistent with db.h

#define MC_DBW_CODE_DB_NEXT                      16
#define MC_DBW_CODE_DB_SET_RANGE                 28
#define MC_DBW_CODE_DB_NOTFOUND              -30988
#define MC_DBW_CODE_DB_NOSERVER              -30991
#define MC_DBW_CODE_DB_TXN_WRITE_NOSYNC  0x00000020

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

