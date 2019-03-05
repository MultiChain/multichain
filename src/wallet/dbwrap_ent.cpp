// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "wallet/dbwrap.h"

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
    LogPrintf("Using walletdbversion=3\n");
}

void CDBWrapEnv::EnvShutdown()
{
    
}

CDBWrapEnv::CDBWrapEnv()
{    
    m_lpDBEnv=NULL;
    m_lpMapFileUseCount=NULL;
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
    m_Env.Open(pathIn);
    return true;
}

CDBConstEnv::VerifyResult CDBWrapEnv::Verify(std::string strFile)
{
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
    return m_Env.Salvage(strFile,fAggressive,vResult);    
}


void CDBWrapEnv::CheckpointLSN(const std::string& strFile)
{
    
}

int CDBWrapEnv::RenameDb(const std::string& strOldFileName,const std::string& strNewFileName)
{
    return m_Env.RenameDb(strOldFileName,strNewFileName);
}

bool CDBWrapEnv::Recover(std::string strFile, std::vector<CDBConstEnv::KeyValPair>& SalvagedData)
{
    return m_Env.Recover(strFile,SalvagedData);
}

CDBWrap::CDBWrap(const std::string& strFilename, const char* pszMode)
{
    m_lpDb=NULL;
    m_lpDbFlat=new CDBFlat(&bitdbwrap.m_Env,strFilename,pszMode);
}

CDBWrap::~CDBWrap() 
{ 
    if(m_lpDbFlat)
    {
        delete m_lpDbFlat;        
    }
}


void CDBWrap::Flush()
{
    m_lpDbFlat->Flush();
}

void CDBWrap::Close()
{
    m_lpDbFlat->Close();
}

void CDBWrapEnv::CloseDb(const string& strFile)
{

}

bool CDBWrapEnv::RemoveDb(const string& strFile)
{
    return m_Env.RemoveDb(strFile);
}


void CDBWrapEnv::Flush(bool fShutdown)
{

}

bool CDBWrap::Read(CDataStream& key, CDataStream& value)
{
    return m_lpDbFlat->Read(key,value);
}

bool CDBWrap::Write(CDataStream& key, CDataStream& value, bool fOverwrite)
{
    return m_lpDbFlat->Write(key,value,fOverwrite);
}

bool CDBWrap::Erase(CDataStream& key)
{
    return m_lpDbFlat->Erase(key);
}

bool CDBWrap::Exists(CDataStream& key)
{
    return m_lpDbFlat->Exists(key);
}

void* CDBWrap::GetCursor()
{
    return m_lpDbFlat->GetCursor();    
}

void CDBWrap::CloseCursor(void* cursor)
{
    return m_lpDbFlat->CloseCursor(cursor);    
}

//    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags = DB_NEXT)
int CDBWrap::ReadAtCursor(void* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags)
{
    return m_lpDbFlat->ReadAtCursor(pcursor,ssKey,ssValue,fFlags);        
}


bool CDBWrap::TxnBegin()
{
    return m_lpDbFlat->TxnBegin();    
}

bool CDBWrap::TxnCommit()
{
    return m_lpDbFlat->TxnCommit();    
}

bool CDBWrap::TxnAbort()
{
    return m_lpDbFlat->TxnAbort();    
}

bool CDBWrap::ReadVersion(int& nVersion)
{
    return m_lpDbFlat->ReadVersion(nVersion);
}

bool CDBWrap::WriteVersion(int nVersion)
{
    return m_lpDbFlat->WriteVersion(nVersion);    
}


bool RewriteWalletDB(const std::string& strFile, const char* pszSkip)
{
    return CDBFlat::Rewrite(&bitdbwrap.m_Env,strFile,pszSkip);    
}


