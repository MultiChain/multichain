// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#include "wallet/chunkdb.h"
#include "community/community.h"

#define MC_CDB_TMP_FLAG_SHOULD_COMMIT           0x00000001
#define MC_CDB_FILE_PAGE_SIZE                   0x00100000

unsigned char null_txid[MC_TDB_TXID_SIZE];

void mc_SubscriptionFileDBRow::Zero()
{
    memset(this,0,sizeof(mc_SubscriptionFileDBRow));    
    m_RecordType=MC_CDB_TYPE_FILE;
}

void mc_SubscriptionDBRow::Zero()
{
    memset(this,0,sizeof(mc_SubscriptionDBRow));    
    m_RecordType=MC_CDB_TYPE_SUBSCRIPTION;    
}

void mc_ChunkDBStat::Zero()
{
    memset(this,0,sizeof(mc_ChunkDBStat));    
    m_RecordType=MC_CDB_TYPE_DB_STAT;
    m_ChunkDBVersion=1;
}

void mc_ChunkDBRow::Zero()
{
    memset(this,0,sizeof(mc_ChunkDBRow));        
}

void mc_ChunkDBRow::SwapPosBytes()
{
    unsigned char *ptr=(unsigned char *)&m_Pos;
    unsigned char t;
    t=ptr[0];
    ptr[0]=ptr[3];
    ptr[3]=t;
    t=ptr[1];
    ptr[1]=ptr[2];
    ptr[2]=t;
}

void mc_ChunkDB::LogString(const char *message)
{
    FILE *fHan;
    
    fHan=fopen(m_LogFileName,"a");
    if(fHan == NULL)
    {
        return;
    }

    mc_LogString(fHan,message);  
    fclose(fHan);
}

void mc_ChunkDB::Zero()
{
    m_DirName[0]=0;
    m_DBName[0]=0;
    m_LogFileName[0]=0;
    m_DB=NULL;
    m_KeyOffset=0;
    m_KeySize=40;
    m_ValueOffset=40;
    m_ValueSize=40;    
    m_TotalSize=m_KeySize+m_ValueSize;
    
    m_Subscriptions=NULL;    
    m_MemPool=NULL;
    m_ChunkData=NULL;
    m_TmpScript=NULL;
    
    m_FeedPos=0;
    
    m_Semaphore=NULL;
    m_LockedBy=0;    
}

int mc_ChunkDB::Destroy()
{
    if(m_DB)
    {
        m_DB->Close();
        delete m_DB;    
        m_DB=NULL;
    }
    
    if(m_Subscriptions)
    {
        delete m_Subscriptions;
    }
    
    if(m_MemPool)
    {
        delete m_MemPool;
    }
    
    if(m_ChunkData)
    {
        delete m_ChunkData;
    }
    
    if(m_TmpScript)
    {
        delete m_TmpScript;
    }
    
    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
     
    
    Zero();
    return MC_ERR_NOERROR;       
}

int mc_ChunkDB::Lock(int write_mode,int allow_secondary)
{        
    uint64_t this_thread;
    this_thread=__US_ThreadID();
    
    if(this_thread == m_LockedBy)
    {
        if(allow_secondary == 0)
        {
            LogString("Secondary lock!!!");
        }
        return allow_secondary;
    }
    
    __US_SemWait(m_Semaphore); 
    m_LockedBy=this_thread;
    
    return 0;
}

void mc_ChunkDB::UnLock()
{    
    m_LockedBy=0;
    __US_SemPost(m_Semaphore);
}

int mc_ChunkDB::Lock()
{        
    return Lock(1,0);
}

int mc_ChunkDB::AddSubscription(mc_SubscriptionDBRow *subscription)
{
    char enthex[33];
    char dir_name[64];
    int err;
    switch(subscription->m_Entity.m_EntityType)
    {
        case MC_TET_STREAM:
            sprintf_hex(enthex,subscription->m_Entity.m_EntityID,MC_AST_SHORT_TXID_SIZE);
            sprintf(dir_name,"chunks/data/stream-%s",enthex);
            break;
        case MC_TET_AUTHOR:
            sprintf(dir_name,"chunks/data/source");
            break;
        case MC_TET_NONE:
            break;            
        default:
            return MC_ERR_NOT_SUPPORTED;
    }
    
    if(subscription->m_Entity.m_EntityType != MC_TET_NONE)
    {
        mc_GetFullFileName(m_Name,dir_name,"",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,subscription->m_DirName);
        if(subscription->m_Entity.m_EntityType == MC_TET_AUTHOR)
        {
            mc_CreateDir(subscription->m_DirName);
        }
    }
    else
    {
        subscription->m_DirName[0]=0x00;
    }

    if(subscription->m_SubscriptionID >= m_Subscriptions->GetCount())
    {
        err=m_Subscriptions->SetCount(subscription->m_SubscriptionID+1);
        if(err)
        {
            return err;
        }
    }
    
    return m_Subscriptions->PutRow(subscription->m_SubscriptionID,subscription,(char*)subscription+m_ValueOffset);    
}

mc_SubscriptionDBRow *mc_ChunkDB::FindSubscription(const mc_TxEntity* entity)
{
    int row;
    mc_SubscriptionDBRow subscription;

    if(entity == NULL)
    {
        return  (mc_SubscriptionDBRow *)m_Subscriptions->GetRow(0);
    }
    
    subscription.Zero();
    
    subscription.m_RecordType=MC_CDB_TYPE_SUBSCRIPTION;
    memcpy(&subscription.m_Entity,entity,sizeof(mc_TxEntity));
    
    row=m_Subscriptions->Seek(&subscription);
    if(row >= 0)
    {
        return  (mc_SubscriptionDBRow *)m_Subscriptions->GetRow(row);
    }
    
    return NULL;
}

int mc_ChunkDB::AddEntityInternal(mc_TxEntity* entity, uint32_t flags)
{
    int err;
    mc_SubscriptionDBRow subscription;
    char msg[256];
    char enthex[65];
    
    err=MC_ERR_NOERROR;
    
    subscription.Zero();
    
    subscription.m_RecordType=MC_CDB_TYPE_SUBSCRIPTION;
    memcpy(&subscription.m_Entity,entity,sizeof(mc_TxEntity));
    
    subscription.m_Entity.m_EntityType &= MC_TET_TYPE_MASK;
    
    if(FindSubscription(&subscription.m_Entity))
    {
        return MC_ERR_NOERROR;
    }
    
    subscription.m_Flags=flags;
    subscription.m_SubscriptionID=m_DBStat.m_LastSubscription+1;
    
    err=AddSubscription(&subscription);
    
    if(err == MC_ERR_NOERROR)
    {
        m_DBStat.m_LastSubscription+=1;

        err=m_DB->Write((char*)&subscription+m_KeyOffset,m_KeySize,(char*)&subscription+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);

        if(err == MC_ERR_NOERROR)
        {    
            err=m_DB->Write((char*)&m_DBStat+m_KeyOffset,m_KeySize,(char*)&m_DBStat+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        }

        if(err == MC_ERR_NOERROR)
        {
            err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);        
        }

        if(err)
        {
            m_DBStat.m_LastSubscription-=1;
            m_Subscriptions->SetCount(m_Subscriptions->GetCount()-1);
        }
    }
    
    sprintf_hex(enthex,entity->m_EntityID,MC_TDB_ENTITY_ID_SIZE);

    if(err)
    {
        sprintf(msg,"Could not add entity (%08X, %s), error: %d",entity->m_EntityType,enthex,err);        
        LogString(msg);            
    }
    else
    {
        sprintf(msg,"Entity (%08X, %s) added successfully",entity->m_EntityType,enthex);
        LogString(msg);
    }
    
    return err;
}

int mc_ChunkDB::AddEntity(mc_TxEntity* entity, uint32_t flags)
{
    int err;
    
    Lock();
    err=AddEntityInternal(entity,flags);
    UnLock();
    
    return err;
}

