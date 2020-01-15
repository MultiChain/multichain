// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "wallet/dbflat.h"

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/version.hpp>


using namespace std;
using namespace boost;

bool fDBFlatDebug=false;
bool fDBFlatDebugKey=false;

bool mc_CopyFile(boost::filesystem::path& pathDBOld,boost::filesystem::path& pathDBNew);
void PrintDBFlatPos(const char *msg,const mc_DBFlatPos *pos)
{
    if(fDBFlatDebug)
    {
        printf("%s: (%u,%u) at offset %u (%08x), flags: %08x\n",msg,pos->m_KeyLen,pos->m_ValLen,pos->m_Offset,pos->m_Offset,pos->m_Flags);        
    }
}

void PrintDataStreamKey(const char *msg,const CDataStream& ss)
{
    if(fDBFlatDebugKey)
    {
        CDataStream ss_copy(ss);
        string strType;
        ss_copy >> strType;
        printf("%s: %s",msg,strType.c_str());
        if(ss_copy.size())
        {
            printf(" + %lu bytes: ",ss_copy.size());
            if(ss_copy.size() == 4)
            {
                uint32_t v;
                ss_copy >> v;
                printf(" %u ",v);
            }
            if(ss_copy.size() == 8)
            {
                uint64_t v;
                ss_copy >> v;
                printf(" %lu ",v);
            }
        }
        printf("\n");
    }
}

void mc_DBFlatPos::Zero()
{
    memset(this,0,sizeof(mc_DBFlatPos));
}

uint32_t mc_DBFlatPos::NextOffset()
{
    int64_t lres;
    lres=ValueOffset()+(int64_t)m_ValLen;
    
    if(lres>0xffffffff)
    {
        return 0xffffffff;
    }
    
    return (uint32_t)lres;
}

uint32_t mc_DBFlatPos::ValueOffset()
{
    int64_t lres;
    uint32_t res;
    lres=m_Offset;
    lres+=MC_DBF_FLAGS_FIELDSIZE+2;
    lres+=m_KeyLen;
    if(m_Flags & MC_DBF_FLAGS_SIZE_KEY_LOW)lres+=1;
    if(m_Flags & MC_DBF_FLAGS_SIZE_KEY_HIGH)lres+=2;
    if(m_Flags & MC_DBF_FLAGS_SIZE_VALUE_LOW)lres+=1;
    if(m_Flags & MC_DBF_FLAGS_SIZE_VALUE_HIGH)lres+=2;
    
    res=0xffffffff;
    if(lres<=0xffffffff)
    {
        res=(uint32_t)lres;
    }
    return res;
}

int mc_DBFlatPos::SetSizeFlags(size_t size,bool is_key)
{
    int bits=0;
    
    if(size>0xff)
    {
        if(size>0xffff)
        {
            if(size>0xffffff)
            {
                bits=3;
            }        
            else
            {
                bits=2;
            }
        }        
        else
        {
            bits=1;
        }
    }
    if(is_key)
    {
        m_KeyLen=size;
        if(bits & 1)m_Flags |= MC_DBF_FLAGS_SIZE_KEY_LOW;
        if(bits & 2)m_Flags |= MC_DBF_FLAGS_SIZE_KEY_HIGH;
    }
    else
    {
        m_ValLen=size;
        if(bits & 1)m_Flags |= MC_DBF_FLAGS_SIZE_VALUE_LOW;
        if(bits & 2)m_Flags |= MC_DBF_FLAGS_SIZE_VALUE_HIGH;        
    }
    
    
    return bits+1;
}

int GetWalletDatVersion(const std::string& strWalletFile)
{
    unsigned char buf[49];
    unsigned char required[49];
    
    memset(required,0,49);
    required[4]=0x0d;
    required[5]=0x04;
    required[6]=0x0c;
    strcpy((char*)required+7,"walletdbsize");
    required[27]=0x10;
    required[28]=0x04;
    required[29]=0x0f;
    strcpy((char*)required+30,"walletdbversion");
    
    uint32_t flags;
    flags=_O_BINARY;
    flags |= O_RDONLY;
    
    int han=open(strWalletFile.c_str(),flags, S_IRUSR | S_IWUSR);
    
    if(han<=0)
    {
        return 2;
    }
    
    size_t size=lseek64(han,0,SEEK_END);
   
    lseek64(han,0,SEEK_SET);
    
    if( (size<49) ||
        (read(han,buf,49) != 49) || 
        (memcmp(required,buf,19) != 0) ||
        (memcmp(required+23,buf+23,22) != 0 ) )
    {
        close(han);
        return 2;        
    }
    
    close(han);    
    return mc_GetLE(buf+45,4);
}

