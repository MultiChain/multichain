// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#ifndef MULTICHAIN_DBWRAP_H
#define MULTICHAIN_DBWRAP_H

#include "utils/util.h"
#include "multichain/multichain.h"
#include "version/clientversion.h"
#include "utils/serialize.h"
#include "utils/streams.h"
#include "utils/sync.h"
#include "version/bcversion.h"
#include "wallet/dbconst.h"
#include "wallet/dbflat.h"

#include <map>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>


class CDiskBlockIndex;
class COutPoint;

struct CBlockLocator;

extern unsigned int nWalletDBUpdated;

void ThreadFlushWalletDB(const std::string& strWalletFile);
void WalletDBLogVersionString();
bool RewriteWalletDB(const std::string& strFile, const char* pszSkip = NULL);

class CDBWrapEnv
{
private:
    void *m_lpDBEnv;
    
    void EnvShutdown();

public:
    CDBFlatEnv m_Env;
    mutable CCriticalSection cs_db;
    std::map<std::string, int>* m_lpMapFileUseCount;

    CDBWrapEnv();
    ~CDBWrapEnv();

    /**
     * Verify that database file strFile is OK. If it is not,
     * call the callback to try to recover.
     * This must be called BEFORE strFile is opened.
     * Returns true if strFile is OK.
     */
    
//    CDBConstEnv::VerifyResult Verify(std::string strFile, bool (*recoverFunc)(CDBWrapEnv& dbenv, std::string strFile));
    CDBConstEnv::VerifyResult Verify(std::string strFile);
    
    void SetSeekDBName(std::string strFile);
    
    /**
     * Salvage data from a file that Verify says is bad.
     * fAggressive sets the DB_AGGRESSIVE flag (see berkeley DB->verify() method documentation).
     * Appends binary key/value pairs to vResult, returns true if successful.
     * NOTE: reads the entire database into memory, so cannot be used
     * for huge databases.
     */
    
    bool Salvage(std::string strFile, bool fAggressive, std::vector<CDBConstEnv::KeyValPair>& vResult);

    bool Open(const boost::filesystem::path& path);
    void Close();
    void Flush(bool fShutdown);
    void CheckpointLSN(const std::string& strFile);

    void CloseDb(const std::string& strFile);
    bool RemoveDb(const std::string& strFile);

//    DbTxn* TxnBegin(int flags = DB_TXN_WRITE_NOSYNC)                          // DEFINE_DB #define	DB_TXN_WRITE_NOSYNC			0x00000020
    void* TxnBegin(int flags = MC_DBW_CODE_DB_TXN_WRITE_NOSYNC);            
    
    int RenameDb(const std::string& strOldFileName,const std::string& strNewFileName);
    bool Recover(std::string strFile, std::vector<CDBConstEnv::KeyValPair>& SalvagedData);
    
    
};

extern CDBWrapEnv bitdbwrap;


/** RAII class that provides access to a Berkeley database */
class CDBWrap
{
protected:
    
    void *m_lpDb;
    CDBFlat *m_lpDbFlat;
    std::string strFile;
    bool fReadOnly;

    explicit CDBWrap(const std::string& strFilename, const char* pszMode = "r+");
    ~CDBWrap();

public:
    void Flush();
    void Close();

private:
    CDBWrap(const CDBWrap&);
    void operator=(const CDBWrap&);

public:
    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        
        if(!Read(ssKey,ssValue))
        {
            return false;
        }
        
        try {
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        
        return true;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true)
    {
        // Key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        
        // Value
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;
    
        return Write(ssKey,ssValue);
    }

    template <typename K>
    bool Erase(const K& key)
    {
        // Key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        return Erase(ssKey);
    }

    template <typename K>
    bool Exists(const K& key)
    {
        // Key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        return Exists(ssKey);        
    }

    bool Read(CDataStream& ssKey, CDataStream& ssValue);
    bool Write(CDataStream& ssKey, CDataStream& ssValue, bool fOverwrite = true);
    bool Erase(CDataStream& ssKey);
    bool Exists(CDataStream& ssKey);
    
    
    
    
//    Dbc* GetCursor()
    void* GetCursor();
    void CloseCursor(void* cursor);

//    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags = DB_NEXT)
    int ReadAtCursor(void* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags = MC_DBW_CODE_DB_NEXT); 

public:
    
    bool TxnBegin();

    bool TxnCommit();

    bool TxnAbort();

    bool ReadVersion(int& nVersion);

    bool WriteVersion(int nVersion);

    
};



#endif /* MULTICHAIN_DBWRAP_H */