int mc_ChunkDB::SourceChunksRecovery()                          
{
    int err;
    mc_SubscriptionDBRow *subscription;
    mc_ChunkDBRow chunk_def;
    char FileName[MC_DCT_DB_MAX_PATH];    
    int FileHan;
    unsigned char* buf;
    int chunk_found=0;
    char msg[256];
    
    subscription=(mc_SubscriptionDBRow *)m_Subscriptions->GetRow(1);
    
    SetFileName(FileName,subscription,subscription->m_LastFileID);
    FileHan=open(FileName,_O_BINARY | O_RDONLY, S_IRUSR | S_IWUSR);
    if(FileHan<=0)
    {
        return MC_ERR_NOERROR;
    }
    int64_t file_size=0;
    int64_t file_offset=subscription->m_LastFileSize;
    int64_t buf_offset,buf_tail,buf_size,offset,read_offset;    
    uint32_t param_value_start;
    size_t bytes;
    int count=0;
    buf_offset=0;
    buf_tail=0;
    file_size=lseek64(FileHan,0,SEEK_END);
    lseek64(FileHan,file_offset,SEEK_SET);
    m_TmpScript->Resize(MC_CDB_MAX_FILE_READ_BUFFER_SIZE,1);
    buf=m_TmpScript->m_lpData;
    chunk_def.Zero();
    chunk_def.m_SubscriptionID=subscription->m_SubscriptionID;
    
    err=MC_ERR_NOERROR;
    
    sprintf(msg,"Starting source recovery, last file: %d, file_size: %ld, file offset: %ld",subscription->m_LastFileID,file_size,file_offset);        
    LogString(msg);            
    
    while(file_offset<file_size)
    {
        buf_size=MC_CDB_MAX_FILE_READ_BUFFER_SIZE-buf_tail;
        if(buf_size>file_size-file_offset)
        {
            buf_size=file_size-file_offset;
        }
        read_offset=file_offset;
        if(read(FileHan,buf+buf_tail,buf_size) != buf_size)
        {
            err=MC_ERR_INTERNAL_ERROR;                    
        }
        if(err==MC_ERR_NOERROR)
        {
            file_offset+=buf_size;
            buf_size+=buf_tail;
            buf_tail=0;
            buf_offset=0;

            while(buf_offset < buf_size)
            {
                offset=mc_GetParamFromDetailsScriptErr(buf,buf_size,buf_offset,&param_value_start,&bytes,&err);
                if(err)
                {
                    buf_tail=buf_size-buf_offset;
                    if(param_value_start<buf_size)
                    {
                        if(param_value_start-buf_offset+bytes>MC_CDB_MAX_FILE_READ_BUFFER_SIZE)
                        {
                            buf_tail=0;
                            file_offset+=param_value_start+bytes-buf_size;
                            if(file_offset<file_size)
                            {
                                lseek64(FileHan,file_offset,SEEK_SET);
                            }
                        }
                    }
                    if(buf_tail)                                
                    {
                        memmove(buf,buf+buf_offset,buf_tail);
                    }
                    buf_offset=buf_size;
                    err=MC_ERR_NOERROR;
                }
                else
                {
                    if(buf[buf_offset] != 0x00)
                    {
                        err= MC_ERR_CORRUPTED;
                    }
                    else
                    {
                        switch(buf[buf_offset+1])
                        {
                            case MC_ENT_SPRM_TIMESTAMP:
                                if(chunk_found)
                                {
                                    chunk_def.m_InternalFileID=subscription->m_LastFileID;
                                    chunk_def.m_InternalFileOffset=subscription->m_LastFileSize;
                                    subscription->m_LastFileSize=read_offset+buf_offset;                                
                                    chunk_def.m_HeaderSize=subscription->m_LastFileSize-chunk_def.m_InternalFileOffset-chunk_def.m_Size;
                                    chunk_def.SwapPosBytes();
                                    err=m_DB->Write((char*)&chunk_def+m_KeyOffset,m_KeySize,(char*)&chunk_def+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                                    chunk_def.SwapPosBytes();
                                    count++;
                                    subscription->m_Count+=1;
                                    subscription->m_FullSize+=chunk_def.m_Size;
                                    m_DBStat.m_Count+=1;
                                    m_DBStat.m_FullSize+=chunk_def.m_Size;
                                    chunk_def.Zero();
                                    chunk_def.m_SubscriptionID=subscription->m_SubscriptionID;
                                }
                                if(err==MC_ERR_NOERROR)
                                {
                                    chunk_found=1;
                                }
                                break;
                            case MC_ENT_SPRM_CHUNK_HASH:
                                memcpy(chunk_def.m_Hash,buf+param_value_start,bytes);
                                break;
                            case MC_ENT_SPRM_ITEM_COUNT:
                                if(bytes != sizeof(uint32_t))
                                {
                                    err=MC_ERR_CORRUPTED;                                            
                                }
                                else
                                {
                                    chunk_def.m_Pos=(uint32_t)mc_GetLE(buf+param_value_start,bytes);
                                }                                        
                                break;
                            case MC_ENT_SPRM_CHUNK_SIZE:
                                if(bytes != sizeof(uint32_t))
                                {
                                    err=MC_ERR_CORRUPTED;                                            
                                }
                                else
                                {
                                    chunk_def.m_Size=(uint32_t)mc_GetLE(buf+param_value_start,bytes);
                                }                                        
                                break;
                            case MC_ENT_SPRM_FILE_END:
                                chunk_def.m_InternalFileID=subscription->m_LastFileID;
                                chunk_def.m_InternalFileOffset=subscription->m_LastFileSize;
                                subscription->m_LastFileSize=read_offset+buf_offset;                                
                                chunk_def.m_HeaderSize=subscription->m_LastFileSize-chunk_def.m_InternalFileOffset-chunk_def.m_Size;
                                sprintf(msg,"Found end marker at: %ld",read_offset+buf_offset);        
                                LogString(msg);            
                                offset=buf_size;
                                file_offset=file_size;                                
                                break;
                        }
                        buf_offset=offset;                            
                    }
                }
                if(err==MC_ERR_NOERROR)
                {
                    if(count >= 1000)
                    {
                        if(err == MC_ERR_NOERROR)
                        {
                            err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);        
                            count=0;
                        }                            
                    }
                }
                if(err)
                {
                    sprintf(msg,"Error %d on file offset %ld",err,read_offset+buf_offset);        
                    LogString(msg);                                
                    buf_offset=buf_size;
                    file_offset=file_size;                                
                }
            }
        }
        else
        {
            sprintf(msg,"Read error %d on file offset %ld",err,read_offset+buf_offset);        
            LogString(msg);                                
            file_offset=file_size;
        }
    }
        
        
    if(FileHan>0)
    {
        close(FileHan);
    }

    if(err == MC_ERR_NOERROR)
    {
        if(chunk_found)
        {
            if(chunk_def.m_HeaderSize == 0)
            {
                chunk_def.m_InternalFileID=subscription->m_LastFileID;
                chunk_def.m_InternalFileOffset=subscription->m_LastFileSize;
                subscription->m_LastFileSize=file_size;                                
                chunk_def.m_HeaderSize=subscription->m_LastFileSize-chunk_def.m_InternalFileOffset-chunk_def.m_Size;
            }
            chunk_def.SwapPosBytes();
            err=m_DB->Write((char*)&chunk_def+m_KeyOffset,m_KeySize,(char*)&chunk_def+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            chunk_def.SwapPosBytes();
            subscription->m_Count+=1;
            subscription->m_FullSize+=chunk_def.m_Size;
            m_DBStat.m_Count+=1;
            m_DBStat.m_FullSize+=chunk_def.m_Size;
            count++;
            if(err == MC_ERR_NOERROR)
            {
                err=m_DB->Write((char*)subscription+m_KeyOffset,m_KeySize,(char*)subscription+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            }
            if(err == MC_ERR_NOERROR)
            {
                err=m_DB->Write((char*)&m_DBStat+m_KeyOffset,m_KeySize,(char*)&m_DBStat+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            }
            if(err == MC_ERR_NOERROR)
            {
                err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_SYNC_ON_COMMIT);
            }                    
        }
    }

    if(err)
    {
        sprintf(msg,"Source recovery completed with error %d",err);                        
    }
    else
    {
        if(chunk_found)
        {
            sprintf(msg,"Source recovery completed, chunks recovered");                    
        }
        else
        {
            sprintf(msg,"Source recovery completed, no recovery needed");                    
        }
    }
    LogString(msg);                        
    
    Dump("SourceChunkRecovery");
    return err;
}

int mc_ChunkDB::RemoveEntityInternal(mc_TxEntity *entity,uint32_t *removed_chunks,uint64_t *removed_size)                          
{
    int err;
    mc_SubscriptionDBRow subscription;
    mc_SubscriptionDBRow *old_subscription;
    mc_ChunkDBRow chunk_def;
    char msg[256];
    char enthex[65];
    char enthex_dir[65];
    unsigned char* buf;
    int FileHan;
    char FileName[MC_DCT_DB_MAX_PATH];    
    int chunk_found=0;
    int count=0;
    
    err=MC_ERR_NOERROR;
    sprintf_hex(enthex,entity->m_EntityID,MC_TDB_ENTITY_ID_SIZE);
    
    if(removed_chunks)
    {
        *removed_chunks=0;
    }
    if(removed_size)
    {
        *removed_size=0;
    }
    
    subscription.Zero();
    subscription.m_RecordType=MC_CDB_TYPE_SUBSCRIPTION;
    memcpy(&subscription.m_Entity,entity,sizeof(mc_TxEntity));
    
    subscription.m_Entity.m_EntityType &= MC_TET_TYPE_MASK;
    sprintf_hex(enthex_dir,subscription.m_Entity.m_EntityID,MC_AST_SHORT_TXID_SIZE);
    
    old_subscription=FindSubscription(&subscription.m_Entity);
    
    if(old_subscription == NULL)
    {
        return MC_ERR_NOERROR;
    }

    CommitInternal(-4,0);
    
    memcpy(&subscription,old_subscription,sizeof(mc_SubscriptionDBRow));
    subscription.m_Entity.m_EntityType |= MC_TET_DELETED;
    
    
    err=m_DB->Write((char*)&subscription+m_KeyOffset,m_KeySize,(char*)&subscription+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
    if(err == MC_ERR_NOERROR)
    {    
        err=m_DB->Delete((char*)old_subscription+m_KeyOffset,m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
    }
    if(err == MC_ERR_NOERROR)
    {
        err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);        
    }
    
    if(err)
    {
        goto exitlbl;
    }
    
    m_Subscriptions->PutRow(old_subscription->m_SubscriptionID,&subscription,(char*)&subscription+m_ValueOffset);
    
    sprintf(msg,"Entity (%08X, %s) unlinked successfully",entity->m_EntityType,enthex);
    LogString(msg);
 
    m_TmpScript->Resize(MC_CDB_MAX_FILE_READ_BUFFER_SIZE,1);
    buf=m_TmpScript->m_lpData;
    
    chunk_def.Zero();
    chunk_def.m_SubscriptionID=old_subscription->m_SubscriptionID;
    
    for(int file_id=0;file_id<=old_subscription->m_LastFileID;file_id++)
    {
        err=MC_ERR_NOERROR;
        
        mc_CreateDir(old_subscription->m_DirName);
        SetFileName(FileName,old_subscription,file_id);
        FileHan=open(FileName,_O_BINARY | O_RDONLY, S_IRUSR | S_IWUSR);
        if(FileHan<=0)
        {
            err=MC_ERR_INTERNAL_ERROR;
        }
    
        if(err==MC_ERR_NOERROR)
        {
            int64_t file_size=0;
            int64_t file_offset=0;
            int64_t buf_offset,buf_tail,buf_size,offset;
            uint32_t param_value_start;
            size_t bytes;
            buf_offset=0;
            buf_tail=0;
            file_size=lseek64(FileHan,0,SEEK_END);
            lseek64(FileHan,0,SEEK_SET);
            while(file_offset<file_size)
            {
                buf_size=MC_CDB_MAX_FILE_READ_BUFFER_SIZE-buf_tail;
                if(buf_size>file_size-file_offset)
                {
                    buf_size=file_size-file_offset;
                }
                if(read(FileHan,buf+buf_tail,buf_size) != buf_size)
                {
                    err=MC_ERR_INTERNAL_ERROR;                    
                }
                if(err==MC_ERR_NOERROR)
                {
                    file_offset+=buf_size;
                    buf_size+=buf_tail;
                    buf_tail=0;
                    buf_offset=0;
                    
                    while(buf_offset < buf_size)
                    {
                        offset=mc_GetParamFromDetailsScriptErr(buf,buf_size,buf_offset,&param_value_start,&bytes,&err);
                        if(err)
                        {
                            buf_tail=buf_size-buf_offset;
                            if(param_value_start<buf_size)
                            {
                                if(param_value_start-buf_offset+bytes>MC_CDB_MAX_FILE_READ_BUFFER_SIZE)
                                {
                                    buf_tail=0;
                                    file_offset+=param_value_start+bytes-buf_size;
                                    if(file_offset<file_size)
                                    {
                                        lseek64(FileHan,file_offset,SEEK_SET);
                                    }
                                }
                            }
                            if(buf_tail)                                
                            {
                                memmove(buf,buf+buf_offset,buf_tail);
                            }
                            buf_offset=buf_size;
                            err=MC_ERR_NOERROR;
                        }
                        else
                        {
                            if(buf[buf_offset] != 0x00)
                            {
                                err= MC_ERR_CORRUPTED;
                            }
                            else
                            {
                                switch(buf[buf_offset+1])
                                {
                                    case MC_ENT_SPRM_CHUNK_HASH:
                                        if(chunk_found)
                                        {
                                            chunk_def.SwapPosBytes();
                                            err=m_DB->Delete((char*)&chunk_def+m_KeyOffset,m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);                                            
                                            chunk_def.SwapPosBytes();
                                            count++;
                                            chunk_def.Zero();
                                            chunk_def.m_SubscriptionID=old_subscription->m_SubscriptionID;
                                        }
                                        if(err==MC_ERR_NOERROR)
                                        {
                                            if(bytes != MC_CDB_CHUNK_HASH_SIZE)
                                            {
                                                err=MC_ERR_CORRUPTED;
                                            }
                                            else
                                            {
                                                chunk_found=1;
                                                memcpy(chunk_def.m_Hash,buf+param_value_start,bytes);
                                            }
                                        }
                                        break;
                                    case MC_ENT_SPRM_CHUNK_SIZE:
                                        if(removed_chunks)
                                        {
                                            *removed_chunks+=1;
                                        }
                                        if(removed_size)
                                        {
                                            *removed_size+=(uint32_t)mc_GetLE(buf+param_value_start,bytes);                                            
                                        }
                                        break;
                                    case MC_ENT_SPRM_ITEM_COUNT:
                                        if(bytes != sizeof(uint32_t))
                                        {
                                            err=MC_ERR_CORRUPTED;                                            
                                        }
                                        else
                                        {
                                            chunk_def.m_Pos=(uint32_t)mc_GetLE(buf+param_value_start,bytes);
                                        }                                        
                                        break;
                                    case MC_ENT_SPRM_FILE_END:
                                        offset=buf_size;
                                        file_offset=file_size;                                
                                        break;
                                }
                                buf_offset=offset;                            
                            }
                        }
                        if(err==MC_ERR_NOERROR)
                        {
                            if(count >= 1000)
                            {
                                if(err == MC_ERR_NOERROR)
                                {
                                    err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);        
                                    count=0;
                                }                            
                            }
                        }
                        if(err)
                        {
                            buf_offset=buf_size;
                            file_offset=file_size;                                
                        }
                    }
                }
                else
                {
                    file_offset=file_size;
                }
            }
        }
        
        
        if(FileHan>0)
        {
            close(FileHan);
        }
        
        string reason;
        if(pEF->LIC_VerifyFeature(MC_EFT_STREAM_OFFCHAIN_SELECTIVE_PURGE,reason))
        {
            pEF->STR_RemoveDataFromFile(FileName);
        }
    }
    
    if(err == MC_ERR_NOERROR)
    {
        if(chunk_found)
        {
            chunk_def.SwapPosBytes();
            err=m_DB->Delete((char*)&chunk_def+m_KeyOffset,m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);                                            
            chunk_def.SwapPosBytes();
            count++;
        }
    }
    if(err == MC_ERR_NOERROR)
    {
        if(count)
        {
            err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);        
            count=0;
        }                            
    }

    if(err == MC_ERR_NOERROR)
    {
        char dir_name[64];
        sprintf(dir_name,"chunks/data/stream-%s",enthex_dir);
        
        mc_RemoveDir(mc_gState->m_Params->NetworkName(),dir_name);
    }

exitlbl:
            

    if(err)
    {
        sprintf(msg,"Could not remove entity (%08X, %s), error: %d",entity->m_EntityType,enthex,err);        
        LogString(msg);            
    }
    else
    {
        sprintf(msg,"Entity (%08X, %s) removed successfully",entity->m_EntityType,enthex);
        LogString(msg);
    }
    
    return err;
}

int mc_ChunkDB::RemoveEntity(mc_TxEntity *entity,uint32_t *removed_chunks,uint64_t *removed_size)                          
{
    int err;
    
    Lock();
    err=RemoveEntityInternal(entity,removed_chunks,removed_size);    
    UnLock();    
    
    return err;
}


int mc_ChunkDB::FindSubscription(const mc_TxEntity *entity,mc_SubscriptionDBRow *subscription)
{
    mc_SubscriptionDBRow *found;
    
    Lock();
    found=FindSubscription(entity);
    if(found)
    {
        memcpy(subscription,found,sizeof(mc_SubscriptionDBRow));
    }    
    UnLock();
    
    return found ? MC_ERR_NOERROR : MC_ERR_NOT_FOUND;
}



int mc_ChunkDB::Initialize(const char *name,uint32_t mode)
{
    int err,value_len,new_db;   
    char msg[256];
    char dir_name[MC_DCT_DB_MAX_PATH];
    
    mc_SubscriptionDBRow subscription;
    
    unsigned char *ptr;
    
    err=MC_ERR_NOERROR;
    
    strcpy(m_Name,name);
    
    m_DB=new mc_Database;
    
    mc_GetFullFileName(name,"chunks","",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DirName);
    mc_CreateDir(m_DirName);
    mc_GetFullFileName(name,"chunks/chunks",".db",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DBName);
    mc_GetFullFileName(name,"chunks","",MC_FOM_RELATIVE_TO_LOGDIR | MC_FOM_CREATE_DIR,dir_name);
    mc_CreateDir(dir_name);
    mc_GetFullFileName(name,"chunks/chunks",".log",MC_FOM_RELATIVE_TO_LOGDIR | MC_FOM_CREATE_DIR,m_LogFileName);
    
    m_DB->SetOption("KeySize",0,m_KeySize);
    m_DB->SetOption("ValueSize",0,m_ValueSize);
    
    
    err=m_DB->Open(m_DBName,MC_OPT_DB_DATABASE_CREATE_IF_MISSING | MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_LEVELDB);
    
    if(err)
    {
        LogString("Initialize: Cannot open database");
        return err;
    }

    m_Subscriptions=new mc_Buffer;
    
    m_Subscriptions->Initialize(m_KeySize,sizeof(mc_SubscriptionDBRow),MC_BUF_MODE_MAP);    
    
    
    subscription.Zero();
    
    m_DBStat.Zero();
    
    ptr=(unsigned char*)m_DB->Read((char*)&m_DBStat+m_KeyOffset,m_KeySize,&value_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(err)
    {
        LogString("Initialize: Cannot read from database");
        return err;
    }

    new_db=1;
    if(ptr)                                                                     
    {        
        new_db=0;
        memcpy((char*)&m_DBStat+m_ValueOffset,ptr,m_ValueSize);
        
        m_Subscriptions->SetCount(m_DBStat.m_LastSubscription+1);
        subscription.Zero();
        AddSubscription(&subscription);
        
        ptr=(unsigned char*)m_DB->MoveNext(&err);
//        ptr=(unsigned char*)m_DB->Read((char*)&subscription+m_KeyOffset,m_KeySize,&value_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
        if(err)
        {
            return err;
        }

        if(ptr)
        {
            memcpy((char*)&subscription,ptr,m_TotalSize);
        }
        
        while(ptr)
        {
            if( (subscription.m_RecordType != MC_CDB_TYPE_SUBSCRIPTION) || 
                (subscription.m_Zero != 0) || (subscription.m_Zero1 != 0) || (subscription.m_Zero2 != 0))
            {
                ptr=NULL;
            }
            if(ptr)
            {
                AddSubscription(&subscription);
                if(subscription.m_SubscriptionID > m_DBStat.m_LastSubscription)
                {
                    m_DBStat.m_LastSubscription=subscription.m_SubscriptionID;
                }
                ptr=(unsigned char*)m_DB->MoveNext(&err);
                if(err)
                {
                    LogString("Error on MoveNext");            
                    return MC_ERR_CORRUPTED;            
                }
                if(ptr)
                {
                    memcpy((char*)&subscription,ptr,m_TotalSize);
                }
            }            
        }
    }
    else
    {
        m_Subscriptions->SetCount(m_DBStat.m_LastSubscription+2);
        subscription.Zero();
        AddSubscription(&subscription);
        
        subscription.m_Entity.m_EntityType=MC_TET_AUTHOR;
        subscription.m_SubscriptionID=1;
        AddSubscription(&subscription);
        
        m_DBStat.m_LastSubscription=1;
        
        err=m_DB->Write((char*)&m_DBStat+m_KeyOffset,m_KeySize,(char*)&m_DBStat+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return err;
        }        
        
        err=m_DB->Write((char*)&subscription+m_KeyOffset,m_KeySize,(char*)&subscription+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return err;
        }        
        
        
        err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return err;
        }        
    }
 
    m_DBStat.m_InitMode &= MC_WMD_MODE_MASK;
    m_DBStat.m_InitMode |= mode;
    
    m_MemPool=new mc_Buffer;                                                // Key - entity with m_Pos set to 0 + txid
    err=m_MemPool->Initialize(m_KeySize,m_TotalSize,MC_BUF_MODE_MAP);
    
    m_ChunkData=new mc_Script();
    m_ChunkData->Clear();
   
    m_TmpScript=new mc_Script;
    m_TmpScript->Clear();
    
    memset(null_txid,0,MC_TDB_TXID_SIZE);    
    
    m_Semaphore=__US_SemCreate();
    if(m_Semaphore == NULL)
    {
        LogString("Initialize: Cannot initialize semaphore");
        return MC_ERR_INTERNAL_ERROR;
    }

    Dump("Initialize");
    
    sprintf(msg, "Initialized. Chunks: %d",m_DBStat.m_Count);
    LogString(msg);    
    
    if(new_db == 0)
    {
        SourceChunksRecovery();        
    }
    
    return err;   
}

void mc_ChunkDB::Dump(const char *message)
{
    Dump(message,0);
}

void mc_ChunkDB::Dump(const char *message, int force)
{    
    if(force == 0)
    {
        if((m_DBStat.m_InitMode & MC_WMD_DEBUG) == 0)
        {
            return;
        }
    }
    mc_ChunkDBRow dbrow;
    
    unsigned char *ptr;
    int dbvalue_len,err,i;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    FILE *fHan;
    
    sprintf(ShortName,"chunks/chunks");
    mc_GetFullFileName(m_Name,ShortName,".dmp",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
    
    fHan=fopen(FileName,"a");
    if(fHan == NULL)
    {
        return;
    }

    mc_LogString(fHan,message);     
    
    fprintf(fHan,"\nDB\n");
    dbrow.Zero();    
    ptr=(unsigned char*)m_DB->Read((char*)&dbrow+m_KeyOffset,m_KeySize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(err)
    {
        return;
    }

    if(ptr)
    {
        memcpy((char*)&dbrow+m_ValueOffset,ptr,m_ValueSize);
        while(ptr)
        {
            mc_MemoryDumpCharSizeToFile(fHan,(char*)&dbrow+m_KeyOffset,0,m_TotalSize,m_TotalSize);        
            ptr=(unsigned char*)m_DB->MoveNext(&err);
            if(ptr)
            {
                memcpy((char*)&dbrow+m_KeyOffset,ptr,m_TotalSize);            
            }
        }
    }

    fprintf(fHan,"\nMempool\n");
    if(m_MemPool->GetCount())
    {
        mc_MemoryDumpCharSizeToFile(fHan,m_MemPool->GetRow(0),0,m_MemPool->GetCount()*m_TotalSize,m_TotalSize);    
    }
    
    fprintf(fHan,"\nData\n");
    if(m_ChunkData->m_Size)
    {
        mc_MemoryDumpCharSizeToFile(fHan,m_ChunkData->m_lpData,0,m_ChunkData->m_Size,m_TotalSize);    
    }
    
    
    fprintf(fHan,"\nSubscriptions\n");
    
    for(i=0;i<m_Subscriptions->GetCount();i++)
    {
        mc_MemoryDumpCharSizeToFile(fHan,m_Subscriptions->GetRow(i),0,m_TotalSize,m_TotalSize);    
    }
    
    fprintf(fHan,"\n<<<<<< \tChain height: %6d\t%s\n\n",mc_gState->m_Permissions->m_Block,message);
    fclose(fHan);
}

int mc_ScriptMatchesTxIDAndVOut(unsigned char *ptr,size_t bytes,const unsigned char *txid,const int vout)
{
    size_t value_size;
    uint32_t offset;

    if(ptr == NULL)
    {
        return MC_ERR_NOT_FOUND;
    }
    
    offset=mc_FindSpecialParamInDetailsScript(ptr,bytes,MC_ENT_SPRM_SOURCE_TXID,&value_size);
    if(offset != bytes)
    {
        if(memcmp(ptr+offset,txid,MC_TDB_TXID_SIZE) == 0)
        {
            if(vout >= 0)
            {
                offset=mc_FindSpecialParamInDetailsScript(ptr,bytes,MC_ENT_SPRM_SOURCE_VOUT,&value_size);
                if(offset != bytes)
                {
                    if(mc_GetLE(ptr+offset,value_size) == vout)
                    {
                        return MC_ERR_NOERROR;                                                                
                    }
                }                            
            }
            else
            {
                return MC_ERR_NOERROR;                            
            }
        }
    }

    return MC_ERR_NOT_FOUND;
}

int mc_ChunkDB::GetChunkDefInternal(
                        mc_ChunkDBRow *chunk_def,
                        const unsigned char *hash,  
                        const void *entity,
                        const unsigned char *txid,
                        const int vout,
                        int *mempool_row,
                        int check_limit)
{
    mc_SubscriptionDBRow *subscription;
    int err,value_len,mprow; 
    size_t bytes;
    uint32_t on_disk_items=0;
    uint32_t total_items=0;
    unsigned char *ptr;
    
    err=MC_ERR_NOERROR;
    
    chunk_def->Zero();
    
    subscription=FindSubscription((mc_TxEntity*)entity);
    
    if(subscription == NULL)
    {
        return MC_ERR_NOT_FOUND;
    }

    if(mempool_row)
    {
        *mempool_row=-1;
    }

    memcpy(chunk_def->m_Hash,hash,MC_CDB_CHUNK_HASH_SIZE);
    chunk_def->m_SubscriptionID=subscription->m_SubscriptionID;
    chunk_def->m_Pos=0;
    
    mprow=m_MemPool->Seek((unsigned char*)chunk_def);
    if(mprow >= 0)
    {
        if(mempool_row)
        {
            *mempool_row=mprow;
        }
        memcpy(chunk_def,(mc_ChunkDBRow *)m_MemPool->GetRow(mprow),sizeof(mc_ChunkDBRow));
        if(txid)
        {
            on_disk_items=chunk_def->m_TmpOnDiskItems;
            total_items=chunk_def->m_ItemCount;
            while(chunk_def->m_Pos+on_disk_items < total_items)
            {
//                ptr=(unsigned char *)m_ChunkData->GetData(chunk_def->m_InternalFileOffset,&bytes);
                if( (chunk_def->m_TxIDStart == 0) || (chunk_def->m_TxIDStart == (uint32_t)mc_GetLE((void*)txid,4)))
                {
                    ptr=GetChunkInternal(chunk_def,-1,-1,&bytes,NULL,NULL);
                    if(mc_ScriptMatchesTxIDAndVOut(ptr,bytes,txid,vout) == MC_ERR_NOERROR)
                    {
                        return MC_ERR_NOERROR;
                    }
                }
                chunk_def->m_Pos+=1;
                if(chunk_def->m_Pos < total_items)
                {
                    mprow=m_MemPool->Seek((unsigned char*)chunk_def);
                    if(mprow >= 0)
                    {
                        memcpy(chunk_def,(mc_ChunkDBRow *)m_MemPool->GetRow(mprow),sizeof(mc_ChunkDBRow));
                    }
                    else
                    {
                        chunk_def->m_Pos=total_items-on_disk_items;
                    }
                }
            }
        }
        else
        {
            if(mempool_row)
            {
                *mempool_row=mprow;
            }
            return MC_ERR_NOERROR;
        }
        if(on_disk_items == 0)
        {
            err=MC_ERR_NOT_FOUND;            
        }
        else
        {
            mprow=-1;
            chunk_def->m_Pos=0;
        }
    }
    
    if(mprow < 0)
    {
        chunk_def->SwapPosBytes();
        ptr=(unsigned char*)m_DB->Read((char*)chunk_def+m_KeyOffset,m_KeySize,&value_len,MC_OPT_DB_DATABASE_NEXT_ON_READ,&err);
        chunk_def->SwapPosBytes();
        if(err)
        {
            return err;
        }

        if(ptr)
        {
            if(entity)
            {
                if(memcmp((char*)chunk_def+m_KeyOffset,ptr,m_KeySize))
                {
                    ptr=NULL;
                }
            }
            else
            {
                if(memcmp(chunk_def->m_Hash,ptr,MC_CDB_CHUNK_HASH_SIZE))
                {
                    ptr=NULL;
                }                
            }
        }
        
        if(ptr)                                                                     
        {
//            mc_DumpSize("E",chunk_def,m_TotalSize,40);
//            mc_DumpSize("F",ptr,m_TotalSize,40);
            memcpy((char*)chunk_def+m_ValueOffset,ptr+m_ValueOffset,m_ValueSize);        
            on_disk_items=chunk_def->m_ItemCount;
//            mc_DumpSize("G",chunk_def,m_TotalSize,40);
            if(entity == NULL)
            {
                if(chunk_def->m_NextSubscriptionID == 0)
                {
                    chunk_def->m_NextSubscriptionID=((mc_ChunkDBRow*)ptr)->m_SubscriptionID;
                }
            }
//            mc_DumpSize("H",chunk_def,m_TotalSize,40);
            if(txid)
            {
//                ptr=GetChunkInternal(chunk_def,-1,-1,&bytes);
                total_items=chunk_def->m_ItemCount;
                if( (check_limit == -1) || ((int)on_disk_items <= check_limit) )
                {
                    while(chunk_def->m_Pos < on_disk_items)
                    {
                        if( (chunk_def->m_TxIDStart == 0) || (chunk_def->m_TxIDStart == (uint32_t)mc_GetLE((void*)txid,4)))
                        {
                            ptr=GetChunkInternal(chunk_def,-1,-1,&bytes,NULL,NULL);
                            if(mc_ScriptMatchesTxIDAndVOut(ptr,bytes,txid,vout) == MC_ERR_NOERROR)
                            {
                                return MC_ERR_NOERROR;
                            }
                        }
                        chunk_def->m_Pos+=1;

                        if(chunk_def->m_Pos < total_items)
                        {
                            chunk_def->SwapPosBytes();
                            ptr=(unsigned char*)m_DB->Read((char*)chunk_def+m_KeyOffset,m_KeySize,&value_len,0,&err);
                            chunk_def->SwapPosBytes();
                            if(err)
                            {
                                return err;
                            }

                            if(ptr)
                            {
                                memcpy((char*)chunk_def+m_ValueOffset,ptr,m_ValueSize);        
    //                            ptr=GetChunkInternal(chunk_def,-1,-1,&bytes);
                            }
    /*                        
                            else
                            {
                                chunk_def->m_Pos=on_disk_items;
                            }
     */ 
                        }
                    }
                }
                err=MC_ERR_NOT_FOUND;            
            }
            else
            {
/*                
                if(entity == NULL)
                {
                    if(mempool_row)
                    {
                        while(chunk_def->m_NextSubscriptionID)
                        {
                            chunk_def->m_SubscriptionID=chunk_def->m_NextSubscriptionID;
                            chunk_def->SwapPosBytes();
                            ptr=(unsigned char*)m_DB->Read((char*)chunk_def+m_KeyOffset,m_KeySize,&value_len,0,&err);
                            chunk_def->SwapPosBytes();
                            if(err)
                            {
                                return err;
                            }
                    
                            if(ptr)
                            {
                                memcpy((char*)chunk_def+m_ValueOffset,ptr,m_ValueSize);        
                            }
                            else
                            {
                                return MC_ERR_CORRUPTED;
                            }                            
                        }
                    }
                }
 */ 
                return MC_ERR_NOERROR;                
            }
        }
        else
        {
            err=MC_ERR_NOT_FOUND;
        }
    }
    
    return MC_ERR_NOT_FOUND;
}

int mc_ChunkDB::GetChunkDef(
                        mc_ChunkDBRow *chunk_def,
                        const unsigned char *hash,  
                        const void *entity,
                        const unsigned char *txid,
                        const int vout)
{
    int err;
    
    Lock();
    err=GetChunkDefInternal(chunk_def,hash,entity,txid,vout,NULL,-1);
    UnLock();
    
    return err;
}

int mc_ChunkDB::GetChunkDefWithLimit(
                        mc_ChunkDBRow *chunk_def,
                        const unsigned char *hash,  
                        const void *entity,
                        const unsigned char *txid,
                        const int vout,
                        int check_limit)
{
    int err;
    
    Lock();
    err=GetChunkDefInternal(chunk_def,hash,entity,txid,vout,NULL,check_limit);
    UnLock();
    
    return err;
}


int mc_ChunkDB::AddChunkInternal(                                                           
                 const unsigned char *hash,                                     
                 const mc_TxEntity *entity,                                     
                 const unsigned char *txid,
                 const int vout,
                 const unsigned char *chunk,                                    
                 const unsigned char *details,                                  
                 const unsigned char *salt,                                     
                 const uint32_t chunk_size,                                     
                 const uint32_t details_size,                                   
                 const uint32_t salt_size,                                   
                 const uint32_t flags)
{
    int err;
    int add_null_row,add_entity_row;
    int total_items,on_disk_items,pos;
    int mempool_entity_row;
    int mempool_last_null_row;
    uint32_t timestamp;
    size_t bytes;
    const unsigned char *ptr;
    char chunk_hex[65];
    char msg[256];
    char enthex[65];
    
    
    mc_ChunkDBRow chunk_def;
    mc_ChunkDBRow entity_chunk_def;
    mc_ChunkDBRow null_chunk_def;
    mc_SubscriptionDBRow *subscription;
    
    chunk_def.Zero();
    
    err=GetChunkDefInternal(&chunk_def,hash,entity,txid,vout,NULL,-1);
    if(err == MC_ERR_NOERROR)
    {
        return MC_ERR_FOUND;
    }
    if(err != MC_ERR_NOT_FOUND)
    {
        return err; 
    }
    
    if( (m_ChunkData->m_Size + chunk_size + details_size + salt_size +MC_CDB_MAX_CHUNK_EXTRA_SIZE > MC_CDB_MAX_CHUNK_DATA_POOL_SIZE) || 
        (m_MemPool->GetCount() + 2 > MC_CDB_MAX_MEMPOOL_SIZE ) )
    {
        CommitInternal(-1,0);
    }

    subscription=FindSubscription(entity);
    if(subscription == NULL)
    {
        mc_TxEntity new_entity;
        memcpy(&new_entity,entity,sizeof(mc_TxEntity));
        AddEntityInternal(&new_entity,0);
        subscription=FindSubscription(entity);
    }
    if(subscription == NULL)
    {
        LogString("Internal error: trying to add chunk to unsubscribed entity");
        return MC_ERR_INTERNAL_ERROR;
    }
    if(subscription->m_SubscriptionID == 0)
    {
        LogString("Internal error: trying to add chunk to null entity");
        return MC_ERR_INTERNAL_ERROR;        
    }
            
    add_null_row=0;
    add_entity_row=0;
    total_items=0;
    on_disk_items=0;
    mempool_entity_row=-1;
    
    if(txid)
    {
        err=GetChunkDefInternal(&entity_chunk_def,hash,entity,NULL,-1,&mempool_entity_row,-1);
        if(err == MC_ERR_NOERROR)
        {
            total_items=entity_chunk_def.m_ItemCount;
            on_disk_items=total_items;
            if(entity_chunk_def.m_InternalFileID < 0)
            {
                on_disk_items=entity_chunk_def.m_TmpOnDiskItems;
            }
        }
        else
        {
            if(err != MC_ERR_NOT_FOUND)
            {
                return err; 
            }
            entity_chunk_def.m_SubscriptionID=0;
            add_entity_row=1;
        }
    }
    else
    {
        add_entity_row=1;        
    }

    if(add_entity_row)
    {
        err=GetChunkDefInternal(&null_chunk_def,hash,NULL,NULL,-1,&mempool_last_null_row,-1);
        if(err == MC_ERR_NOT_FOUND)
        {
            add_null_row=1;
        }  
        else
        {      
            if(err != MC_ERR_NOERROR)
            {
                return err; 
            }
        }
    }
        
    chunk_def.Zero();
    memcpy(chunk_def.m_Hash,hash,MC_CDB_CHUNK_HASH_SIZE);
    chunk_def.m_InternalFileID=-1;
    chunk_def.m_InternalFileOffset=m_ChunkData->m_NumElements;
    
    m_TmpScript->Clear();    
    m_TmpScript->AddElement();
    
    timestamp=mc_TimeNowAsUInt();
    pos=total_items-on_disk_items;
    
    m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_TIMESTAMP,(unsigned char*)&timestamp,sizeof(timestamp));
    m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_CHUNK_HASH,hash,MC_CDB_CHUNK_HASH_SIZE);
    if(txid)
    {
        m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_SOURCE_TXID,txid,MC_TDB_TXID_SIZE);
        if(vout >= 0)
        {
            m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_SOURCE_VOUT,(unsigned char*)&vout,sizeof(vout));            
        }
    }