std::vector<unsigned char>DST2VUC(CDataStream ss)
{
    std::vector<unsigned char> result;
    result.resize(ss.size());
    memcpy(&result[0],&ss[0],ss.size());
    return result;
}

CDBConstEnv::VerifyResult CDBFlatEnv::Verify(std::string strFile,std::vector<CDBConstEnv::KeyValPair>* lpvResultOut)
{
    CDBFlat dbsrc;
    void *cursor;
    int ret;
    mc_DBFlatPos pos;  
    
    if(dbsrc.Open(this,strFile,"r"))
    {
        LogPrintf("CDBFlatEnv::Verify : Cannot open database.\n");
        return CDBConstEnv::RECOVER_FAIL;
    }
    
    cursor=dbsrc.GetCursor();
    if(cursor == NULL)
    {
        return CDBConstEnv::RECOVER_FAIL;
    }

    if(lpvResultOut)
    {
        lpvResultOut->clear();
    }
    
    m_FileRows.clear();
    m_FileName=strFile;
    
    ret=0;
    while (ret == 0)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        
        ret = dbsrc.ReadAtCursor(cursor, ssKey, ssValue, MC_DBW_CODE_DB_NEXT, &pos);

        if(ret == 0)
        {
            std::map<vector<unsigned char>,mc_DBFlatPos>::iterator it = m_FileRows.find(DST2VUC(ssKey));
            if(it != m_FileRows.end())
            {
                PrintDBFlatPos("Verify update old",&(it->second));
                PrintDBFlatPos("Verify update new",&pos);
                memcpy(&(it->second),&pos, sizeof(mc_DBFlatPos));
            }
            else
            {
                PrintDBFlatPos("Verify insert    ",&pos);
                m_FileRows.insert(make_pair(DST2VUC(ssKey), pos));
            }
            if(lpvResultOut)
            {
                lpvResultOut->push_back(make_pair(DST2VUC(ssKey), DST2VUC(ssValue)));
            }
        }
    }
    
    if(ret == MC_DBW_CODE_DB_NOTFOUND)                     
    {
        ret=0;       
    }
    
    dbsrc.CloseCursor(cursor);
    dbsrc.Close();
    
    if(ret)
    {
        LogPrintf("CDBFlatEnv::Verify : Verification failed.\n");
        return CDBConstEnv::RECOVER_FAIL;
    }
    
    return CDBConstEnv::VERIFY_OK;
}

bool CDBFlatEnv::Salvage(std::string strFile, bool fAggressive, std::vector<CDBConstEnv::KeyValPair>& vResult)
{
    if(Verify(strFile,&vResult) == CDBConstEnv::VERIFY_OK)
    {
        return true;
    }
    
    LogPrintf("CDBFlatEnv::Salvage : Database salvage found errors, all data may not be recoverable.\n");
    if (!fAggressive) 
    {
        LogPrintf("CDBFlatEnv::Salvage : Rerun with aggressive mode to ignore errors and continue.\n");
        return false;
    }
    
    vResult.clear();
    
    CDBFlat dbsrc;
    void *cursor;
    int ret;
    uint32_t last_offset;
    uint32_t last_recovery_offset=0;
    bool in_recovery=false;

    if(dbsrc.Open(this,strFile,"r"))
    {
        return false;
    }
    
    cursor=dbsrc.GetCursor();
    if(cursor == NULL)
    {
        return false;
    }

    ret=0;
    while (ret == 0)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);        
        
        last_offset=((mc_DBFlatPos*)cursor)->m_Offset;
        if(in_recovery)
        {
            dbsrc.SetFileOffset(last_offset);            
        }
        ret = dbsrc.ReadAtCursor(cursor, ssKey, ssValue, MC_DBW_CODE_DB_NEXT);

        if(ret == 0)
        {
            vResult.push_back(make_pair(DST2VUC(ssKey), DST2VUC(ssValue)));
            if(in_recovery)
            {
                last_recovery_offset=last_offset;
            }
            else
            {
                last_recovery_offset=0;                
            }
            in_recovery=false;
        }
        else
        {
            if( (ret != MC_DBW_CODE_DB_NOTFOUND) && (last_offset > 0) )
            {
                if(last_recovery_offset)
                {
                    vResult.pop_back();
                    ((mc_DBFlatPos*)cursor)->m_Offset=last_recovery_offset+1;
                    last_recovery_offset=0;
                }
                else
                {
                    ((mc_DBFlatPos*)cursor)->m_Offset=last_offset+1;                    
                }
                in_recovery=true;
                ret=0;
            }
        }
    }
    
    if(ret == MC_DBW_CODE_DB_NOTFOUND)                      
    {
        ret=0;       
    }
    
    dbsrc.CloseCursor(cursor);
    dbsrc.Close();
    
    if(ret)
    {
        return CDBConstEnv::RECOVER_FAIL;
    }
    
    return true;
}

