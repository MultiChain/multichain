// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
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
    
    m_Env.Open(pathIn);
    return true;
}

CDBConstEnv::VerifyResult CDBWrapEnv::Verify(std::string strFile)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Verify(strFile);    
    }
    
    return m_Env.Verify(strFile);
}

void CDBWrapEnv::SetSeekDBName(std::string strFile)
{
    if(m_lpDBEnv == NULL)
    {
        m_Env.m_FileName=strFile;
    }
}

bool CDBWrapEnv::Salvage(std::string strFile, bool fAggressive, std::vector<CDBConstEnv::KeyValPair>& vResult)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Salvage(strFile,fAggressive,vResult);    
    }
    
    return m_Env.Salvage(strFile,fAggressive,vResult);    
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
    
    return m_Env.RenameDb(strOldFileName,strNewFileName);
}

bool CDBWrapEnv::Recover(std::string strFile, std::vector<CDBConstEnv::KeyValPair>& SalvagedData)
{
    if(m_lpDBEnv)
    {
        return ((CDBEnv*)m_lpDBEnv)->Recover(strFile,SalvagedData);    
    }    
    return m_Env.Recover(strFile,SalvagedData);
}

CDBWrap::CDBWrap(const std::string& strFilename, const char* pszMode)
{
    m_lpDb=NULL;
    m_lpDbFlat=NULL;
    if( (mc_gState->m_WalletMode & MC_WMD_FLAT_DAT_FILE) == 0 )
    {
        m_lpDb=new CDB(strFilename,pszMode);
        return;
    }        
    m_lpDbFlat=new CDBFlat(&bitdbwrap.m_Env,strFilename,pszMode);
}

CDBWrap::~CDBWrap() 
{ 
    if(m_lpDb)
    {
        delete (CDB*)m_lpDb;        
    }
    if(m_lpDbFlat)
    {
        delete m_lpDbFlat;        
    }
}


void CDBWrap::Flush()
{
    if(m_lpDb)
    {
        ((CDB*)m_lpDb)->Flush();
    }    
    m_lpDbFlat->Flush();
}

void CDBWrap::Close()
{
    if(m_lpDb)
    {
        ((CDB*)m_lpDb)->Close();
        return;
    }    
    m_lpDbFlat->Close();
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
    
    return m_Env.RemoveDb(strFile);
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
    return m_lpDbFlat->Read(key,value);
}

bool CDBWrap::Write(CDataStream& key, CDataStream& value, bool fOverwrite)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->Write(key,value,fOverwrite);
    }        
    return m_lpDbFlat->Write(key,value,fOverwrite);
}

bool CDBWrap::Erase(CDataStream& key)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->Erase(key);
    }            
    return m_lpDbFlat->Erase(key);
}

bool CDBWrap::Exists(CDataStream& key)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->Exists(key);
    }            
    return m_lpDbFlat->Exists(key);
}

//    Dbc* GetCursor()
void* CDBWrap::GetCursor()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->GetCursor();
    }            
    return m_lpDbFlat->GetCursor();    
}

void CDBWrap::CloseCursor(void* cursor)
{
    if(m_lpDb)
    {
        ((CDB*)m_lpDb)->CloseCursor((Dbc*)cursor);
        return;
    }            
    return m_lpDbFlat->CloseCursor(cursor);    
}

//    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags = DB_NEXT)
int CDBWrap::ReadAtCursor(void* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->ReadAtCursor((Dbc*)pcursor,ssKey,ssValue,fFlags);
    }            
    return m_lpDbFlat->ReadAtCursor((Dbc*)pcursor,ssKey,ssValue,fFlags);        
}


bool CDBWrap::TxnBegin()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->TxnBegin();
    }            
    return m_lpDbFlat->TxnBegin();    
}

bool CDBWrap::TxnCommit()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->TxnCommit();
    }            
    return m_lpDbFlat->TxnCommit();    
}

bool CDBWrap::TxnAbort()
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->TxnAbort();
    }            
    return m_lpDbFlat->TxnAbort();    
}

bool CDBWrap::ReadVersion(int& nVersion)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->ReadVersion(nVersion);
    }            
    return m_lpDbFlat->ReadVersion(nVersion);
}


bool CDBWrap::WriteVersion(int nVersion)
{
    if(m_lpDb)
    {
        return ((CDB*)m_lpDb)->WriteVersion(nVersion);
    }            
    return m_lpDbFlat->WriteVersion(nVersion);    
}


bool RewriteWalletDB(const std::string& strFile, const char* pszSkip)
{
    if( (mc_gState->m_WalletMode & MC_WMD_FLAT_DAT_FILE) == 0 )
    {
        return CDB::Rewrite(strFile,pszSkip);
    }            
    return CDBFlat::Rewrite(&bitdbwrap.m_Env,strFile,pszSkip);    
}