/*    
    else
    {
        m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_SOURCE_TXID,null_txid,MC_TDB_TXID_SIZE);
        m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_SOURCE_VOUT,(unsigned char*)&vout,sizeof(vout));                    
    }
 */ 
    if(details_size)
    {
        m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_CHUNK_DETAILS,details,details_size);        
    }
    if(salt_size)
    {
        m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_SALT,salt,salt_size);        
    }
    if(total_items == 0)
    {
        m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_CHUNK_SIZE,(unsigned char*)&chunk_size,sizeof(chunk_size));
    }
    else
    {
        m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_ITEM_COUNT,(unsigned char*)&total_items,sizeof(total_items));        
    }
    
    chunk_def.m_HeaderSize=m_TmpScript->m_Size;
    
    if(total_items == 0)
    {
        if(chunk_size)
        {
            m_TmpScript->SetSpecialParamValue(MC_ENT_SPRM_CHUNK_DATA,chunk,chunk_size);                
            chunk_def.m_HeaderSize=m_TmpScript->m_Size-chunk_size;
        }
    }    

    chunk_def.m_Size=chunk_size;    
    chunk_def.m_Flags=flags;
    
    ptr=m_TmpScript->GetData(0,&bytes);
    
    m_ChunkData->SetElement(m_ChunkData->m_NumElements-1);
    m_ChunkData->AddElement();
    err=m_ChunkData->SetData(ptr,bytes);
    if(err)
    {
        return err;
    }
        
    if(add_null_row)
    {
        chunk_def.m_SubscriptionID=0;
        chunk_def.m_Pos=0;
        chunk_def.m_ItemCount=1;
        chunk_def.m_TmpOnDiskItems=0;
/*        
        chunk_def.m_PrevSubscriptionID=0;
 */ 
        chunk_def.m_NextSubscriptionID=subscription->m_SubscriptionID;
        m_MemPool->Add(&chunk_def,(char*)&chunk_def+m_ValueOffset);        
    }

    chunk_def.m_SubscriptionID=subscription->m_SubscriptionID;
    chunk_def.m_Pos=pos;
    chunk_def.m_ItemCount=total_items+1;
    chunk_def.m_TmpOnDiskItems=on_disk_items;
    chunk_def.m_NextSubscriptionID=0;
    if(txid)
    {
        chunk_def.m_TxIDStart=(uint32_t)mc_GetLE((void*)txid,4);
    }
    