bool CDBFlatEnv::Open(const boost::filesystem::path& path)
{
    m_DirPath=path;
    return true;
}

void CDBFlatEnv::Close()
{
    
}

bool CDBFlatEnv::RemoveDb(const std::string& strFile)
{
    boost::filesystem::path pathDB = m_DirPath / strFile;
    
    if(__US_DeleteFile(pathDB.string().c_str()))
    {
        return false;
    }
    
    return true;
}

bool CDBFlatEnv::CopyDb(const std::string& strOldFileName,const std::string& strNewFileName)
{
    boost::filesystem::path pathDBOld = m_DirPath / strOldFileName;
    boost::filesystem::path pathDBNew = m_DirPath / strNewFileName;

    return mc_CopyFile(pathDBOld,pathDBNew);
/*    
    try {
#if BOOST_VERSION >= 104000
                    filesystem::copy_file(pathDBOld, pathDBNew, filesystem::copy_option::overwrite_if_exists);
#else
                    filesystem::copy_file(pathSrc, pathDest);
#endif
    } catch(const filesystem::filesystem_error &e) {
        LogPrintf("error copying %s to %s - %s\n", pathDBOld.string(), pathDBNew.string(), e.what());
        return false;
    } 
    return true;
 */ 
}

int CDBFlatEnv::RenameDb(const std::string& strOldFileName,const std::string& strNewFileName)
{
    if(!CopyDb(strOldFileName,strNewFileName))
    {
        return -1;
    }
    RemoveDb(strOldFileName);
    return 0;
}

bool CDBFlatEnv::Recover(std::string strFile, std::vector<CDBConstEnv::KeyValPair>& SalvagedData)
{
    CDBFlat dbdst;
    int ret=0;
    
    string strFileCopy = strFile + ".recover";
   
    RemoveDb(strFileCopy);
    
    if(dbdst.Open(this,strFileCopy,"r+"))
    {
        return false;
    }

    for(int i=0;i<(int)SalvagedData.size();i++)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        ssKey.SetType(SER_DISK);
        ssKey.clear();        
        ssKey.write((char*)&(SalvagedData[i].first[0]),SalvagedData[i].first.size());
        
        ssValue.SetType(SER_DISK);
        ssValue.clear();        
        ssValue.write((char*)&(SalvagedData[i].second[0]),SalvagedData[i].second.size());
        
        bool fWalleDBSizeOrVersion=false;
        if(ssKey.size() == 13)
        {
            if( (ssKey[0]==12) && (memcmp(&ssKey[1],"walletdbsize",12) == 0) )
            {
                fWalleDBSizeOrVersion=true;
            }
        }
        if(ssKey.size() == 16)
        {
            if( (ssKey[0]==15) && (memcmp(&ssKey[1],"walletdbversion",12) == 0) )
            {
                fWalleDBSizeOrVersion=true;
            }
        }
        
        if (!fWalleDBSizeOrVersion)
        {
            if(!dbdst.Write(ssKey,ssValue,false))
            {
               ret=MC_DBW_CODE_DB_NOSERVER;
            }
        }        
    }
    
    dbdst.Close();
    
    if(ret == MC_DBW_CODE_DB_NOTFOUND)                     
    {
        ret=0;       
    }
    
    if(ret)
    {
        return false;
    }

    string strFileBackup = strFile + ".copy";
    
    if (boost::filesystem::exists(this->m_DirPath / strFile))
    {
        if(!CopyDb(strFile,strFileBackup))
        {
            return false;                
        }
    }
    if(!CopyDb(strFileCopy,strFile))
    {
        CopyDb(strFileBackup,strFile);
        return false;        
    }
    
    if(m_FileName == strFile)
    {
        if(Verify(strFile) != CDBConstEnv::VERIFY_OK)
        {
            CopyDb(strFileBackup,strFile);
            return false;
        }
    }
    
    RemoveDb(strFileCopy);
    RemoveDb(strFileBackup);
    
    return true;
}

