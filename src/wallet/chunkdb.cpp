// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#include "wallet/chunkdb.h"

#define MC_CDB_MAX_FILE_SIZE             0x8000000                              // Maximal data file size, 128MB

void sprintf_hex(char *hex,const unsigned char *bin,int size);
/*
{
    int i;
    for(i=0;i<size;i++)
    {
        sprintf(hex+(i*2),"%02x",bin[size-1-i]);
    }
    
    hex[size*2]=0;      
}
*/

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
    m_KeySize=36;
    m_ValueOffset=36;
    m_ValueSize=28;    
    m_TotalSize=m_KeySize+m_ValueSize;
    
    m_Subscriptions=NULL;    
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
    
    Zero();
    return MC_ERR_NOERROR;       
}

int mc_ChunkDB::AddSubscription(mc_SubscriptionDBRow *subscription)
{
    char enthex[33];
    char dir_name[64];
    switch(subscription->m_Entity.m_EntityType)
    {
        case MC_TET_STREAM:
            sprintf_hex(enthex,subscription->m_Entity.m_EntityID,MC_AST_SHORT_TXID_SIZE);
            sprintf(dir_name,"chunks/data/stream-%s",enthex);
            break;
        default:
            return MC_ERR_NOT_SUPPORTED;
    }
    mc_GetFullFileName(m_Name,dir_name,"",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,subscription->m_DirName);
    mc_CreateDir(subscription->m_DirName);
    
    return m_Subscriptions->Add(subscription,(char*)subscription+m_ValueOffset);    
}

mc_SubscriptionDBRow *mc_ChunkDB::FindSubscription(mc_TxEntity* entity)
{
    int row;
    mc_SubscriptionDBRow subscription;
    
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

int mc_ChunkDB::AddEntity(mc_TxEntity* entity, uint32_t flags)
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

int mc_ChunkDB::Initialize(const char *name,uint32_t mode)
{
    int err,value_len,i;   
    char msg[256];
    
    mc_SubscriptionDBRow subscription;
    
    unsigned char *ptr;
    
    err=MC_ERR_NOERROR;
    
    strcpy(m_Name,name);
    
    m_DB=new mc_Database;
    
    mc_GetFullFileName(name,"chunks","",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DirName);
    mc_CreateDir(m_DirName);
    mc_GetFullFileName(name,"chunks/chunks",".db",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DBName);
    mc_GetFullFileName(name,"chunks/chunks",".log",MC_FOM_RELATIVE_TO_DATADIR,m_LogFileName);
    
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
    
    m_DBStat.Zero();
    
    ptr=(unsigned char*)m_DB->Read((char*)&m_DBStat+m_KeyOffset,m_KeySize,&value_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(err)
    {
        LogString("Initialize: Cannot read from database");
        return err;
    }

    if(ptr)                                                                     
    {        
        memcpy((char*)&m_DBStat+m_ValueOffset,ptr,m_ValueSize);
        
        subscription.Zero();
        
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
                (subscription.m_Zero != 0) || (subscription.m_Zero1 != 0) )
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
        err=m_DB->Write((char*)&m_DBStat+m_KeyOffset,m_KeySize,(char*)&m_DBStat+m_ValueOffset,m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
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
 
    m_DBStat.m_InitMode |= mode;
    
    Dump("Initialize");
    
    sprintf(msg, "Initialized. Chunks: %d",m_DBStat.m_Count);
    LogString(msg);    
    
    return err;   
}

void mc_ChunkDB::Dump(const char *message)
{    
    if((m_DBStat.m_InitMode & MC_WMD_DEBUG) == 0)
    {
        return;
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
    
    fprintf(fHan,"Entities\n");
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
    
/*    
    for(i=0;i<MC_TDB_MAX_IMPORTS;i++)
    {
        if(m_RawMemPools[i])
        {
            if(m_RawMemPools[i]->GetCount())
            {
                fprintf(fHan,"RawMemPool %d\n",m_Imports[i].m_ImportID);
                mc_MemoryDumpCharSizeToFile(fHan,m_RawMemPools[i]->GetRow(0),0,m_RawMemPools[i]->GetCount()*m_Database->m_TotalSize,m_Database->m_TotalSize);    
            }
        }
        if(m_MemPools[i])
        {
            if(m_MemPools[i]->GetCount())
            {
                fprintf(fHan,"MemPool %d\n",m_Imports[i].m_ImportID);
                mc_MemoryDumpCharSizeToFile(fHan,m_MemPools[i]->GetRow(0),0,m_MemPools[i]->GetCount()*m_Database->m_TotalSize,m_Database->m_TotalSize);    
            }
        }
    }
    if(m_RawUpdatePool)
    {
        if(m_RawUpdatePool->GetCount())
        {
            fprintf(fHan,"RawUpdatePool\n");
            mc_MemoryDumpCharSizeToFile(fHan,m_RawUpdatePool->GetRow(0),0,m_RawUpdatePool->GetCount()*m_Database->m_TotalSize,m_Database->m_TotalSize);    
        }
    }
*/    
    fprintf(fHan,"\n<<<<<< \tChain height: %6d\t%s\n\n",mc_gState->m_Permissions->m_Block,message);
    fclose(fHan);
}