/*    
    if(add_null_row)
    {
        chunk_def.m_PrevSubscriptionID=0;
        chunk_def.m_NextSubscriptionID=0;
    }
    else
    {
        if(add_entity_row)
        {
            chunk_def.m_PrevSubscriptionID=entity_chunk_def.m_SubscriptionID;
            chunk_def.m_NextSubscriptionID=0;
        }
        else
        {
            chunk_def.m_PrevSubscriptionID=entity_chunk_def.m_PrevSubscriptionID;
            chunk_def.m_NextSubscriptionID=entity_chunk_def.m_NextSubscriptionID;            
        }
    }
*/
    
    m_MemPool->Add(&chunk_def,(char*)&chunk_def+m_ValueOffset);
    
    if(mempool_entity_row >= 0)
    {
        ((mc_ChunkDBRow *)m_MemPool->GetRow(mempool_entity_row))->m_ItemCount += 1;        
    }
    
    
    if(mempool_last_null_row >= 0)
    {
        if(add_entity_row)
        {
            ((mc_ChunkDBRow *)m_MemPool->GetRow(mempool_last_null_row))->m_NextSubscriptionID = subscription->m_SubscriptionID;                    
        }
    }

    
    sprintf_hex(enthex,entity->m_EntityID,MC_TDB_ENTITY_ID_SIZE);
    sprintf_hex(chunk_hex,hash,MC_CDB_CHUNK_HASH_SIZE);    
    sprintf(msg,"New Chunk %s, size %d, flags %08X, entity (%08X, %s)",chunk_hex,chunk_size,flags,entity->m_EntityType,enthex);
    LogString(msg);
 