CDBFlat::CDBFlat(CDBFlatEnv *lpEnv, const std::string& strFilename, const char* pszMode)
{
    Open(lpEnv,strFilename,pszMode);
}

void CDBFlat::Zero()
{
    m_lpEnv=NULL;    
    m_FileName.clear();
    m_OpenMode.clear();
    m_CanSeek=false;
    m_CanWrite=false;
    m_FileHan=0;
    m_FileSize=0;       
}

int CDBFlat::Open(CDBFlatEnv *lpEnv, const std::string& strFilename, const char* pszMode)
{
    if(fDBFlatDebug)printf("Open\n");
    m_lpEnv=lpEnv;
    m_FileHan=0;
    m_FileSize=0;
    m_CanSeek=false;
    m_CanWrite=!((!strchr(pszMode, '+') && !strchr(pszMode, 'w')));
    m_FileName=strFilename;
    m_OpenMode=strprintf("%s",pszMode);
        
    boost::filesystem::path pathDBFile = lpEnv->m_DirPath / strFilename;

    uint32_t flags;
    flags=_O_BINARY;
    if(m_CanWrite)
    {
        flags |= O_RDWR | O_CREAT;
    }
    else
    {
        flags |= O_RDONLY;
    }
    
    m_FileHan=open(pathDBFile.string().c_str(),flags, S_IRUSR | S_IWUSR);
    
    if(m_FileHan<=0)
    {
        if(fDBFlatDebug)printf("Cannot Open\n");
        m_FileHan=0;
        return MC_ERR_DBOPEN_ERROR;
    }
    
    m_FileSize=GetFileSize();

    if(m_FileSize == 0)
    {        
        if(!m_CanWrite)
        {
            close(m_FileHan);
            m_FileHan=0;
            return MC_ERR_CORRUPTED;            
        }
        
        uint32_t version=3;
        
        Lock(false);
        m_FileSize=23;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssKey << string("walletdbsize");
        ssValue << m_FileSize;
        Write(ssKey,ssValue);
        UnLock();
        
        m_FileSize=GetFileSize();
                
        ssKey.clear();
        ssValue.clear();        
        ssKey << string("walletdbversion");
        ssValue << version;
        Write(ssKey,ssValue);        
    }
    else
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        
        mc_DBFlatPos pos;
        pos.Zero();
        lseek64(m_FileHan,0,SEEK_SET);
        
        m_FileSize=GetFileSize(false);
    }
    
    if(m_lpEnv->m_FileName == m_FileName)
    {
        m_CanSeek=true;
    }
    
    return MC_ERR_NOERROR;
}

void CDBFlat::SetFileOffset(uint32_t offset)
{    
//    printf("File Offset set to: %u\n",offset);
    lseek64(m_FileHan,offset,SEEK_SET);    
}

uint32_t CDBFlat::GetFileSize(bool real)
{
    uint32_t file_size;
    if(real)
    {
        file_size=lseek64(m_FileHan,0,SEEK_END);
//        printf("Real File Size: %u\n",file_size);
        return file_size;
    }
    
    SetFileOffset(0);
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    mc_DBFlatPos pos;
    pos.Zero();
    
    m_FileSize=MC_DBF_FLAGS_FIELDSIZE+2+13+sizeof(uint32_t);
    if(ReadAtCursor(&pos,ssKey,ssValue) == 0)
    {
        file_size=0;
        if(ssValue.size() == sizeof(uint32_t))
        {
            ssValue >> file_size;
        }
    }
    else
    {
        file_size=0;
    }
    
//    printf("Stored File Size: %u\n",file_size);
    return file_size;
}


void CDBFlat::Flush()
{
    
}

void CDBFlat::Close()
{
    if(m_FileHan > 0)
    {
        if(fDBFlatDebug)printf("Close\n");
        close(m_FileHan);
    }
}

