// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#ifndef MULTICHAIN_DBFLAT_H
#define MULTICHAIN_DBFLAT_H

#include "utils/util.h"
#include "multichain/multichain.h"
#include "version/clientversion.h"
#include "utils/serialize.h"
#include "utils/streams.h"
#include "utils/sync.h"
#include "version/bcversion.h"
#include "wallet/dbconst.h"

#include <map>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#define MC_DBF_FLAGS_FIELDSIZE                   4


#define MC_DBF_FLAGS_EMPTY              0x00000000
#define MC_DBF_FLAGS_SIZE_KEY_LOW       0x00000100
#define MC_DBF_FLAGS_SIZE_KEY_HIGH      0x00000200
#define MC_DBF_FLAGS_SIZE_VALUE_LOW     0x00000400
#define MC_DBF_FLAGS_SIZE_VALUE_HIGH    0x00000800
#define MC_DBF_FLAGS_DELETED            0x00010000


int GetWalletDatVersion(const std::string& strWalletFile);

typedef struct mc_DBFlatPos
{
    uint32_t m_Offset;
    uint32_t m_Flags;
    uint32_t m_KeyLen;
    uint32_t m_ValLen;
    
    mc_DBFlatPos()
    {
        Zero();
    }
    
    void Zero();
    uint32_t NextOffset();
    uint32_t ValueOffset();
    int SetSizeFlags(size_t size,bool is_key);
} mc_DBFlatPos;

class CDBFlatEnv
{
public:
    boost::filesystem::path m_DirPath;
    std::string m_FileName;
    std::map <std::vector<unsigned char>,mc_DBFlatPos> m_FileRows;
    uint32_t m_FileSize;
    
    CDBFlatEnv()
    {
        m_DirPath.clear();
        m_FileName.clear();
        m_FileRows.clear();
        m_FileSize=0;
    }
    
    ~CDBFlatEnv()
    {
        
    }

    CDBConstEnv::VerifyResult Verify(std::string strFile,std::vector<CDBConstEnv::KeyValPair>* lpvResultOut = NULL);
    
    
    bool Salvage(std::string strFile, bool fAggressive, std::vector<CDBConstEnv::KeyValPair>& vResult);

    bool Open(const boost::filesystem::path& path);
    void Close();
    
    bool RemoveDb(const std::string& strFile);

    bool CopyDb(const std::string& strOldFileName,const std::string& strNewFileName);
    int RenameDb(const std::string& strOldFileName,const std::string& strNewFileName);
    bool Recover(std::string strFile, std::vector<CDBConstEnv::KeyValPair>& SalvagedData);        
};

class CDBFlat
{
public:
    
    CDBFlatEnv *m_lpEnv;    
    std::string m_FileName;
    std::string m_OpenMode;
    bool m_CanSeek;
    bool m_CanWrite;
    int m_FileHan;
    uint32_t m_FileSize;

    explicit CDBFlat(CDBFlatEnv *lpEnv, const std::string& strFilename, const char* pszMode = "r+");
    CDBFlat()
    {
        Zero();
    }
    ~CDBFlat()
    {
        Close();
    }

    void Zero();
    
public:
    
    int Open(CDBFlatEnv *lpEnv, const std::string& strFilename, const char* pszMode = "r+");
    void Flush();
    void Close();
    void Lock(bool for_size=false);
    void UnLock();
    void SetFileOffset(uint32_t offset);
    uint32_t GetFileSize(bool real=true);
private:
    CDBFlat(const CDBFlat&);
    void operator=(const CDBFlat&);
public:
    
    bool Read(CDataStream& ssKey, CDataStream& ssValue);
    bool Write(CDataStream& ssKey, CDataStream& ssValue, bool fOverwrite = true);
    bool Erase(CDataStream& ssKey);
    bool Exists(CDataStream& ssKey);
    
    void* GetCursor();
    void CloseCursor(void* cursor);

    int ReadAtCursor(void* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags = MC_DBW_CODE_DB_NEXT, mc_DBFlatPos *lpPosRes=NULL); 

public:
    
    bool TxnBegin();

    bool TxnCommit();

    bool TxnAbort();

    bool ReadVersion(int& nVersion);

    bool WriteVersion(int nVersion);

    bool static Rewrite(CDBFlatEnv *lpEnv,const std::string& strFile, const char* pszSkip = NULL);
    
};



#endif /* MULTICHAIN_DBFLAT_H */