/*        
    if(entity->m_EntityType == MC_TET_AUTHOR)
    {
        if((m_DBStat.m_InitMode & MC_WMD_NO_CHUNK_FLUSH) == 0)
        {
            FlushLastChunk();
//            CommitInternal(-2);
        }
    }
 */ 
    
    return MC_ERR_NOERROR;
}

int mc_ChunkDB::AddChunk(                                                           
                 const unsigned char *hash,                                     
                 const mc_TxEntity *entity,                                     
                 const unsigned char *txid,
                 const int vout,
                 const unsigned char *chunk,                                    
                 const unsigned char *details,                                  
                 const unsigned char *salt,                                     
                 const uint32_t chunk_size,                                     
                 const uint32_t details_size,                                   
                 const uint32_t salt_size,                                      
                 const uint32_t flags)
{
    int err;
    
    Lock();
    err=AddChunkInternal(hash,entity,txid,vout,chunk,details,salt,chunk_size,details_size,salt_size,flags);
    UnLock();
    
    return err;
}

void mc_ChunkDB::SetFileName(char *FileName,
                     mc_SubscriptionDBRow *subscription,
                     uint32_t fileid)
{
    sprintf(FileName,"%s/chunks%06u.dat",subscription->m_DirName,fileid);    
}


void mc_GetChunkSalt(unsigned char* ptr,uint32_t max_size,unsigned char *salt,uint32_t *salt_size)
{
    if(salt)
    {
        *salt_size=0;
        uint32_t salt_offset;
        size_t salt_bytes=0;
        salt_offset=mc_FindSpecialParamInDetailsScript(ptr,max_size,MC_ENT_SPRM_SALT,&salt_bytes);
        if(salt_offset != max_size)
        {
            if(salt_bytes <= MAX_CHUNK_SALT_SIZE)
            {
                *salt_size=salt_bytes;
                memcpy(salt,ptr+salt_offset,*salt_size);
            }
        }            
    }    
}