bool CDBFlat::Read(CDataStream& ssKey, CDataStream& ssValue)
{
    if(m_FileHan <= 0)
    {
        return false;
    }
    if(!m_CanSeek)
    {
        return false;
    }
    
    PrintDataStreamKey("Read",ssKey);
    
    std::map<vector<unsigned char>,mc_DBFlatPos>::iterator it = m_lpEnv->m_FileRows.find(DST2VUC(ssKey));
    if(it == m_lpEnv->m_FileRows.end())
    {
        PrintDataStreamKey("Not found",ssKey);
        return false;
    }
    if(it->second.NextOffset() > m_FileSize)
    {
        return false;        
    }
    
    Lock();
    SetFileOffset(it->second.ValueOffset());
    vector <char> vv;
    vv.reserve(it->second.m_ValLen);
    if(read(m_FileHan,&vv[0],it->second.m_ValLen) != it->second.m_ValLen)
    {
        UnLock();
        return MC_DBW_CODE_DB_NOSERVER;  
    }
    UnLock();
    ssValue.SetType(SER_DISK);
    ssValue.clear();
    ssValue.write(&vv[0],it->second.m_ValLen);                            
    PrintDataStreamKey("Found",ssKey);
    return true;
}

void CDBFlat::Lock(bool for_size)
{
    if(m_FileHan)
    {
        __US_LockFile(m_FileHan);
        
        if(!for_size)
        {
            m_FileSize=GetFileSize(false);
        }
    }    
}

void CDBFlat::UnLock()
{
    if(m_FileHan)
    {
        __US_UnLockFile(m_FileHan);
    }
}


bool CDBFlat::Write(CDataStream& ssKey, CDataStream& ssValue, bool fOverwrite)
{
    mc_DBFlatPos pos;
    mc_DBFlatPos erase_pos;
    uint32_t key_size_bytes;
    uint32_t val_size_bytes;
    
    if(m_FileHan <= 0)
    {
        return false;
    }
    if(!m_CanWrite)
    {
        return false;        
    }
        
    pos.m_Flags=MC_DBF_FLAGS_EMPTY;
    pos.m_Offset=m_FileSize;
    
    bool fWalleDBSize=false;
    if(ssKey.size() == 13)
    {
        if( (ssKey[0]==12) && (memcmp(&ssKey[1],"walletdbsize",12) == 0) )
        {
            fWalleDBSize=true;
        }
    }
    if(fWalleDBSize)
    {
        pos.m_Offset=0;
        pos.m_KeyLen=ssKey.size();
        pos.m_ValLen=sizeof(uint32_t);
        if(ssValue.size() != pos.m_ValLen)
        {
            return false;
        }
    
        SetFileOffset(pos.m_Offset);
        
        write(m_FileHan,&pos.m_Flags,MC_DBF_FLAGS_FIELDSIZE);
        write(m_FileHan,&pos.m_KeyLen,1);
        write(m_FileHan,&pos.m_ValLen,1);
        write(m_FileHan,&ssKey[0],pos.m_KeyLen);
        write(m_FileHan,&ssValue[0],pos.m_ValLen);
        if(GetFileSize() < pos.NextOffset())
        {
            LogPrintf("CDBFlat::Write : Corrupted - wrong file size mark.\n");            
            return false;
        }               
        
        return true;
    }
    else
    {
        PrintDataStreamKey("Write",ssKey);
        if(m_CanSeek)
        {
            std::map<vector<unsigned char>,mc_DBFlatPos>::const_iterator it = m_lpEnv->m_FileRows.find(DST2VUC(ssKey));
            if(it != m_lpEnv->m_FileRows.end())
            {
                PrintDBFlatPos("Write erase      ",&(it->second));
                if(!fOverwrite)
                {
                    return false;
                }
                memcpy(&erase_pos,&(it->second),sizeof(mc_DBFlatPos));
            }
        }        
    }
    
    key_size_bytes=pos.SetSizeFlags(ssKey.size(),true);
    val_size_bytes=pos.SetSizeFlags(ssValue.size(),false);

    Lock();
    pos.m_Offset=m_FileSize;
    SetFileOffset(pos.m_Offset);
    write(m_FileHan,&pos.m_Flags,MC_DBF_FLAGS_FIELDSIZE);
    write(m_FileHan,&pos.m_KeyLen,key_size_bytes);
    write(m_FileHan,&pos.m_ValLen,val_size_bytes);
    if(pos.m_KeyLen)write(m_FileHan,&ssKey[0],pos.m_KeyLen);
    if(pos.m_ValLen)write(m_FileHan,&ssValue[0],pos.m_ValLen);
    
    if(GetFileSize() < pos.NextOffset())
    {
        LogPrintf("CDBFlat::Write : Corrupted - failed write - required: %u, file size: %u.\n",pos.NextOffset(),GetFileSize());            
        UnLock();
        return false;
    }        

    m_FileSize=pos.NextOffset();
    if(fDBFlatDebug)printf("New file size: %d\n",m_FileSize);
    CDataStream ssFileSizeKey(SER_DISK, CLIENT_VERSION);
    CDataStream ssFileSizeVal(SER_DISK, CLIENT_VERSION);
    ssFileSizeKey << string("walletdbsize");
    ssFileSizeVal << m_FileSize;
    
    if(!Write(ssFileSizeKey,ssFileSizeVal))
    {
        UnLock();
        return false;        
    }
    
    __US_FlushFile(m_FileHan);        
    
    if(erase_pos.m_Offset)
    {
        erase_pos.m_Flags |= MC_DBF_FLAGS_DELETED;
        SetFileOffset(erase_pos.m_Offset);
        if(write(m_FileHan,&erase_pos.m_Flags,MC_DBF_FLAGS_FIELDSIZE) != MC_DBF_FLAGS_FIELDSIZE)
        {
            UnLock();
            return false;
        }                
        std::map<vector<unsigned char>,mc_DBFlatPos>::iterator it = m_lpEnv->m_FileRows.find(DST2VUC(ssKey));
        if(it != m_lpEnv->m_FileRows.end())
        {
            memcpy(&(it->second),&pos, sizeof(mc_DBFlatPos));
        }
    }
    else
    {
        if(m_CanSeek)
        {
            PrintDBFlatPos("Write insert     ",&pos);
            PrintDataStreamKey("Write insert     ",ssKey);
            m_lpEnv->m_FileRows.insert(make_pair(DST2VUC(ssKey), pos));
        }
    }
    
    __US_FlushFile(m_FileHan);        
    UnLock();
    
    
    return true;
}

