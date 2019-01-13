// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "wallet/dbwrap.h"

#include "wallet/db.h"

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/version.hpp>

#include <openssl/rand.h>

using namespace std;
using namespace boost;

unsigned int nWalletDBUpdated;

CDBWrapEnv bitdbwrap;

void WalletDBLogVersionString()
{
    LogPrintf("Using BerkeleyDB version %s\n", DbEnv::version(0, 0, 0));
}

int GetWalletDatVersion(const std::string& strWalletFile)
{
    return 2;
}

void CDBWrapEnv::EnvShutdown()
{
    
}

CDBWrapEnv::CDBWrapEnv()
{    
    m_lpDBEnv=NULL;
    m_lpMapFileUseCount=NULL;
/*    
    if( (mc_gState->m_WalletMode & MC_WMD_FLAT_DAT_FILE) == 0 )
    {
        m_lpDBEnv=new CDBEnv;
        m_lpMapFileUseCount=&(((CDBEnv*)m_lpDBEnv)->mapFileUseCount);
    }
 */ 
}

CDBWrapEnv::~CDBWrapEnv()
{
    EnvShutdown();
}

void CDBWrapEnv::Close()
{
    EnvShutdown();
}

bool CDBWrapEnv::Open(const boost::filesystem::path& pathIn)
{
    if( (mc_gState->m_WalletMode & MC_WMD_FLAT_DAT_FILE) == 0 )
    {
        m_lpDBEnv=&bitdb;
//        m_lpDBEnv=new CDBEnv;
        m_lpMapFileUseCount=&(((CDBEnv*)m_lpDBEnv)->mapFileUseCount);
    }
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Open(pathIn);    
    }
    
    return true;
}

CDBConstEnv::VerifyResult CDBWrapEnv::Verify(std::string strFile)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Verify(strFile);    
    }
    
    return CDBConstEnv::VERIFY_OK;
}


bool CDBWrapEnv::Salvage(std::string strFile, bool fAggressive, std::vector<CDBConstEnv::KeyValPair>& vResult)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Salvage(strFile,fAggressive,vResult);    
    }
    
    return true;    
}


void CDBWrapEnv::CheckpointLSN(const std::string& strFile)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->CheckpointLSN(strFile);    
    }
}

int CDBWrapEnv::RenameDb(const std::string& strOldFileName,const std::string& strNewFileName)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->RenameDb(strOldFileName,strNewFileName);    
    }    
    
    return 0;
}

bool CDBWrapEnv::Recover(std::string strFile, std::vector<CDBConstEnv::KeyValPair>& SalvagedData)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Recover(strFile,SalvagedData);    
    }    
    return true;
}

CDBWrap::CDBWrap(const std::string& strFilename, const char* pszMode)
{
    m_lpDb=NULL;
    if( (mc_gState->m_WalletMode & MC_WMD_FLAT_DAT_FILE) == 0 )
    {
        m_lpDb=new CDB(strFilename,pszMode);
    }        
}

CDBWrap::~CDBWrap() 
{ 
    if(m_lpDb)
    {
        delete (CDB*)m_lpDb;        
    }
}


void CDBWrap::Flush()
{
    if(m_lpDb)
    {
        ((CDB*)m_lpDb)->Flush();
    }    
}

void CDBWrap::Close()
{
    if(m_lpDb)
    {
        ((CDB*)m_lpDb)->Close();
    }    
}

void CDBWrapEnv::CloseDb(const string& strFile)
{
    if(m_lpDBEnv)
    {
        ((CDBEnv*)m_lpDBEnv)->CloseDb(strFile);
    }    
}

bool CDBWrapEnv::RemoveDb(const string& strFile)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->RemoveDb(strFile);
    }    
    
    return true;
}


void CDBWrapEnv::Flush(bool fShutdown)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Flush(fShutdown);
    }    
}

bool CDBWrap::Read(CDataStream& key, CDataStream& value)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->Read(key,value);
    }    
    return true;
}

bool CDBWrap::Write(CDataStream& key, CDataStream& value, bool fOverwrite)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->Write(key,value,fOverwrite);
    }        
    return true;
}

bool CDBWrap::Erase(CDataStream& key)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->Erase(key);
    }            
    return true;
}

bool CDBWrap::Exists(CDataStream& key)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->Exists(key);
    }            
    return true;
}

//    Dbc* GetCursor()
void* CDBWrap::GetCursor()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->GetCursor();
    }            
    return NULL;    
}

void CDBWrap::CloseCursor(void* cursor)
{
    if(m_lpDb)
    {
        ((CDB*)m_lpDb)->CloseCursor((Dbc*)cursor);
    }            
}

//    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags = DB_NEXT)
int CDBWrap::ReadAtCursor(void* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->ReadAtCursor((Dbc*)pcursor,ssKey,ssValue,fFlags);
    }            
    return 0;        
}


bool CDBWrap::TxnBegin()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->TxnBegin();
    }            
    return true;    
}

bool CDBWrap::TxnCommit()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->TxnCommit();
    }            
    return true;    
}

bool CDBWrap::TxnAbort()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->TxnAbort();
    }            
    return true;    
}

bool CDBWrap::ReadVersion(int& nVersion)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->ReadVersion(nVersion);
    }            
    return true;    
}


bool CDBWrap::WriteVersion(int nVersion)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->WriteVersion(nVersion);
    }            
    return true;    
}


bool RewriteWalletDB(const std::string& strFile, const char* pszSkip)
{
    if( (mc_gState->m_WalletMode & MC_WMD_FLAT_DAT_FILE) == 0 )
    {
        return CDB::Rewrite(strFile,pszSkip);
    }            
    return true;    
}