unsigned char *mc_ChunkDB::GetChunkInternal(mc_ChunkDBRow *chunk_def,
                                    int32_t offset,
                                    int32_t len,
                                    size_t *bytes,
                                    unsigned char *salt,
                                    uint32_t *salt_size)        
{
    unsigned char *ptr;
    size_t bytes_to_read;
    int subscription_id;
    mc_SubscriptionDBRow *subscription;
    char FileName[MC_DCT_DB_MAX_PATH];    
    int FileHan;
    uint32_t read_from;
    mc_ChunkDBRow chunk_def_zero;
    
    ptr=NULL;
    if((offset >= 0) && (chunk_def->m_Pos > 0) )
    {
        subscription_id=chunk_def->m_SubscriptionID;
        if(subscription_id == 0)
        {
            subscription_id=chunk_def->m_NextSubscriptionID;
        }
        subscription=(mc_SubscriptionDBRow *)m_Subscriptions->GetRow(subscription_id);
        
        if(GetChunkDefInternal(&chunk_def_zero,chunk_def->m_Hash,&(subscription->m_Entity),NULL,0,NULL,-1) == MC_ERR_NOERROR)
        {
            return GetChunkInternal(&chunk_def_zero,offset,len,bytes,salt,salt_size);
        }
        return NULL;
    }
    
    FileHan=0;
    if(chunk_def->m_InternalFileID < 0)
    {
        ptr=(unsigned char *)m_ChunkData->GetData(chunk_def->m_InternalFileOffset,&bytes_to_read);
        read_from=0;
        if(offset >= 0)
        {
            mc_GetChunkSalt(ptr,chunk_def->m_HeaderSize,salt,salt_size);
            read_from+=chunk_def->m_HeaderSize+offset;
            if(offset >= (int)chunk_def->m_Size)
            {
                return NULL;
            }
            bytes_to_read=chunk_def->m_Size-offset;
        }
        if(bytes)
        {
            *bytes=bytes_to_read;
        }
        return ptr+read_from;
    }
    else
    {
        subscription_id=chunk_def->m_SubscriptionID;
        if(subscription_id == 0)
        {
            subscription_id=chunk_def->m_NextSubscriptionID;
        }
        subscription=(mc_SubscriptionDBRow *)m_Subscriptions->GetRow(subscription_id);
        SetFileName(FileName,subscription,chunk_def->m_InternalFileID);
     
        FileHan=open(FileName,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if(FileHan<=0)
        {
            return NULL;
        }
        
        if(chunk_def->m_HeaderSize >=0x80000000)                                // Fixing the overflow bug if data is not written
        {
            chunk_def->m_HeaderSize+=chunk_def->m_Size;
        }
    
        read_from=chunk_def->m_InternalFileOffset;
        bytes_to_read=chunk_def->m_HeaderSize;
        if(offset >= 0)
        {            
            if(salt)
            {
                if(lseek64(FileHan,read_from,SEEK_SET) != (int)read_from)
                {
                    goto exitlbl;
                }
                m_TmpScript->Clear();
                if(m_TmpScript->Resize(bytes_to_read,1))
                {
                    goto exitlbl;                                
                }
    
                if(read(FileHan,m_TmpScript->m_lpData,bytes_to_read) != (int)bytes_to_read)
                {
                    goto exitlbl;
                }
                mc_GetChunkSalt(m_TmpScript->m_lpData,chunk_def->m_HeaderSize,salt,salt_size);                
            }

            read_from+=chunk_def->m_HeaderSize+offset;
            bytes_to_read=chunk_def->m_Size;
            if(len>0)
            {
                bytes_to_read=len;
            }
        }
        
        if(lseek64(FileHan,read_from,SEEK_SET) != (int)read_from)
        {
            goto exitlbl;
        }
        
        m_TmpScript->Clear();
        if(m_TmpScript->Resize(bytes_to_read,1))
        {
            goto exitlbl;            
        }
    
        if(read(FileHan,m_TmpScript->m_lpData,bytes_to_read) != (int)bytes_to_read)
        {
            goto exitlbl;
        }
        
        ptr=m_TmpScript->m_lpData;        
        if(bytes)
        {
            *bytes=bytes_to_read;
        }
}

exitlbl:
        
    if(FileHan)
    {        
        close(FileHan);
    }
        
    return ptr;
}

unsigned char *mc_ChunkDB::GetChunk(mc_ChunkDBRow *chunk_def,
                                    int32_t offset,
                                    int32_t len,
                                    size_t *bytes,
                                    unsigned char *salt,
                                    uint32_t *salt_size)
{
    unsigned char *ptr;
    
    Lock();
    ptr=GetChunkInternal(chunk_def,offset,len,bytes,salt,salt_size);
    UnLock();
    
    return ptr;
}


int mc_ChunkDB::AddToFile(const void* chunk,                  
                          uint32_t size,
                          mc_SubscriptionDBRow *subscription,
                          uint32_t fileid,
                          uint32_t offset,
                          uint32_t flush_mode)
{
    char FileName[MC_DCT_DB_MAX_PATH];         
    int FileHan,err;
    uint32_t tail_size,file_size,expected_end,new_file_size;
    unsigned char tail[3];
    
    tail[0]=0x00;
    tail[1]=MC_ENT_SPRM_FILE_END;
    tail[2]=0x00;
    
    mc_CreateDir(subscription->m_DirName);
    SetFileName(FileName,subscription,fileid);
    err=MC_ERR_NOERROR;
    FileHan=open(FileName,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
    if(FileHan<=0)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    expected_end=offset+size+3;
    tail_size=0;
    if( (offset == 0) || ( (expected_end % MC_CDB_FILE_PAGE_SIZE) != (offset % MC_CDB_FILE_PAGE_SIZE) ) )
    {
        file_size=lseek64(FileHan,0,SEEK_END);
        new_file_size=((expected_end-1) / MC_CDB_FILE_PAGE_SIZE + 1) * MC_CDB_FILE_PAGE_SIZE;
        if(new_file_size > file_size)
        {
            if(new_file_size > expected_end)
            {
                tail_size=new_file_size-expected_end;
            }
        }
    }
    
    if(lseek64(FileHan,offset,SEEK_SET) != offset)
    {
        err=MC_ERR_INTERNAL_ERROR;
        goto exitlbl;
    }
    
    if(write(FileHan,chunk,size) != size)
    {
        err=MC_ERR_INTERNAL_ERROR;
        return MC_ERR_INTERNAL_ERROR;
    }

    if(write(FileHan,tail,3) != 3)
    {
        err=MC_ERR_INTERNAL_ERROR;
        return MC_ERR_INTERNAL_ERROR;
    }
    
    if(tail_size)
    {
        unsigned char empty_buf[65536];
        uint32_t empty_buf_size=65536;
        memset(empty_buf,0,empty_buf_size);
        while(tail_size)
        {
            if(empty_buf_size>tail_size)
            {
                empty_buf_size=tail_size;
            }
            if(write(FileHan,empty_buf,empty_buf_size) != empty_buf_size)
            {
                err=MC_ERR_INTERNAL_ERROR;
                return MC_ERR_INTERNAL_ERROR;
            }
            tail_size-=empty_buf_size;
        }        
    }
    
exitlbl:

    if(err == MC_ERR_NOERROR)
    {
        if(flush_mode)
        {
            __US_FlushFileWithMode(FileHan,flush_mode & MC_CDB_FLUSH_MODE_DATASYNC);            
        }
    }
    close(FileHan);
    return err;
}

int mc_ChunkDB::FlushDataFile(mc_SubscriptionDBRow *subscription,uint32_t fileid,uint32_t flush_mode)
{
    char FileName[MC_DCT_DB_MAX_PATH];         
    int FileHan;
    
    mc_CreateDir(subscription->m_DirName);
    SetFileName(FileName,subscription,fileid);
    
    FileHan=open(FileName,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    
    if(FileHan<=0)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    __US_FlushFileWithMode(FileHan,flush_mode);
    close(FileHan);
    return MC_ERR_NOERROR;
}

int mc_ChunkDB::FlushSourceChunks(uint32_t flush_mode)
{
    mc_ChunkDBRow *chunk_def;
    mc_SubscriptionDBRow *subscription;
    int err,row,first_row,last_row;
    int full_commit_required;
    uint32_t size;
    char msg[256];
        
    if(flush_mode & MC_CDB_FLUSH_MODE_COMMIT)
    {
        return Commit(-2,flush_mode);
    }
    
    if( (flush_mode & MC_CDB_FLUSH_MODE_FILE) == 0)
    {
        return MC_ERR_NOERROR;
    }

    full_commit_required=0;
    
    Lock();
    
    err=MC_ERR_NOERROR;

    row=m_MemPool->GetCount()-1;
    first_row=m_MemPool->GetCount();
    last_row=first_row;
    
    while(row >= 0)
    {
        chunk_def=(mc_ChunkDBRow *)m_MemPool->GetRow(row);
        if(chunk_def->m_SubscriptionID == 1)
        {
            if( (chunk_def->m_StorageFlags & MC_CFL_STORAGE_FLUSHED) == 0)
            {
                first_row=row;
                if(last_row == m_MemPool->GetCount())
                {
                    last_row=row;
                }
            }
            else
            {
                row=0;
            }
        }
        row--;
    }
        
    subscription=(mc_SubscriptionDBRow *)m_Subscriptions->GetRow(1);
    
    for(row=first_row;row<m_MemPool->GetCount();row++)
    {
        chunk_def=(mc_ChunkDBRow *)m_MemPool->GetRow(row);
        if(chunk_def->m_SubscriptionID == 1)
        {
            size=chunk_def->m_Size+chunk_def->m_HeaderSize;                     // In source, chunk stored only once, with data
            if(subscription->m_LastFileSize+size > MC_CDB_MAX_FILE_SIZE)                          // New file is needed
            {
                FlushDataFile(subscription,subscription->m_LastFileID,0);
                subscription->m_LastFileID+=1;
                subscription->m_LastFileSize=0;
                full_commit_required=1;
            }            

            err=AddToFile(GetChunkInternal(chunk_def,-1,-1,NULL,NULL,NULL),size,
                          subscription,subscription->m_LastFileID,subscription->m_LastFileSize,(row==last_row) ? flush_mode : 0);
            if(err)
            {
                sprintf(msg,"Couldn't store key in file, error:  %d",err);
                LogString(msg);
                goto exitlbl;
            }

            chunk_def->m_InternalFileID=subscription->m_LastFileID;
            chunk_def->m_InternalFileOffset=subscription->m_LastFileSize;
            chunk_def->m_Pos+=chunk_def->m_TmpOnDiskItems;
            chunk_def->m_StorageFlags |= MC_CFL_STORAGE_FLUSHED;

            subscription->m_LastFileSize+=size;    
            subscription->m_Count+=1;
            subscription->m_FullSize+=chunk_def->m_Size;

            m_DBStat.m_Count+=1;
            m_DBStat.m_FullSize+=chunk_def->m_Size;            
        }        
    }
    
exitlbl:    
    UnLock();

    if(full_commit_required)
    {
        return Commit(-2,flush_mode);        
    }

    return err; 
}

int mc_ChunkDB::CommitInternal(int block,uint32_t flush_mode)
{
    int r,s;
    int err;
//    int last_file_id, last_file_offset;
    uint32_t size;
    int value_len;
    unsigned char *ptr;
    char msg[256];

    mc_SubscriptionDBRow *subscription;
    mc_ChunkDBRow *chunk_def;
    mc_ChunkDBRow entity_chunk_def;
    
    err=MC_ERR_NOERROR;
    
    if(m_MemPool->GetCount() == 0)
    {
        goto exitlbl;
    }
    Dump("Before Commit");
        
//    last_file_id=-1;
//    last_file_offset=0;
    
    for(r=0;r<m_MemPool->GetCount();r++)
    {
        chunk_def=(mc_ChunkDBRow *)m_MemPool->GetRow(r);
        
        if(chunk_def->m_SubscriptionID)
        {
            s=chunk_def->m_SubscriptionID;

            subscription=(mc_SubscriptionDBRow *)m_Subscriptions->GetRow(s);
            subscription->m_TmpFlags |= MC_CDB_TMP_FLAG_SHOULD_COMMIT;

            if( (chunk_def->m_StorageFlags & MC_CFL_STORAGE_FLUSHED) == 0)
            {
//                size=chunk_def->m_Size+chunk_def->m_HeaderSize;               // Fixed bug, chunk itself is not always stored
                size=chunk_def->m_HeaderSize;
                if( (chunk_def->m_Pos + chunk_def->m_TmpOnDiskItems) == 0)
                {
                    size+=chunk_def->m_Size;
                }
                if(subscription->m_LastFileSize+size > MC_CDB_MAX_FILE_SIZE)                          // New file is needed
                {
                    FlushDataFile(subscription,subscription->m_LastFileID,flush_mode);
                    subscription->m_LastFileID+=1;
                    subscription->m_LastFileSize=0;
                }            

                err=AddToFile(GetChunkInternal(chunk_def,-1,-1,NULL,NULL,NULL),size,
                              subscription,subscription->m_LastFileID,subscription->m_LastFileSize,0);
                if(err)
                {
                    sprintf(msg,"Couldn't store key in file, error:  %d",err);
                    LogString(msg);
                    return err;
                }

                chunk_def->m_InternalFileID=subscription->m_LastFileID;
                chunk_def->m_InternalFileOffset=subscription->m_LastFileSize;
                chunk_def->m_Pos+=chunk_def->m_TmpOnDiskItems;

                subscription->m_LastFileSize+=size;    
                subscription->m_Count+=1;
                subscription->m_FullSize+=chunk_def->m_Size;

                m_DBStat.m_Count+=1;
                m_DBStat.m_FullSize+=chunk_def->m_Size;
            }

            if(chunk_def->m_TmpOnDiskItems)
            {
                if(chunk_def->m_SubscriptionID)
                {
                entity_chunk_def.Zero();
                memcpy(&entity_chunk_def,chunk_def,sizeof(mc_ChunkDBRow));
                entity_chunk_def.m_Pos=0;
                entity_chunk_def.m_TmpOnDiskItems=0;
                entity_chunk_def.SwapPosBytes();
                ptr=(unsigned char*)m_DB->Read((char*)&entity_chunk_def+m_KeyOffset,m_KeySize,&value_len,0,&err);
                entity_chunk_def.SwapPosBytes();
                if(err)
                {
                    goto exitlbl;
                }
                if(ptr)
                {
                    memcpy((char*)&entity_chunk_def+m_ValueOffset,ptr,m_ValueSize);
                    entity_chunk_def.m_ItemCount=chunk_def->m_ItemCount;
                    entity_chunk_def.SwapPosBytes();
                    err=m_DB->Write((char*)&entity_chunk_def+m_KeyOffset,m_KeySize,(char*)&entity_chunk_def+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                    entity_chunk_def.SwapPosBytes();
                    if(err)
                    {
                        goto exitlbl;
                    }                                                            
                }
                }
            }

            chunk_def->m_TmpOnDiskItems=0;
            chunk_def->SwapPosBytes();
            err=m_DB->Write((char*)chunk_def+m_KeyOffset,m_KeySize,(char*)chunk_def+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            chunk_def->SwapPosBytes();
            if(err)
            {
                goto exitlbl;
            }                                            
        }
    }
    
    for(s=0;s<m_Subscriptions->GetCount();s++)
    {
        subscription=(mc_SubscriptionDBRow *)m_Subscriptions->GetRow(s);
        if(subscription->m_TmpFlags & MC_CDB_TMP_FLAG_SHOULD_COMMIT)            
        {
            subscription->m_TmpFlags=0;

            FlushDataFile(subscription,subscription->m_LastFileID,flush_mode);

            err=m_DB->Write((char*)subscription+m_KeyOffset,m_KeySize,(char*)subscription+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            if(err)
            {
                goto exitlbl;
            }                                            

        }
    }

    err=m_DB->Write((char*)&m_DBStat+m_KeyOffset,m_KeySize,(char*)&m_DBStat+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
    if(err)
    {
        goto exitlbl;
    }                                            
        
    err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_SYNC_ON_COMMIT);
    if(err)
    {
        goto exitlbl;
    }
    
    
exitlbl:
    if(err)
    {
        sprintf(msg,"Could not commit new Block %d, Chunks: %d, Error: %d",block,m_MemPool->GetCount(),err);
        LogString(msg);   
    }
    else
    {
        if(block >= -1)
        {
            sprintf(msg,"NewBlock %d, Chunks: %d,",block,m_MemPool->GetCount());
            LogString(msg);   
        }
        m_FeedPos=0;
        m_MemPool->Clear();
        m_ChunkData->Clear();
    }
    Dump("After Commit");
        
    return MC_ERR_NOERROR;    
}

int mc_ChunkDB::Commit(int block)
{
    int err;
    
    if(block)
    {
        pEF->FED_EventChunksAvailable();    
    }
    
    Lock();
    err=CommitInternal(block,0);
    UnLock();
    
    return err;
}

int mc_ChunkDB::Commit(int block,uint32_t flush_mode)
{
    int err;
    
    Lock();
    err=CommitInternal(block,flush_mode);
    UnLock();
    
    return err;
}

int mc_ChunkDB::RestoreChunkIfNeeded(mc_ChunkDBRow *chunk_def)
{
    if( (chunk_def->m_StorageFlags & MC_CFL_STORAGE_PURGED) == 0 )
    {
        return MC_ERR_NOERROR;
    }
    
    int err=MC_ERR_NOERROR;
    chunk_def->m_StorageFlags-=MC_CFL_STORAGE_PURGED;
    
    chunk_def->SwapPosBytes();
    err=m_DB->Write((char*)chunk_def+m_KeyOffset,m_KeySize,
            (char*)chunk_def+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_DEFAULT);
    chunk_def->SwapPosBytes();                                        

    return err;
}