bool CDBFlat::Erase(CDataStream& ssKey)
{
    if(m_FileHan <= 0)
    {
        return false;
    }
    if(!m_CanWrite)
    {
        return false;        
    }
    if(!m_CanSeek)
    {
        return false;                
    }
    
    PrintDataStreamKey("Erase",ssKey);
    
    std::map<vector<unsigned char>,mc_DBFlatPos>::iterator it = m_lpEnv->m_FileRows.find(DST2VUC(ssKey));
    if(it != m_lpEnv->m_FileRows.end())
    {
        it->second.m_Flags |= MC_DBF_FLAGS_DELETED;
        Lock();
        SetFileOffset(it->second.m_Offset);
        if(write(m_FileHan,&it->second.m_Flags,MC_DBF_FLAGS_FIELDSIZE) != MC_DBF_FLAGS_FIELDSIZE)
        {
            UnLock();
            return false;
        }                
        m_lpEnv->m_FileRows.erase(it);
        __US_FlushFile(m_FileHan);        
        UnLock();
    }
    
        
    return true;
}

bool CDBFlat::Exists(CDataStream& ssKey)
{
    if(m_FileHan <= 0)
    {
        return false;
    }
    if(!m_CanWrite)
    {
        return false;        
    }
    if(!m_CanSeek)
    {
        return false;                
    }
    
    PrintDataStreamKey("Exist",ssKey);
    std::map<vector<unsigned char>,mc_DBFlatPos>::const_iterator it = m_lpEnv->m_FileRows.find(DST2VUC(ssKey));
    if(it != m_lpEnv->m_FileRows.end())
    {
        return true;
    }
    
    return false;
}
    
void* CDBFlat::GetCursor()
{
    if(m_FileHan <= 0)        
    {
        return NULL;
    }
    Lock();
    SetFileOffset(0);
    return new mc_DBFlatPos;
}

void CDBFlat::CloseCursor(void* cursor)
{
    if(cursor)
    {
        UnLock();
        delete (mc_DBFlatPos*)cursor;
    }
}

int CDBFlat::ReadAtCursor(void* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags,mc_DBFlatPos *lpPosRes)
{
    mc_DBFlatPos *lpPos;
    uint32_t key_size_bytes;
    uint32_t val_size_bytes;
    if(pcursor)
    {        
        while(true)        
        {
            lpPos=(mc_DBFlatPos*)pcursor;
            if(lpPos->m_Offset)
            {
                PrintDBFlatPos("Read At Cursor",lpPos);
            }
            if(lpPos->m_Offset+MC_DBF_FLAGS_FIELDSIZE >= m_FileSize)
            {
                if(lpPos->m_Offset == m_FileSize)
                {
                    return MC_DBW_CODE_DB_NOTFOUND;
                }
                return MC_DBW_CODE_DB_NOSERVER;                                             
            }
            if(read(m_FileHan,&(lpPos->m_Flags),MC_DBF_FLAGS_FIELDSIZE) != MC_DBF_FLAGS_FIELDSIZE)
            {
                return MC_DBW_CODE_DB_NOSERVER;        
            }
            key_size_bytes=1;
            if(lpPos->m_Flags & MC_DBF_FLAGS_SIZE_KEY_LOW)key_size_bytes+=1;
            if(lpPos->m_Flags & MC_DBF_FLAGS_SIZE_KEY_HIGH)key_size_bytes+=2;
            val_size_bytes=1;
            if(lpPos->m_Flags & MC_DBF_FLAGS_SIZE_VALUE_LOW)val_size_bytes+=1;
            if(lpPos->m_Flags & MC_DBF_FLAGS_SIZE_VALUE_HIGH)val_size_bytes+=2;
            if(lpPos->m_Offset+key_size_bytes+val_size_bytes > m_FileSize)
            {
                return MC_DBW_CODE_DB_NOSERVER;  
            }            
            lpPos->m_KeyLen=0;
            lpPos->m_ValLen=0;
            if(read(m_FileHan,&(lpPos->m_KeyLen),key_size_bytes) != key_size_bytes)
            {
                return MC_DBW_CODE_DB_NOSERVER;  
            }
            if(read(m_FileHan,&(lpPos->m_ValLen),val_size_bytes) != val_size_bytes)
            {
                return MC_DBW_CODE_DB_NOSERVER;  
            }            
            if(lpPos->NextOffset() > m_FileSize)
            {
                return MC_DBW_CODE_DB_NOSERVER;  
            }            
            if( (lpPos->m_Flags & MC_DBF_FLAGS_DELETED) == 0)
            {
                ssKey.clear();
                ssValue.clear();
                if(lpPos->m_KeyLen)
                {
                    vector <char> vv;
                    vv.reserve(lpPos->m_KeyLen);
                    if(read(m_FileHan,&vv[0],lpPos->m_KeyLen) != lpPos->m_KeyLen)
                    {
                        return MC_DBW_CODE_DB_NOSERVER;  
                    }
                    ssKey.SetType(SER_DISK);
                    ssKey.clear();
                    ssKey.write(&vv[0],lpPos->m_KeyLen);                            
                }
                if(lpPos->m_ValLen)
                {
                    vector <char> vv;
                    vv.reserve(lpPos->m_ValLen);
                    if(read(m_FileHan,&vv[0],lpPos->m_ValLen) != lpPos->m_ValLen)
                    {
                        return MC_DBW_CODE_DB_NOSERVER;  
                    }
                    ssValue.SetType(SER_DISK);
                    ssValue.clear();
                    ssValue.write(&vv[0],lpPos->m_ValLen);                            
                }
                if(lpPosRes)
                {
                    memcpy(lpPosRes,lpPos,sizeof(mc_DBFlatPos));
                }
                if(lpPos->m_Offset)
                {
                    PrintDBFlatPos("Successful read",lpPos);
                    PrintDataStreamKey("Key",ssKey);
                }
                lpPos->m_Offset=lpPos->NextOffset();
                return 0;
            }
            else
            {
                lpPos->m_Offset=lpPos->NextOffset();          
                SetFileOffset(lpPos->m_Offset);
            }
        }        
    }
    
    return MC_DBW_CODE_DB_NOSERVER;  
}


bool CDBFlat::TxnBegin()
{
    string strFileCopy = m_FileName + ".txncopy";
    boost::filesystem::path pathDBFile = m_lpEnv->m_DirPath / m_FileName;
    boost::filesystem::path pathDBCopy = m_lpEnv->m_DirPath / strFileCopy;
    
    return m_lpEnv->CopyDb(m_FileName,strFileCopy);
}

bool CDBFlat::TxnCommit()
{
    string strFileCopy = m_FileName + ".txncopy";
    boost::filesystem::path pathDBCopy = m_lpEnv->m_DirPath / strFileCopy;
    
    __US_DeleteFile(pathDBCopy.string().c_str());
    
    return true;
}

bool CDBFlat::TxnAbort()
{
    bool ret;
    
    Close();

    string strFileCopy = m_FileName + ".txncopy";
    
    ret=m_lpEnv->CopyDb(strFileCopy,m_FileName);
        
    Open(m_lpEnv,m_FileName,m_OpenMode.c_str());
    
    if(ret)
    {
        if(m_FileHan)
        {
            __US_DeleteFile(strFileCopy.c_str());
            return true;
        }
    }
    
    return false;
}

bool CDBFlat::ReadVersion(int& nVersion)
{
    if(m_FileHan <= 0)
    {
        return false;
    }
    if(!m_CanSeek)
    {
        return false;                
    }
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    CDataStream ssVal(SER_DISK, CLIENT_VERSION);
    ssKey << string("version");
    
    if(!Read(ssKey,ssVal))
    {
        return false;                        
    }
    
    if(ssVal.size() != sizeof(int))
    {
        return false;                        
    }
    
    nVersion=mc_GetLE(&ssVal[0],sizeof(int));
    
    return true;
}

bool CDBFlat::WriteVersion(int nVersion)
{
    if(m_FileHan <= 0)
    {
        return false;
    }
    if(!m_CanWrite)
    {
        return false;        
    }
    if(!m_CanSeek)
    {
        return false;                
    }
    
    CDataStream ssKey(SER_DISK, CLIENT_VERSION);
    CDataStream ssVal(SER_DISK, CLIENT_VERSION);
    ssKey << string("version");
    ssVal << nVersion;
    
    return Write(ssKey,ssVal);
}

bool CDBFlat::Rewrite(CDBFlatEnv *lpEnv,const string& strFile, const char* pszSkip)
{
    CDBFlat dbsrc;
    CDBFlat dbdst;
    void *cursor;
    int ret;
    
    string strFileCopy = strFile + ".rewrite";
   
    if(dbsrc.Open(lpEnv,strFile,"r"))
    {
        return false;
    }
    
    if(dbdst.Open(lpEnv,strFileCopy,"r+"))
    {
        return false;
    }
    
    cursor=dbsrc.GetCursor();
    if(cursor == NULL)
    {
        return false;        
    }

    ret=0;
    while (ret == 0)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        
        ret = dbsrc.ReadAtCursor(cursor, ssKey, ssValue);

        if(ret == 0)
        {
            bool fWalleDBSizeOrVersion=false;
            if(ssKey.size() == 13)
            {
                if( (ssKey[0]==12) && (memcmp(&ssKey[1],"walletdbsize",12) == 0) )
                {
                    fWalleDBSizeOrVersion=true;
                }
            }
            if(ssKey.size() == 16)
            {
                if( (ssKey[0]==15) && (memcmp(&ssKey[1],"walletdbversion",12) == 0) )
                {
                    fWalleDBSizeOrVersion=true;
                }
            }
        
            if (!fWalleDBSizeOrVersion)
            {
                if(!dbdst.Write(ssKey,ssValue,false))
                {
                   ret=MC_DBW_CODE_DB_NOSERVER;
                }
            }        
        }
    }
        
    dbsrc.CloseCursor(cursor);
    dbsrc.Close();
    dbdst.Close();
    
    if(ret == MC_DBW_CODE_DB_NOTFOUND)
    {
        ret=0;       
    }
    
    if(ret)
    {
        return false;
    }


    if(!lpEnv->CopyDb(strFileCopy,strFile))
    {
        return false;        
    }
    
    if(lpEnv->m_FileName == strFile)
    {
        if(lpEnv->Verify(strFile) != CDBConstEnv::VERIFY_OK)
        {
            return false;
        }
    }
    
    lpEnv->RemoveDb(strFileCopy);
    
    return true;
}