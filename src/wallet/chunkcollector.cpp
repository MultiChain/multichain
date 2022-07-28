// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#include "wallet/chunkcollector.h"

#define MC_IMPOSSIBLE_NEXT_ATTEMPT 0xFFFFFFFF
#define MC_CCW_KBS_PER_SECOND_DELAY_UP         60000
#define MC_CCW_KBS_PER_SECOND_DELAY_DOWN       15000

void mc_ChunkEntityKey::Zero()
{
    memset(this,0, sizeof(mc_ChunkEntityKey));    
}

void mc_ChunkEntityValue::Zero()
{
    memset(this,0, sizeof(mc_ChunkEntityValue));    
}


void mc_ChunkCollectorRow::Zero()
{
    memset(this,0, sizeof(mc_ChunkCollectorRow));
}

void mc_ChunkCollectorDBRow::Zero()
{
    memset(this,0, sizeof(mc_ChunkCollectorDBRow));    
}

void mc_ChunkCollectorStat::Zero()
{
    memset(this,0, sizeof(mc_ChunkCollectorStat));    
}    
    
void mc_ChunkCollector::Zero()
{
    m_DB=NULL;
    m_ChunkDB=NULL;
    m_KeyOffset=0;
    m_KeyDBOffset=0;
    m_KeySize=MC_TDB_TXID_SIZE+sizeof(int)+sizeof(mc_ChunkEntityKey)+3*sizeof(uint32_t)+MAX_CHUNK_SALT_SIZE;
    m_KeyDBSize=MC_TDB_TXID_SIZE+sizeof(int)+sizeof(uint32_t)+MC_CDB_CHUNK_HASH_SIZE+sizeof(mc_TxEntity);   // 96
    m_ValueOffset=m_KeySize;
    m_ValueDBOffset=m_KeyDBSize;
    m_ValueSize=sizeof(mc_ChunkEntityValue);
    m_ValueDBSize=6*sizeof(uint32_t)+3*sizeof(int64_t)+MAX_CHUNK_SALT_SIZE;     // 80
    m_TotalSize=m_KeySize+m_ValueSize;
    m_TotalDBSize=m_KeyDBSize+m_ValueDBSize;
    m_Name[0]=0;
    m_DBName[0]=0;
    m_NextAutoCommitTimestamp=0;
    m_NextTryTimestamp=0;
    m_LastKBPerDestinationChangeTimestamp=0;
    m_MarkPool=NULL;
    m_MemPool=NULL;
    m_MemPoolNext=NULL;
    m_MemPool1=NULL;
    m_MemPool2=NULL;
    
    m_MaxMemPoolSize=MC_CCW_DEFAULT_MEMPOOL_SIZE;
    m_AutoCommitDelay=MC_CCW_DEFAULT_AUTOCOMMIT_DELAY;
    m_TimeoutRequest=(int)GetArg("-chunkrequesttimeout",MC_CCW_TIMEOUT_REQUEST);
    if(m_TimeoutRequest <= MC_CCW_TIMEOUT_REQUEST_SHIFT)
    {
        m_TimeoutRequest=MC_CCW_TIMEOUT_REQUEST_SHIFT+1;
    }
    int kb_per_sec=(int)GetArg("-chunkmaxkbpersecond",MC_CCW_MAX_KBS_PER_SECOND);
    
    m_MaxMBPerSecond=1;
    if(kb_per_sec > 1024)
    {
        m_MaxMBPerSecond=(kb_per_sec-1)/1024+1;
    }
    if(m_MaxMBPerSecond <= 1)
    {
        m_MaxMBPerSecond=1;
    }
    if(m_MaxMBPerSecond > MC_CCW_MAX_KBS_PER_SECOND*1024)
    {
        m_MaxMBPerSecond=MC_CCW_MAX_KBS_PER_SECOND*1024;
    }
    m_MaxMaxKBPerDestination=(m_TimeoutRequest-MC_CCW_TIMEOUT_REQUEST_SHIFT)*kb_per_sec;
    m_MinMaxKBPerDestination=(m_TimeoutRequest-MC_CCW_TIMEOUT_REQUEST_SHIFT)*MC_CCW_MIN_KBS_PER_SECOND;
    m_MaxKBPerDestination=m_MaxMaxKBPerDestination;
    
    m_TimeoutQuery=(int)GetArg("-chunkquerytimeout",MC_CCW_TIMEOUT_QUERY);
    m_TotalChunkCount=0;
    m_TotalChunkSize=0;
    
    m_StatLast[0].Zero();
    m_StatLast[1].Zero();
    m_StatTotal[0].Zero();
    m_StatTotal[1].Zero();
    
    m_Semaphore=NULL;
    m_LockedBy=0;     
    
    m_InitMode=0;    
}

int mc_ChunkCollector::Destroy()
{
    if(m_DB)
    {
        m_DB->Close();
        delete m_DB;    
        m_DB=NULL;
    }

    if(m_MarkPool)
    {
        delete m_MarkPool;
    }
    
    if(m_MemPool1)
    {
        delete m_MemPool1;
    }
    
    if(m_MemPool2)
    {
        delete m_MemPool2;
    }

    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
     
    
    Zero();
    return MC_ERR_NOERROR;       
}

int mc_ChunkCollector::Lock(int write_mode,int allow_secondary)
{        
    uint64_t this_thread;
    this_thread=__US_ThreadID();
    
    if(this_thread == m_LockedBy)
    {
        return allow_secondary;
    }
    
    __US_SemWait(m_Semaphore); 
    m_LockedBy=this_thread;
    
    return 0;
}

void mc_ChunkCollector::AdjustKBPerDestination(CNode *pfrom,bool success)
{
    int64_t time_now=GetTimeMillis();
    if(pfrom->nMaxKBPerDestination == 0)
    {
        pfrom->nMaxKBPerDestination=m_MaxKBPerDestination;
    }
    
    if(success)
    {
        if(pfrom->nMaxKBPerDestination >= m_MaxMaxKBPerDestination)
        {
            return;
        }
        if( (time_now - pfrom->nLastKBPerDestinationChangeTimestamp) < MC_CCW_KBS_PER_SECOND_DELAY_UP )
        {
            return;
        }
        pfrom->nMaxKBPerDestination *= 2;
        if(pfrom->nMaxKBPerDestination >= m_MaxMaxKBPerDestination)
        {
            pfrom->nMaxKBPerDestination = m_MaxMaxKBPerDestination;
        }
    }
    else
    {
        if(pfrom->nMaxKBPerDestination <= m_MinMaxKBPerDestination)
        {
            return;
        }
        if( (time_now - pfrom->nLastKBPerDestinationChangeTimestamp) < MC_CCW_KBS_PER_SECOND_DELAY_DOWN )
        {
            return;
        }
        pfrom->nMaxKBPerDestination /= 2;
        if(pfrom->nMaxKBPerDestination <= m_MinMaxKBPerDestination)
        {
            pfrom->nMaxKBPerDestination = m_MinMaxKBPerDestination;
        }        
    }
    pfrom->nLastKBPerDestinationChangeTimestamp=time_now;        
    int max_kb_per_destination=0;
    LogPrintf("Adjusted offchain processing rate for peer %d on %s to %dMB\n",pfrom->id,(success ? "success" : "failure"),pfrom->nMaxKBPerDestination/1024);
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if(pnode->nMaxKBPerDestination > max_kb_per_destination)
            {
                max_kb_per_destination=pnode->nMaxKBPerDestination;
            }
        }        
    }
    if(max_kb_per_destination != m_MaxKBPerDestination)
    {
        m_MaxKBPerDestination=max_kb_per_destination;
        LogPrintf("Adjusted global offchain processing rate on %s to %dMB\n",(success ? "success" : "failure"),m_MaxKBPerDestination/1024);        
    }
}


void mc_ChunkCollector::UnLock()
{    
    m_LockedBy=0;
    __US_SemPost(m_Semaphore);
}

int mc_ChunkCollector::Lock()
{        
    return Lock(1,0);
}

void mc_ChunkCollector::SetDBRow(mc_ChunkCollectorRow* collect_row)
{
    m_DBRow.Zero();
    m_DBRow.m_QueryNextAttempt=mc_SwapBytes32(collect_row->m_DBNextAttempt);
    m_DBRow.m_Vout=collect_row->m_Vout;
    memcpy(m_DBRow.m_TxID,collect_row->m_TxID,MC_TDB_TXID_SIZE);
    memcpy(&(m_DBRow.m_Entity),&(collect_row->m_ChunkDef.m_Entity),sizeof(mc_TxEntity));
    memcpy(m_DBRow.m_Hash,collect_row->m_ChunkDef.m_Hash,MC_CDB_CHUNK_HASH_SIZE);
    m_DBRow.m_Size=collect_row->m_ChunkDef.m_Size;
    m_DBRow.m_Flags=collect_row->m_ChunkDef.m_Flags;
    m_DBRow.m_QueryAttempts=collect_row->m_State.m_QueryAttempts;
    m_DBRow.m_Status=collect_row->m_State.m_Status;
    memcpy(m_DBRow.m_Salt,collect_row->m_Salt,MAX_CHUNK_SALT_SIZE);
    m_DBRow.m_SaltSize=collect_row->m_SaltSize;
    m_DBRow.m_CollectorFlags=collect_row->m_Flags;
}

void mc_ChunkCollector::GetDBRow(mc_ChunkCollectorRow* collect_row)
{
    collect_row->Zero();
    collect_row->m_DBNextAttempt=mc_SwapBytes32(m_DBRow.m_QueryNextAttempt);
    collect_row->m_State.m_QueryNextAttempt=collect_row->m_DBNextAttempt;    
    collect_row->m_Vout=m_DBRow.m_Vout;
    memcpy(collect_row->m_TxID,m_DBRow.m_TxID,MC_TDB_TXID_SIZE);
    memcpy(&(collect_row->m_ChunkDef.m_Entity),&(m_DBRow.m_Entity),sizeof(mc_TxEntity));
    memcpy(collect_row->m_ChunkDef.m_Hash,m_DBRow.m_Hash,MC_CDB_CHUNK_HASH_SIZE);
    collect_row->m_ChunkDef.m_Size=m_DBRow.m_Size;
    collect_row->m_ChunkDef.m_Flags=m_DBRow.m_Flags;
    collect_row->m_State.m_QueryAttempts=m_DBRow.m_QueryAttempts;
    collect_row->m_State.m_Status=m_DBRow.m_Status;
    collect_row->m_State.m_Status |= MC_CCF_INSERTED;                
    memcpy(collect_row->m_Salt,m_DBRow.m_Salt,MAX_CHUNK_SALT_SIZE);
    collect_row->m_SaltSize=m_DBRow.m_SaltSize;
    collect_row->m_Flags=m_DBRow.m_CollectorFlags;
}

int mc_ChunkCollector::DeleteDBRow(mc_ChunkCollectorRow *collect_row)
{       
    uint32_t next_attempt;
    SetDBRow(collect_row);

    next_attempt=m_DBRow.m_QueryNextAttempt;
    if(next_attempt)
    {
        m_DBRow.m_QueryNextAttempt=MC_IMPOSSIBLE_NEXT_ATTEMPT;
        m_DB->Delete((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);    
        m_DBRow.m_QueryNextAttempt=next_attempt;
    }
    return m_DB->Delete((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);    
}

int mc_ChunkCollector::InsertDBRow(mc_ChunkCollectorRow *collect_row)
{
    uint32_t next_attempt;
    uint32_t query_attempts;
    collect_row->m_State.m_Status &= MC_CCF_ERROR_MASK;
    SetDBRow(collect_row);
    collect_row->m_State.m_Status |= MC_CCF_INSERTED;    

    next_attempt=m_DBRow.m_QueryNextAttempt;
    if(next_attempt)
    {
        query_attempts=m_DBRow.m_QueryAttempts;
        m_DBRow.m_QueryNextAttempt=MC_IMPOSSIBLE_NEXT_ATTEMPT;
        m_DBRow.m_QueryAttempts=next_attempt;
        m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        m_DBRow.m_QueryNextAttempt=next_attempt;
        m_DBRow.m_QueryAttempts=query_attempts;
    }

    return m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
}

int mc_ChunkCollector::UpdateDBRow(mc_ChunkCollectorRow *collect_row)
{
    uint32_t next_attempt;
    uint32_t query_attempts;
    int err;    
    collect_row->m_State.m_Status &= MC_CCF_ERROR_MASK;
    SetDBRow(collect_row);

    next_attempt=m_DBRow.m_QueryNextAttempt;
    if(next_attempt)
    {
        m_DBRow.m_QueryNextAttempt=MC_IMPOSSIBLE_NEXT_ATTEMPT;
        m_DB->Delete((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);    
        m_DBRow.m_QueryNextAttempt=next_attempt;
    }

    err=m_DB->Delete((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);    
    if(err)
    {
        return err;
    }
    collect_row->m_DBNextAttempt=collect_row->m_State.m_QueryNextAttempt;
    m_DBRow.m_QueryNextAttempt=mc_SwapBytes32(collect_row->m_DBNextAttempt);
    collect_row->m_State.m_Status |= MC_CCF_INSERTED;    

    next_attempt=m_DBRow.m_QueryNextAttempt;
    if(next_attempt)
    {
        query_attempts=m_DBRow.m_QueryAttempts;
        m_DBRow.m_QueryNextAttempt=MC_IMPOSSIBLE_NEXT_ATTEMPT;
        m_DBRow.m_QueryAttempts=next_attempt;
        m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        m_DBRow.m_QueryNextAttempt=next_attempt;
        m_DBRow.m_QueryAttempts=query_attempts;
    }

    return m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
}

int mc_ChunkCollector::SeekDB(void *dbrow)
{
    int err,value_len;   
    unsigned char *ptr;
    
    err=MC_ERR_NOERROR;
    ptr=(unsigned char*)m_DB->Read((char*)dbrow+m_KeyDBOffset,m_KeyDBSize,&value_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(ptr==NULL)
    {
        return MC_ERR_NOT_FOUND;
    }
    
    if(value_len != (int)m_ValueDBSize)
    {
        return MC_ERR_NOT_SUPPORTED;
    }
    
    memcpy((unsigned char*)&m_DBRow+m_KeyDBOffset,(unsigned char*)dbrow,m_KeyDBSize);
    memcpy((unsigned char*)&m_DBRow+m_ValueDBOffset,ptr,m_ValueDBSize);
    memcpy(&m_LastDBRow,&m_DBRow,m_TotalDBSize);
    
    return err;
}

int mc_ChunkCollector::UpgradeDB()
{
    int err,value_len;   
    unsigned char *ptr;
    int diff,count;

    err=MC_ERR_NOERROR;
    ptr=(unsigned char*)m_DB->Read((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,&value_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    
    diff=m_ValueDBSize-value_len;
    if(diff <= 0)
    {
        return MC_ERR_CORRUPTED;
    }
    
    count=0;
    m_DBRow.Zero();
    while(ptr)
    {
        ptr=(unsigned char*)m_DB->MoveNext(&err);
        if(ptr)
        {
            memcpy((char*)&m_DBRow+m_KeyDBOffset,ptr,m_TotalDBSize-diff);            
            
            m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            count++;
            
            if(count >= 1000)
            {
                err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
                if(err)
                {
                    return err;
                }                                        
            }
        }
    }
    
    m_DBRow.Zero();
    ptr=(unsigned char*)m_DB->Read((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,&value_len,0,&err);
    memcpy((unsigned char*)&m_DBRow+m_ValueDBOffset,ptr,value_len);
    m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
    err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
    if(err)
    {
        return err;
    }                                        
    
    
    m_DBRow.Zero();    
    err=SeekDB(&m_DBRow);
    
    return err;
}

int mc_ChunkCollector::ReadFromDB(mc_Buffer *mempool,int rows)
{
    int err,mprow;   
    mc_ChunkCollectorRow collect_row;
    mc_ChunkDBRow chunk_def;
    unsigned char *ptr;
    int row;
    
    if(rows <= 0)    
    {
        return MC_ERR_NOERROR;
    }
    
    err=MC_ERR_NOERROR;
    
    ptr=NULL;
    row=mempool->GetCount();
    while(row<rows)
    {
        ptr=(unsigned char*)m_DB->MoveNext(&err);
        if(err)
        {
            return MC_ERR_CORRUPTED;            
        }
        if(ptr)
        {
            memcpy((char*)&m_DBRow,ptr,m_TotalDBSize);   
            if(m_DBRow.m_QueryNextAttempt != MC_IMPOSSIBLE_NEXT_ATTEMPT)
            {
                GetDBRow(&collect_row);
                collect_row.m_State.m_Status |= MC_CCF_INSERTED;                
                mprow=mempool->Seek(&collect_row);
                if(mprow < 0)
                {
                    if(m_ChunkDB->GetChunkDefWithLimit(&chunk_def,collect_row.m_ChunkDef.m_Hash,&(collect_row.m_ChunkDef.m_Entity),collect_row.m_TxID,collect_row.m_Vout,
                            MC_CCW_MAX_ITEMS_PER_CHUNKFOR_CHECK) == MC_ERR_NOERROR)
                    {
                        collect_row.m_State.m_Status |= MC_CCF_DELETED;
                    }
                    mempool->Add(&collect_row);
                    row++;
                }
            }
        }
        else
        {
            row=rows;
        }
    }
    
    if(mempool->GetCount() < rows)
    {
        if(mempool->GetCount() <= m_TotalChunkCount)
        {
            m_TotalChunkCount=mempool->GetCount();
            m_TotalChunkSize=0;
            for(row=0;row<m_MemPool->GetCount();row++)
            {
                m_TotalChunkSize+=((mc_ChunkCollectorRow *)m_MemPool->GetRow(row))->m_ChunkDef.m_Size;
            }
        }
    }
    
    memcpy(&m_LastDBRow,&m_DBRow,m_TotalDBSize);
    
    return MC_ERR_NOERROR;
}


void mc_ChunkCollector::Dump(const char *message)
{   
    int i;
    
    if((m_InitMode & MC_WMD_DEBUG) == 0)
    {
        return;
    }
    unsigned char *ptr;
    int dbvalue_len,err;
    char ShortName[65];                                     
    char FileName[MC_DCT_DB_MAX_PATH];                      
    FILE *fHan;
    
    sprintf(ShortName,"chunks/collect");
    mc_GetFullFileName(m_Name,ShortName,".dmp",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,FileName);
    
    fHan=fopen(FileName,"a");
    if(fHan == NULL)
    {
        return;
    }

    mc_LogString(fHan,message);     
    
    if(m_DB)
    {
        fprintf(fHan,"\nDB\n");
        m_DBRow.Zero();    
        ptr=(unsigned char*)m_DB->Read((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
        if(err)
        {
            return;
        }

        if(ptr)
        {
            memcpy((char*)&m_DBRow+m_ValueDBOffset,ptr,m_ValueDBSize);
            while(ptr)
            {
                mc_MemoryDumpCharSizeToFile(fHan,(char*)&m_DBRow+m_KeyDBOffset,0,m_TotalDBSize,64);        
                ptr=(unsigned char*)m_DB->MoveNext(&err);
                if(ptr)
                {
                    memcpy((char*)&m_DBRow+m_KeyDBOffset,ptr,m_TotalDBSize);            
                }
            }
        }
    }

    fprintf(fHan,"\nMempool\n");
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        mc_MemoryDumpCharSizeToFile(fHan,m_MemPool->GetRow(i),0,m_TotalSize,64);    
    }
    
    fprintf(fHan,"\n<<<<<< \tChain height: %6d\t%s\n\n",mc_gState->m_Permissions->m_Block,message);
    fclose(fHan);
}

int mc_ChunkCollector::Initialize(mc_ChunkDB *chunk_db,const char *name,uint32_t mode)
{
    int err=MC_ERR_NOERROR;    
    if(name)
    {
        strcpy(m_Name,name);

        m_DB=new mc_Database;

        mc_GetFullFileName(name,"chunks/collect",".db",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_DBName);

        m_DB->SetOption("KeySize",0,m_KeyDBSize);
        m_DB->SetOption("ValueSize",0,m_ValueDBSize);


        err=m_DB->Open(m_DBName,MC_OPT_DB_DATABASE_CREATE_IF_MISSING | MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_LEVELDB | MC_OPT_DB_DATABASE_THREAD_SAFE);

        if(err)
        {
            return err;
        }
    }

    m_InitMode=mode;
    m_ChunkDB=chunk_db;
    
    
    m_MarkPool=new mc_Buffer;                                                
    err=m_MarkPool->Initialize(m_KeySize,m_TotalSize,MC_BUF_MODE_DEFAULT);
    m_MemPool1=new mc_Buffer;                                                
    err=m_MemPool1->Initialize(m_KeySize,m_TotalSize,MC_BUF_MODE_MAP);
    m_MemPool2=new mc_Buffer;                                                
    err=m_MemPool2->Initialize(m_KeySize,m_TotalSize,MC_BUF_MODE_MAP);
    
    m_MemPool=m_MemPool1;

    m_DBRow.Zero();
    
    if(m_DB)
    {
        err=SeekDB(&m_DBRow);
        if(err == MC_ERR_NOT_SUPPORTED)
        {
            err=UpgradeDB();
        }
        
        if(err)
        {
            if(err != MC_ERR_NOT_FOUND)
            {
                return err;
            }
        }
        if(err != MC_ERR_NOT_FOUND)
        {
            m_TotalChunkCount=m_DBRow.m_TotalChunkCount;
            m_TotalChunkSize=m_DBRow.m_TotalChunkSize;
            if(m_TotalChunkCount < 0)
            {
                m_TotalChunkCount=0;
                m_TotalChunkSize=0;
            }
            err=ReadFromDB(m_MemPool,m_MaxMemPoolSize);
            if(err)
            {
                return err;
            }
        }
        else
        {
            err=m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
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
    }
    
    m_Semaphore=__US_SemCreate();
    if(m_Semaphore == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }    
    
    Dump("Initialize");
    
    return err;   
}

int mc_ChunkCollector::InsertChunk(                                                            // Adds chunk to mempool
                 const unsigned char *hash,                                     // Chunk hash (before chopping)    
                 const mc_TxEntity *entity,                                     // Parent entity
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size,
                 const uint32_t salt_size,
                 const uint32_t flags)
{
    int err;
    
    Lock();
    err=InsertChunkInternal(hash,entity,txid,vout,chunk_size,salt_size,flags);
    UnLock();
    
    return err;    
}

int mc_ChunkCollector::Unsubscribe(mc_Buffer* lpEntities)
{
    Lock();
    
    int i,err;    
    unsigned char *ptr;
    mc_ChunkCollectorRow *row;
    mc_ChunkCollectorRow collect_row;
    int try_again,commit_required;
    mc_TxEntity entity;
    
    err=MC_ERR_NOERROR;
    
    commit_required=0;
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(i);        
        memcpy(&entity,&row->m_ChunkDef.m_Entity,sizeof(mc_TxEntity));
        entity.m_EntityType |= MC_TET_CHAINPOS;
        if(lpEntities->Seek(&entity) >= 0)    
        {
            row->m_State.m_Status |= MC_CCF_DELETED;
            commit_required=1;
        }        
    }
    
    if(commit_required)
    {
        CommitInternal(0);            
    }
    
    if(m_MemPool == m_MemPool1)
    {
        m_MemPoolNext=m_MemPool2;
    }
    else
    {
        m_MemPoolNext=m_MemPool1;        
    }
    
    
    try_again=1;
    m_DBRow.Zero();    
    if(m_DB)
    {        
        while(try_again)
        {
            m_MemPoolNext->Clear();
            try_again=0;
            err=SeekDB(&m_DBRow);

            if(err)
            {
                goto exitlbl;
            }
            
            ptr=(unsigned char*)m_DB->MoveNext(&err);
            while(ptr)
            {
                if(err)
                {
                    goto exitlbl;         
                }
                memcpy((char*)&m_DBRow,ptr,m_TotalDBSize);   
                ptr=(unsigned char*)m_DB->MoveNext(&err);
                GetDBRow(&collect_row);
                memcpy(&entity,&collect_row.m_ChunkDef.m_Entity,sizeof(mc_TxEntity));
                entity.m_EntityType |= MC_TET_CHAINPOS;
                if(lpEntities->Seek(&entity) >= 0)    
                {
                    m_MemPoolNext->Add(&collect_row);
                }
                else
                {
                    memcpy(&m_LastDBRow,&m_DBRow,m_TotalDBSize);                       
                }
                if( (m_MemPoolNext->GetCount() >= 2*m_MaxMemPoolSize) || (ptr == NULL) )
                {
                    if(ptr)
                    {
                        try_again=1;
                        ptr=NULL;
                        memcpy(&m_DBRow,&m_LastDBRow,m_TotalDBSize);                        
                    }
                    if(m_MemPoolNext->GetCount())
                    {
                        commit_required=1;
                        for(i=0;i<m_MemPoolNext->GetCount();i++)
                        {
                            row=(mc_ChunkCollectorRow *)m_MemPoolNext->GetRow(i);        
                            m_TotalChunkCount--;
                            m_TotalChunkSize-=row->m_ChunkDef.m_Size;
                            if(row->m_State.m_Status & MC_CCF_INSERTED)
                            {
                                DeleteDBRow(row);                
                            }            
                        }
                        m_DBRow.Zero();
                        m_DBRow.m_TotalChunkSize=m_TotalChunkSize;
                        m_DBRow.m_TotalChunkCount=m_TotalChunkCount;
                        m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);

                        err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
                        if(err)
                        {
                            goto exitlbl;
                        }                                        
                    }
                }                
            }
        }        
    }
    
    m_MemPoolNext->Clear();    
    
    if(commit_required)
    {
        CommitInternal(1);    
    }
    
exitlbl:

    UnLock();    
    
    return err;
}

int mc_ChunkCollector::InsertChunkInternal(                  
                 const unsigned char *hash,   
                 const mc_TxEntity *entity,   
                 const unsigned char *txid,
                 const int vout,
                 const uint32_t chunk_size,
                 const uint32_t salt_size,
                 const uint32_t flags)
{
    mc_ChunkCollectorRow collect_row;
    int mprow;
    
    collect_row.Zero();
    memcpy(collect_row.m_ChunkDef.m_Hash,hash,MC_CDB_CHUNK_HASH_SIZE);
    memcpy(&collect_row.m_ChunkDef.m_Entity,entity,sizeof(mc_TxEntity));
    memcpy(collect_row.m_TxID,txid,MC_TDB_TXID_SIZE);
    collect_row.m_Vout=vout;
    collect_row.m_ChunkDef.m_Size=chunk_size;
    collect_row.m_State.m_Status=MC_CCF_NEW;
    collect_row.m_Flags=flags;
    collect_row.m_SaltSize=salt_size;
    memset(collect_row.m_Salt,0,MAX_CHUNK_SALT_SIZE);
    
    mprow=m_MemPool->Seek(&collect_row);
    if(mprow<0)
    {
        m_MemPool->Add(&collect_row);
        m_TotalChunkCount++;
        m_TotalChunkSize+=chunk_size;
        
        if(m_MemPool->GetCount() >= 2*m_MaxMemPoolSize)
        {
            CommitInternal(0);
        }
    }
    
    return MC_ERR_NOERROR;
}
    
int mc_ChunkCollector::MarkAndClear(uint32_t flag, int unmark)
{
    int i,mprow;
    
    mc_ChunkCollectorRow *row;
    Lock();    
    
    for(i=0;i<m_MarkPool->GetCount();i++)
    {
        mprow=m_MemPool->Seek(m_MarkPool->GetRow(i));
        if(mprow >= 0)
        {
            row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(mprow);
            if(unmark)
            {
                row->m_State.m_Status &= ~flag;
            }
            else
            {
                row->m_State.m_Status |= flag;
            }
        }
    }
    
    m_MarkPool->Clear();
    UnLock();    
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::CopyFlags()
{
    int i,mprow;
    
    mc_ChunkCollectorRow *row;
    mc_ChunkCollectorRow *mark_row;
    Lock();    
    
    for(i=0;i<m_MarkPool->GetCount();i++)
    {
        mark_row=(mc_ChunkCollectorRow *)m_MarkPool->GetRow(i);
        mprow=m_MemPool->Seek(mark_row);
        if(mprow >= 0)
        {
            row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(mprow);
            row->m_State.m_Status=mark_row->m_State.m_Status;
        }
    }
    
    m_MarkPool->Clear();
    UnLock();    
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::FillMarkPoolByHash(const unsigned char *hash)
{
    int i;    
    mc_ChunkCollectorRow *row;
    
    Lock();
    
    m_MarkPool->Clear();
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(i);        
        if(memcmp(row->m_ChunkDef.m_Hash,hash,MC_CDB_CHUNK_HASH_SIZE) == 0)
        {
            m_MarkPool->Add(row);
        }
    }
    
    UnLock();        
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::FillMarkPoolByFlag(uint32_t flag, uint32_t not_flag)
{
    int i;    
    mc_ChunkCollectorRow *row;
    
    Lock();
    
    m_MarkPool->Clear();
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(i);        
        if(row->m_State.m_Status & flag)
        {
            if( (row->m_State.m_Status & not_flag) == 0)
            {
                m_MarkPool->Add(row);
            }
        }
    }
    
    UnLock();        
    
    return MC_ERR_NOERROR;
}

int mc_ChunkCollector::Commit()
{
    int err;
    
    Lock();
    err=CommitInternal(1);
    UnLock();
    
    return err;        
}

int mc_ChunkCollector::CommitInternal(int fill_mempool)
{
    int i;    
    mc_ChunkCollectorRow *row;
    int err,commit_required;
    uint32_t time_now;

    err=MC_ERR_NOERROR;
    
    if(m_DB == NULL)
    {
        return MC_ERR_NOT_ALLOWED;
    }
    
    Dump("Before Commit");
    
    time_now=mc_TimeNowAsUInt();
    
    if(m_MemPool == m_MemPool1)
    {
        m_MemPoolNext=m_MemPool2;
    }
    else
    {
        m_MemPoolNext=m_MemPool1;        
    }
    
    m_MemPoolNext->Clear();
    
    commit_required=0;
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        row=(mc_ChunkCollectorRow *)m_MemPool->GetRow(i);        
        
        if(row->m_State.m_Status & MC_CCF_DELETED)
        {
            m_TotalChunkCount--;
            m_TotalChunkSize-=row->m_ChunkDef.m_Size;
            if(row->m_State.m_Status & MC_CCF_INSERTED)
            {
                commit_required=1;
                DeleteDBRow(row);                
//                m_DB->Delete((char*)row+m_KeyOffset,m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            }            
        }
        else
        {
            if( (row->m_State.m_Status & MC_CCF_INSERTED) == 0 )
            {
                commit_required=1;
                InsertDBRow(row);
            }
            else
            {
                if(row->m_State.m_Status & MC_CCF_UPDATED)
                {
                    commit_required=1;
                    UpdateDBRow(row);                
                }
            }
/*            
            if( ((row->m_State.m_Status & MC_CCF_INSERTED) == 0 ) || (row->m_State.m_Status & MC_CCF_UPDATED) )
            {
                
                row->m_State.m_Status &= MC_CCF_ERROR_MASK;
                m_DB->Write((char*)row+m_KeyOffset,m_KeySize,(char*)row+m_ValueOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                row->m_State.m_Status |= MC_CCF_INSERTED;
            }            
 */ 
            if(!row->m_State.m_Query.IsZero() || 
               ((fill_mempool != 0) && (row->m_State.m_QueryNextAttempt <= time_now) && (m_MemPoolNext->GetCount() < m_MaxMemPoolSize)) )
            {
                    m_MemPoolNext->Add(row);                                    
            }
        }
    }    

    if(commit_required)
    {
        m_DBRow.Zero();
        m_DBRow.m_TotalChunkSize=m_TotalChunkSize;
        m_DBRow.m_TotalChunkCount=m_TotalChunkCount;
        m_DB->Write((char*)&m_DBRow+m_KeyDBOffset,m_KeyDBSize,(char*)&m_DBRow+m_ValueDBOffset,m_ValueDBSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        
        err=m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return err;
        }                
    }
   
    if(fill_mempool)
    {
        if(m_MemPoolNext->GetCount() < m_MaxMemPoolSize)
        {
            m_LastDBRow.Zero();
            err=SeekDB(&m_LastDBRow);
            if(err == MC_ERR_NOERROR)
            {
                ReadFromDB(m_MemPoolNext,m_MaxMemPoolSize);
            }
        }
    }

    m_MemPool->Clear();
    m_MemPool=m_MemPoolNext;
    
    Dump("Commit");
    
    return err;
}

uint32_t MultichainNextChunkQueryAttempt(uint32_t attempts)
{
    if(attempts <  2)return 0;
    return (uint32_t)(int64_t)(pow(1.5,attempts-1)-1);
}

int mc_IsReadPermissionedStream(mc_ChunkEntityKey* chunk,map<uint160,int>& cache,set<CPubKey>* sAddressesToSign)
{
    if(chunk->m_Entity.m_EntityType != MC_TET_STREAM)
    {
        return -1;
    }    
    
    unsigned char *ptr=chunk->m_Entity.m_EntityID;
    uint160 enthash=*(uint160*)ptr;
    map<uint160, int>::const_iterator it = cache.find(enthash);
    if(it != cache.end())
    {
        return it->second;        
    }

    mc_EntityDetails entity;
    int result=0;
    
    {
        LOCK(cs_main);                                                          // possible caching improvement here
        if(mc_gState->m_Assets->FindEntityByShortTxID(&entity,chunk->m_Entity.m_EntityID) == 0)
        {
            result=-2;
        }               
    }
    
    if(result)
    {
        return result;        
    }
    
    result=0;
    
    if(entity.AnyoneCanRead() == 0)
    {
        result=1;
        if(sAddressesToSign)
        {
            CKeyID keyID;
            set<CPubKey>::iterator it;
            for (it = sAddressesToSign->begin(); it !=  sAddressesToSign->end(); ++it)
            {
                if(result)
                {
                    keyID=(*it).GetID();
                    if(mc_gState->m_Permissions->CanRead(chunk->m_Entity.m_EntityID,(unsigned char*)(&keyID)))
                    {
                        result=0;
                    }
                }
            }
            if(result)
            {
                CPubKey pubkey=pEF->WLT_FindReadPermissionedAddress(&entity);
                if(pubkey.IsValid())
                {
                    sAddressesToSign->insert(pubkey);
                    result=0;                
                }                
            }
        }
    }
    
    cache.insert(make_pair(enthash,result));
    return result;    
}

bool MultichainProcessChunkResponse(const CRelayResponsePair *response_pair,map <int,int>* request_pairs,mc_ChunkCollector* collector)
{
    mc_RelayRequest *request;
    mc_RelayResponse *response;
    map<uint160,int> mapReadPermissionCache;
    request=pRelayManager->FindRequest(response_pair->request_id);
    if(request == NULL)
    {
        return false;
    }
    response=&(request->m_Responses[response_pair->response_id]);
    
    unsigned char *ptr;
    unsigned char *ptrEnd;
    unsigned char *ptrStart;
    int shift,count,size;
    int shiftOut,countOut,sizeOut;
    int chunk_err;
    mc_ChunkEntityKey *chunk;
    mc_ChunkEntityKey *chunkOut;
    unsigned char *ptrOut;
    unsigned char *ptrOutEnd;
    bool result=false;
    string strError="";
    mc_ChunkCollectorRow *collect_row;
        
    uint32_t total_size=0;
    ptrStart=&(request->m_Payload[0]);
    
    size=sizeof(mc_ChunkEntityKey);
    shift=0;
    count=0;
    
    ptr=ptrStart;
    ptrEnd=ptr+request->m_Payload.size();
        
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_EXPIRATION:
                ptr+=5;
                break;
            case MC_RDT_CHUNK_IDS:
                ptr++;
                count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                ptr+=shift;
                if(count*size != (ptrEnd-ptr))
                {
                    goto exitlbl;
                }                
                for(int c=0;c<count;c++)
                {
                    if(mc_IsReadPermissionedStream((mc_ChunkEntityKey*)ptr,mapReadPermissionCache,NULL))
                    {
                        result=pEF->OFF_ProcessChunkResponse(request,response,request_pairs,collector,strError);
                        goto exitlbl;
                    }
                    total_size+=((mc_ChunkEntityKey*)ptr)->m_Size+size;
                    ptr+=size;
                }
                break;
            default:
                result=pEF->OFF_ProcessChunkResponse(request,response,request_pairs,collector,strError);
                goto exitlbl;
        }
    }
    

    if(response->m_Payload.size() < 1+shift+total_size)
    {
        strError="Total size mismatch";
        goto exitlbl;        
    }

    ptrOut=&(response->m_Payload[0]);
    ptrOutEnd=ptrOut+response->m_Payload.size();
    if(*ptrOut != MC_RDT_CHUNKS)
    {
        strError="Unsupported payload format";
        goto exitlbl;                
    }
    
    ptrOut++;
    countOut=(int)mc_GetVarInt(ptrOut,1+shift+total_size,-1,&shiftOut);
    if( (countOut != count) || (shift != shiftOut) )
    {
        strError="Chunk count mismatch";
        goto exitlbl;                        
    }
    ptrOut+=shift;
    
    ptr=ptrStart+1+shift+5;
    for(int c=0;c<count;c++)
    {
        sizeOut=((mc_ChunkEntityKey*)ptr)->m_Size;
        chunk=(mc_ChunkEntityKey*)ptr;
        chunkOut=(mc_ChunkEntityKey*)ptrOut;
        if(ptrOutEnd - ptrOut < size)
        {
            strError="Total size mismatch";
            goto exitlbl;                                                    
        }
        ptrOut+=size;
        if(chunk->m_Size != chunkOut->m_Size)
        {
            for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? chunk->m_Size : 1;                
            strError="Chunk info size mismatch";
            goto exitlbl;                                        
        }
        if(memcmp(chunk->m_Hash,chunkOut->m_Hash,sizeof(uint256)))
        {
            for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? chunk->m_Size : 1;                
            strError="Chunk info hash mismatch";
            goto exitlbl;                                                    
        }
        if(memcmp(&(chunk->m_Entity),&(chunkOut->m_Entity),sizeof(mc_TxEntity)))
        {
            for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? chunk->m_Size : 1;                
            strError="Chunk info entity mismatch";
            goto exitlbl;                                                    
        }
        sizeOut=chunk->m_Size;
        map <int,int>::iterator itreq = request_pairs->find(c);
        if (itreq != request_pairs->end())
        {
            collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(itreq->second);
            if(ptrOutEnd - ptrOut < collect_row->m_SaltSize+sizeOut)
            {
                strError="Total size mismatch";
                goto exitlbl;                                                    
            }
            if(collect_row->m_SaltSize)
            {
                memcpy(collect_row->m_Salt,ptrOut,collect_row->m_SaltSize);
                ptrOut+=collect_row->m_SaltSize;
            }
            uint256 hash;
//            mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(ptrOut,sizeOut,&hash);
            mc_gState->m_TmpBuffers->m_RpcHasher1->DoubleHash(collect_row->m_Salt,collect_row->m_SaltSize,ptrOut,sizeOut,&hash);
            if(memcmp(&hash,chunk->m_Hash,sizeof(uint256)))
            {
                for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Baddelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                strError="Chunk data hash mismatch";
                goto exitlbl;                                        
            }
            if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
            {
                chunk_err=pwalletTxsMain->m_ChunkDB->AddChunk(chunk->m_Hash,&(chunk->m_Entity),(unsigned char*)collect_row->m_TxID,collect_row->m_Vout,
                        ptrOut,NULL,collect_row->m_Salt,sizeOut,0,collect_row->m_SaltSize,collect_row->m_Flags);
                if(chunk_err)
                {
                    if(chunk_err != MC_ERR_FOUND)
                    {
                        strError=strprintf("Internal chunk DB error: %d",chunk_err);
                        goto exitlbl;                    
                    }
                }
                else
                {
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Delivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    unsigned char* ptrhash=chunk->m_Hash;
                    if(fDebug)LogPrint("chunks","Retrieved chunk %s\n",(*(uint256*)ptrhash).ToString().c_str());                
                }
            }
            collect_row->m_State.m_Status |= MC_CCF_DELETED;
        }        
        
        ptr+=size;
        ptrOut+=sizeOut;
    }
    
    result=true;
    
exitlbl:
                
    pRelayManager->UnLock();
                
    if(strError.size())
    {
        if(fDebug)LogPrint("chunks","Bad response from peer %d: %s\n",response->m_NodeFrom,strError.c_str());
    }
    return result;
}

int MultichainResponseScore(mc_RelayResponse *response,mc_ChunkCollectorRow *collect_row,map<int64_t,int64_t>& destination_loads,uint32_t max_total_size)
{
    unsigned char *ptr;
    unsigned char *ptrEnd;
    unsigned char *ptrStart;
    int shift,count,size;
    int64_t total_size;
    mc_ChunkEntityKey *chunk;
    int c;
    if( (response->m_Status & MC_RST_SUCCESS) == 0 )
    {
        return MC_CCW_WORST_RESPONSE_SCORE;
    }

    total_size=0;
    map<int64_t,int64_t>::iterator itdld = destination_loads.find(response->SourceID());
    if (itdld != destination_loads.end())
    {
        total_size=itdld->second;
    }                                    
    
    if(total_size + collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey) > max_total_size)
    {
        return MC_CCW_WORST_RESPONSE_SCORE;                
    }
    
    ptrStart=&(response->m_Payload[0]);
    
    size=sizeof(mc_ChunkEntityKey);
    shift=0;
    count=0;
    
    ptr=ptrStart;
    ptrEnd=ptr+response->m_Payload.size();
    if(*ptr == MC_RDT_ENTERPRISE_FEATURES)
    {
        if(mc_gState->m_Features->ReadPermissions())
        {
            ptr++;
            int length=mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
            ptr+=shift;
            ptr+=length;
        }
        else
        {
            return MC_CCW_WORST_RESPONSE_SCORE;                    
        }
    }
    ptr++;
    count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
    ptr+=shift;
    
    c=0;
    while(c<count)
    {
        chunk=(mc_ChunkEntityKey*)ptr;
        if( (memcmp(chunk->m_Hash,collect_row->m_ChunkDef.m_Hash,MC_CDB_CHUNK_HASH_SIZE) == 0) && 
            (memcmp(&(chunk->m_Entity),&(collect_row->m_ChunkDef.m_Entity),sizeof(mc_TxEntity)) == 0))
        {
            if(chunk->m_Flags & MC_CCF_ERROR_MASK)
            {
                return MC_CCW_WORST_RESPONSE_SCORE;
            }
            c=count+1;
        }
        ptr+=size;
        c++;
    }
    if(c == count)
    {
        return MC_CCW_WORST_RESPONSE_SCORE;        
    }
        
    return (response->m_TryCount+response->m_HopCount)*1024*1024+total_size/1024;
}

int MultichainCollectChunks(mc_ChunkCollector* collector)
{
    uint32_t time_now,expiration,dest_expiration;    
    vector <mc_ChunkEntityKey> vChunkDefs;
    int row,last_row,last_count,to_end_of_query;
    uint32_t total_size,max_total_query_size,max_total_destination_size,total_in_queries,max_total_in_queries,query_count;
    mc_ChunkCollectorRow *collect_row;
    mc_ChunkCollectorRow *collect_subrow;
    time_now=mc_TimeNowAsUInt();
    vector<unsigned char> payload;
    unsigned char buf[16];
    int shift,count;
    unsigned char *ptrOut;
    mc_OffchainMessageID query_id,request_id;
    map <mc_OffchainMessageID,bool> query_to_delete;
    map <CRelayResponsePair,CRelayRequestPairs> requests_to_send;    
    map <CRelayResponsePair,CRelayRequestPairs> responses_to_process;    
    map <int64_t,int64_t> destination_loads;    
    mc_RelayRequest *request;
    mc_RelayRequest *query;
    mc_RelayResponse *response;
    CRelayResponsePair response_pair;
    vector<int> vRows;
    CRelayRequestPairs request_pairs;
    int best_score,best_response,this_score,not_processed;
    map<uint160,int> mapReadPermissionCache;
    set<CPubKey> sAddressesToSign;
    
    pRelayManager->CheckTime();
    pRelayManager->InvalidateResponsesFromDisconnected();
    
    collector->Lock();

    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(collect_row->m_State.m_RequestTimeStamp <= time_now)
            {
                if(!collect_row->m_State.m_Request.IsZero())
                {
                    pRelayManager->DeleteRequest(collect_row->m_State.m_Request);
                    collect_row->m_State.m_Request=0;                    
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                }                
            }
            request=NULL;
            if(!collect_row->m_State.m_Request.IsZero())
            {
                request=pRelayManager->FindRequest(collect_row->m_State.m_Request);
                if(request == NULL)
                {
                    collect_row->m_State.m_Request=0;
                    collect_row->m_State.m_RequestTimeStamp=0;
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                                    
                }
            }
            if(request)
            {
                if(request->m_Responses.size())
                {
                    response_pair.request_id=collect_row->m_State.m_Request;
                    response_pair.response_id=0;
//                    printf("coll new rsp: row: %d, id: %s, %d\n",row,collect_row->m_State.m_Request.ToString().c_str(),collect_row->m_State.m_RequestPos);
                    map<CRelayResponsePair,CRelayRequestPairs>::iterator itrsp = responses_to_process.find(response_pair);
                    if (itrsp == responses_to_process.end())
                    {
                        request_pairs.m_Pairs.clear();
                        request_pairs.m_Pairs.insert(make_pair(collect_row->m_State.m_RequestPos,row));
                        responses_to_process.insert(make_pair(response_pair,request_pairs));
                    }       
                    else
                    {
                        itrsp->second.m_Pairs.insert(make_pair(collect_row->m_State.m_RequestPos,row));
                    }                    
                }            
                pRelayManager->UnLock();
            }
        }        
    }

    BOOST_FOREACH(PAIRTYPE(const CRelayResponsePair, CRelayRequestPairs)& item, responses_to_process)    
    {
        MultichainProcessChunkResponse(&(item.first),&(item.second.m_Pairs),collector);
        pRelayManager->DeleteRequest(item.first.request_id);
    }


    max_total_destination_size=collector->m_MaxKBPerDestination*1024;
//    max_total_size/=MC_CCW_QUERY_SPLIT;
/*    
    if(max_total_destination_size > MAX_SIZE-OFFCHAIN_MSG_PADDING)
    {
        max_total_destination_size=MAX_SIZE-OFFCHAIN_MSG_PADDING;        
    }
    
    if(max_total_size < MAX_CHUNK_SIZE + sizeof(mc_ChunkEntityKey))
    {
        max_total_size = MAX_CHUNK_SIZE + sizeof(mc_ChunkEntityKey);
    }
 */ 
    max_total_query_size=MAX_CHUNK_SIZE + sizeof(mc_ChunkEntityKey);
    if(max_total_destination_size<max_total_query_size)
    {
        max_total_destination_size=max_total_query_size;
    }
    
    max_total_in_queries=collector->m_MaxKBPerDestination*1024;
    max_total_in_queries*=collector->m_TimeoutRequest;
    total_in_queries=0;
    query_count=0;
    
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            request=NULL;
            if(!collect_row->m_State.m_Request.IsZero())
            {
                request=pRelayManager->FindRequest(collect_row->m_State.m_Request);
                if(request == NULL)
                {
                    collect_row->m_State.m_Request=0;
                    collect_row->m_State.m_RequestTimeStamp=0;
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                }
            }
            if(request)
            {
                map<int64_t,int64_t>::iterator itdld = destination_loads.find(request->m_DestinationID);
                if (itdld == destination_loads.end())
                {
                    destination_loads.insert(make_pair(request->m_DestinationID,collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey)));
                }       
                else
                {
                    itdld->second+=collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey);
                }  
                pRelayManager->UnLock();
            }
        }
        else
        {
            if(!collect_row->m_State.m_Query.IsZero())
            {
                map<mc_OffchainMessageID, bool>::iterator itqry = query_to_delete.find(collect_row->m_State.m_Query);
                if (itqry == query_to_delete.end())
                {
                    query_to_delete.insert(make_pair(collect_row->m_State.m_Query,true));
                }       
            }            
        }
    }
    
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(collect_row->m_State.m_Request.IsZero())
            {
                query=NULL;
                if(!collect_row->m_State.m_Query.IsZero())
                {
                    query=pRelayManager->FindRequest(collect_row->m_State.m_Query);
                    if(query == NULL)
                    {
                        collect_row->m_State.m_Query=0;
                        collect_row->m_State.m_QueryNextAttempt=time_now+MultichainNextChunkQueryAttempt(collect_row->m_State.m_QueryAttempts);                                                
                        collect_row->m_State.m_Status |= MC_CCF_UPDATED;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Unresponded+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    }
                }
                if(query)
                {
                    best_response=-1;
                    best_score=MC_CCW_WORST_RESPONSE_SCORE;
                    for(int i=0;i<(int)query->m_Responses.size();i++)
                    {
                        this_score=MultichainResponseScore(&(query->m_Responses[i]),collect_row,destination_loads,max_total_destination_size);
                        if(this_score < best_score)
                        {
                            best_score=this_score;
                            best_response=i;
                        }
                    }
                    if(best_response >= 0)
                    {
                        response_pair.request_id=collect_row->m_State.m_Query;
                        response_pair.response_id=best_response;                        
                        map<CRelayResponsePair,CRelayRequestPairs>::iterator itrsp = requests_to_send.find(response_pair);                                                
                        if (itrsp == requests_to_send.end())
                        {                            
//                    printf("coll new req: row: %d, id:  %s, rsps: %d, score (%d,%d)\n",row,collect_row->m_State.m_Query.ToString().c_str(),(int)query->m_Responses.size(),best_score,best_response);
                            request_pairs.m_Pairs.clear();
                            request_pairs.m_Pairs.insert(make_pair(row,0));
                            requests_to_send.insert(make_pair(response_pair,request_pairs));
                        }       
                        else
                        {
//                    printf("coll old req: row: %d, id:  %s, rsps: %d, score (%d,%d)\n",row,collect_row->m_State.m_Query.ToString().c_str(),(int)query->m_Responses.size(),best_score,best_response);
                            itrsp->second.m_Pairs.insert(make_pair(row,0));
                        }                    
                        map<int64_t,int64_t>::iterator itdld = destination_loads.find(query->m_Responses[best_response].SourceID());
                        if (itdld == destination_loads.end())
                        {
                            destination_loads.insert(make_pair(query->m_Responses[best_response].SourceID(),collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey)));
                        }       
                        else
                        {
                            itdld->second+=collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey);
                        }                                    
                    }
                    pRelayManager->UnLock();
                }
            }
            if(!collect_row->m_State.m_Query.IsZero())
            {
                to_end_of_query=collect_row->m_State.m_QueryTimeStamp-time_now;
                if(to_end_of_query<0)to_end_of_query=0;
                if(to_end_of_query>collector->m_TimeoutRequest)to_end_of_query=collector->m_TimeoutRequest;
                total_in_queries+=collect_row->m_ChunkDef.m_Size*to_end_of_query;
            }
        }        
    }
    
    BOOST_FOREACH(PAIRTYPE(const CRelayResponsePair, CRelayRequestPairs)& item, requests_to_send)    
    {
        string strError;
        bool lost_permission=false;
        mapReadPermissionCache.clear();
        int ef_cache_id;
        sAddressesToSign.clear();
        
        vector<unsigned char> vRPPayload;
        BOOST_FOREACH(PAIRTYPE(const int, int)& chunk_row, item.second.m_Pairs)    
        {                            
            collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(chunk_row.first);
            if(mc_IsReadPermissionedStream(&(collect_subrow->m_ChunkDef),mapReadPermissionCache,&sAddressesToSign) != 0)
            {
                lost_permission=true;
            }            
        }
        if(!lost_permission)
        {
            vRPPayload.clear();
            request=pRelayManager->FindRequest(item.first.request_id);
            if(request == NULL)
            {
                return false;
            }

            response=&(request->m_Responses[item.first.response_id]);
            
            ef_cache_id=-1;
            if(sAddressesToSign.size())
            {
                if(!pEF->OFF_GetPayloadForReadPermissioned(&vRPPayload,&ef_cache_id,strError))
                {
                    if(fDebug)LogPrint("chunks","Error creating read-permissioned EF payload: %s\n",
                            strError.c_str());                                            
                }
                
            }

            payload.clear();
            shift=mc_PutVarInt(buf,16,item.second.m_Pairs.size());
            payload.resize(5+1+shift+vRPPayload.size()+sizeof(mc_ChunkEntityKey)*item.second.m_Pairs.size());

            expiration=time_now+collector->m_TimeoutRequest;
            dest_expiration=expiration+response->m_MsgID.m_TimeStamp-request->m_MsgID.m_TimeStamp;// response->m_TimeDiff;
            ptrOut=&(payload[0]);
            *ptrOut=MC_RDT_EXPIRATION;
            ptrOut++;
            mc_PutLE(ptrOut,&dest_expiration,sizeof(dest_expiration));
            ptrOut+=sizeof(dest_expiration);
            if(vRPPayload.size())
            {
                memcpy(ptrOut,&(vRPPayload[0]),vRPPayload.size());      
                ptrOut+=vRPPayload.size();
            }
            *ptrOut=MC_RDT_CHUNK_IDS;
            ptrOut++;
            memcpy(ptrOut,buf,shift);
            ptrOut+=shift;
            count=0;
            BOOST_FOREACH(PAIRTYPE(const int, int)& chunk_row, item.second.m_Pairs)    
            {                            
                collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(chunk_row.first);
    //            printf("S %d\n",chunk_row.first);
                collect_subrow->m_State.m_RequestPos=count;
                memcpy(ptrOut,&(collect_subrow->m_ChunkDef),sizeof(mc_ChunkEntityKey));
                ptrOut+=sizeof(mc_ChunkEntityKey);
                count++;
            }
    //        mc_DumpSize("req",&(payload[0]),1+shift+sizeof(mc_ChunkEntityKey)*item.second.m_Pairs.size(),64);
            request_id=pRelayManager->SendNextRequest(response,MC_RMT_CHUNK_REQUEST,0,payload,sAddressesToSign,ef_cache_id);
            if(!request_id.IsZero())
            {
                if(fDebug)LogPrint("chunks","New chunk request: %s, response: %s, chunks: %d\n",request_id.ToString().c_str(),response->m_MsgID.ToString().c_str(),item.second.m_Pairs.size());
                BOOST_FOREACH(PAIRTYPE(const int, int)& chunk_row, item.second.m_Pairs)    
                {                
                    collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(chunk_row.first);
                    collect_subrow->m_State.m_Request=request_id;
                    collect_subrow->m_State.m_RequestTimeStamp=expiration;
                    for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Requested+=k ? collect_subrow->m_ChunkDef.m_Size : 1;                
        //            printf("T %d %d %s\n",chunk_row.first,collect_subrow->m_State.m_RequestPos,collect_subrow->m_State.m_Request.ToString().c_str());
                }                            
            }
        }
        else
        {
            if(fDebug)LogPrint("chunks","Cannot send chunk request: %s, chunks: %d, lost permission\n",
                    request_id.ToString().c_str(),item.second.m_Pairs.size());                        
        }
    }

    row=0;
    last_row=0;
    last_count=0;
    total_size=0;
    
    mapReadPermissionCache.clear();
    sAddressesToSign.clear();
    
    while(row<=collector->m_MemPool->GetCount())
    {
        string strError;
        collect_row=NULL;
        if(row<collector->m_MemPool->GetCount())
        {
            collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        }
        
        if( (collect_row == NULL) || 
            (last_count >= MC_CCW_MAX_CHUNKS_PER_QUERY) || 
            (total_size+collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey)> max_total_query_size) || 
            (sAddressesToSign.size() > MC_CCW_MAX_SIGNATURES_PER_REQEST) )
        {
            if(last_count)
            {
                int extra_size=0;
                int ef_size;
                const unsigned char* ef=pEF->OFF_SupportedEnterpriseFeatures(NULL,0,&ef_size);
                if(sAddressesToSign.size())
                {
                    shift=mc_PutVarInt(buf,16,ef_size);
                    extra_size+=1+shift+ef_size;
                }
                payload.clear();
                shift=mc_PutVarInt(buf,16,last_count);
                payload.resize(extra_size+1+shift+sizeof(mc_ChunkEntityKey)*last_count);
                ptrOut=&(payload[0]);
                
                if(sAddressesToSign.size())
                {
                    shift=mc_PutVarInt(buf,16,ef_size);
                    *ptrOut=MC_RDT_ENTERPRISE_FEATURES;
                    ptrOut++;
                    memcpy(ptrOut,buf,shift);
                    ptrOut+=shift;
                    memcpy(ptrOut,ef,ef_size);
                    ptrOut+=ef_size;
                }
                
                shift=mc_PutVarInt(buf,16,last_count);
                *ptrOut=MC_RDT_CHUNK_IDS;
                ptrOut++;
                memcpy(ptrOut,buf,shift);
                ptrOut+=shift;
                for(int r=last_row;r<row;r++)
                {
                    collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(r);
                    if(collect_subrow->m_State.m_Status & MC_CCF_SELECTED)
                    {
                        memcpy(ptrOut,&(collect_subrow->m_ChunkDef),sizeof(mc_ChunkEntityKey));
                        ptrOut+=sizeof(mc_ChunkEntityKey);
                    }
                }
                query_id=pRelayManager->SendRequest(NULL,MC_RMT_CHUNK_QUERY,0,payload);
                if(fDebug)LogPrint("chunks","New chunk query: %s, chunks: %d, rows [%d-%d), in queries %d (out of %d), per destination: %dKB, timeout: %d\n",query_id.ToString().c_str(),last_count,last_row,row,
                        total_in_queries,max_total_in_queries,collector->m_MaxKBPerDestination,collector->m_TimeoutRequest);
                for(int r=last_row;r<row;r++)
                {
                    collect_subrow=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(r);
                    if(collect_subrow->m_State.m_Status & MC_CCF_SELECTED)
                    {
//                        printf("coll new qry: row: %d, id: %lu: att: %d\n",r,query_id,collect_subrow->m_State.m_QueryAttempts);
                        collect_subrow->m_State.m_Status -= MC_CCF_SELECTED;
                        collect_subrow->m_State.m_Query=query_id;
                        collect_subrow->m_State.m_QueryAttempts+=1;
                        collect_subrow->m_State.m_QueryTimeStamp=time_now+collector->m_TimeoutQuery;
                        collect_subrow->m_State.m_QuerySilenceTimestamp=time_now+collector->m_TimeoutRequest;
                        if(collect_subrow->m_State.m_QueryAttempts>1)
                        {
                            collect_subrow->m_State.m_QuerySilenceTimestamp=collect_subrow->m_State.m_QueryTimeStamp;
                        }
                        collect_subrow->m_State.m_Status |= MC_CCF_UPDATED;
                        to_end_of_query=collect_subrow->m_State.m_QueryTimeStamp-time_now;
                        if(to_end_of_query<0)to_end_of_query=0;
                        if(to_end_of_query>collector->m_TimeoutRequest)to_end_of_query=collector->m_TimeoutRequest;
                        total_in_queries+=(collect_subrow->m_ChunkDef.m_Size+ sizeof(mc_ChunkEntityKey))*to_end_of_query;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Queried+=k ? collect_subrow->m_ChunkDef.m_Size : 1;                
                    }
                }
                last_row=row;
                last_count=0;     
                total_size=0;
                mapReadPermissionCache.clear();
                sAddressesToSign.clear();
            }
        }
        
        if(collect_row)
        {
            if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
            {
                int expired=0;
                if(collect_row->m_State.m_QueryTimeStamp <= time_now)
                {
                    expired=1;
                }
                else
                {                    
                    if(collect_row->m_State.m_QuerySilenceTimestamp <= time_now)
                    {
                        query=NULL;
                        if(!collect_row->m_State.m_Query.IsZero())
                        {
                            query=pRelayManager->FindRequest(collect_row->m_State.m_Query);
                        }
                        if(query)
                        {
                            if(query->m_Responses.size() == 0)
                            {
                                expired=1;
                            }
                            pRelayManager->UnLock();
                        }                        
                    }                    
                }
                
                if(expired)
                {
                    if(!collect_row->m_State.m_Request.IsZero())
                    {
                        pRelayManager->DeleteRequest(collect_row->m_State.m_Request);
                        collect_row->m_State.m_Request=0;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Undelivered+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    }
                    if(!collect_row->m_State.m_Query.IsZero())
                    {
                        map<mc_OffchainMessageID, bool>::iterator itqry = query_to_delete.find(collect_row->m_State.m_Query);
                        if (itqry == query_to_delete.end())
                        {
                            query_to_delete.insert(make_pair(collect_row->m_State.m_Query,true));
                        }       
                        collect_row->m_State.m_Query=0;
                        collect_row->m_State.m_QueryNextAttempt=time_now+MultichainNextChunkQueryAttempt(collect_row->m_State.m_QueryAttempts);      
                        collect_row->m_State.m_Status |= MC_CCF_UPDATED;
                        for(int k=0;k<2;k++)collector->m_StatTotal[k].m_Unresponded+=k ? collect_row->m_ChunkDef.m_Size : 1;                
                    }
                    if((collect_row->m_State.m_QueryNextAttempt <= time_now) && (total_in_queries < max_total_in_queries) && ((int)query_count<collector->m_MaxMemPoolSize))
                    {
                        if( (collect_row->m_State.m_Status & MC_CCF_ERROR_MASK) == 0)
                        {
                            if(mc_IsReadPermissionedStream(&(collect_row->m_ChunkDef),mapReadPermissionCache,&sAddressesToSign) == 0)
                            {
                                if(sAddressesToSign.size() <= MC_CCW_MAX_SIGNATURES_PER_REQEST)
                                {
                                    collect_row->m_State.m_Status |= MC_CCF_SELECTED;
                                    last_count++;
                                    total_size+=collect_row->m_ChunkDef.m_Size + sizeof(mc_ChunkEntityKey);
                                    query_count++;
                                }
                            }
                            else
                            {
                                unsigned char* ptrhash=collect_row->m_ChunkDef.m_Hash;
                                if(fDebug)LogPrint("chunks","Dropped chunk (lost permission) %s\n",(*(uint256*)ptrhash).ToString().c_str());                
                                collect_row->m_State.m_Status |= MC_CCF_DELETED;
                            }
                        }
                    }
                }
                else
                {
                    query_count++;
                }
            }
        }
        row++;
    }
        
    not_processed=0;
    
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(!collect_row->m_State.m_Query.IsZero())
            {
                map<mc_OffchainMessageID, bool>::iterator itqry = query_to_delete.find(collect_row->m_State.m_Query);
                if (itqry != query_to_delete.end())
                {
                    itqry->second=false;
                }            
            }
            not_processed++;
        }
    }

    BOOST_FOREACH(PAIRTYPE(const mc_OffchainMessageID, bool)& item, query_to_delete)    
    {
        if(item.second)
        {
            pRelayManager->DeleteRequest(item.first);
        }
    }    
    
/*    
    for(int k=0;k<2;k++)collector->m_StatLast[k].Zero();
    collector->m_StatLast[0].m_Pending=collector->m_TotalChunkCount;
    collector->m_StatLast[1].m_Pending=collector->m_TotalChunkSize;
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        size=collect_row->m_ChunkDef.m_Size;
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(!collect_row->m_State.m_Request.IsZero())
            {
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Requested+=k ? size : 1;                
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
            }
            else
            {
                if(!collect_row->m_State.m_Query.IsZero())
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
                }
                else                    
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Sleeping+=k ? size : 1;                    
                }                
            }
        }        
        else
        {
            for(int k=0;k<2;k++)collector->m_StatLast[k].m_Pending-=k ? size : 1;                                
        }
    }
 */   
    collector->UnLock();
    if(not_processed < collector->m_MaxMemPoolSize/2)
    {
        if(collector->m_NextAutoCommitTimestamp < GetTimeMillis())
        {
            collector->Commit();
            collector->m_NextAutoCommitTimestamp=GetTimeMillis()+collector->m_AutoCommitDelay;
        }
    }
    
    int err=pEF->FED_EventChunksAvailable();
    if(err)
    {
        LogPrintf("ERROR: Cannot write offchain items to feeds, error %d\n",err);
    }
    
    
    return not_processed;
}

int MultichainCollectChunksQueueStats(mc_ChunkCollector* collector)
{
    int row;
    int delay;
    mc_ChunkCollectorRow *collect_row;
    uint32_t size;
    
    collector->Lock();    
    
    for(int k=0;k<2;k++)collector->m_StatLast[k].Zero();
    collector->m_StatLast[0].m_Pending=collector->m_TotalChunkCount;
    collector->m_StatLast[1].m_Pending=collector->m_TotalChunkSize;
    for(row=0;row<collector->m_MemPool->GetCount();row++)
    {
        collect_row=(mc_ChunkCollectorRow *)collector->m_MemPool->GetRow(row);
        size=collect_row->m_ChunkDef.m_Size;
        for(int k=0;k<2;k++)collector->m_StatLast[k].m_Undelivered+=k ? size : 1;                
        if( (collect_row->m_State.m_Status & MC_CCF_DELETED ) == 0 )
        {
            if(!collect_row->m_State.m_Request.IsZero())
            {
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Requested+=k ? size : 1;                
                for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
            }
            else
            {
                if(!collect_row->m_State.m_Query.IsZero())
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Queried+=k ? size : 1;                                        
                }
                else                    
                {
                    for(int k=0;k<2;k++)collector->m_StatLast[k].m_Sleeping+=k ? size : 1;                    
                }                
            }
        }        
        else
        {
            for(int k=0;k<2;k++)collector->m_StatLast[k].m_Pending-=k ? size : 1;                                
        }
    }

    collector->UnLock();    
    if(collector->m_MemPool->GetCount() > (int)(collector->m_MaxMemPoolSize*1.2))
    {
        if(collector->m_NextAutoCommitTimestamp < GetTimeMillis())
        {
            collector->Commit();
            collector->m_NextAutoCommitTimestamp=GetTimeMillis()+collector->m_AutoCommitDelay;
        }
    }
    
    delay=collector->m_StatLast[0].m_Queried;
    if(delay>MC_CCW_MAX_DELAY_BETWEEN_COLLECTS)
    {
        delay=MC_CCW_MAX_DELAY_BETWEEN_COLLECTS;
    }
    
    return delay;
}

void mc_RelayPayload_ChunkIDs(vector<unsigned char>* payload,vector <mc_ChunkEntityKey>& vChunkDefs,int size,const unsigned char* ef,int ef_size)
{
    unsigned char buf[16];
    int shift,extra_size;
    unsigned char *ptrOut;
    
    if(payload)
    {
        if(vChunkDefs.size())
        {
            extra_size=0;
            if(ef)
            {
                shift=mc_PutVarInt(buf,16,ef_size);
                extra_size+=1+shift+ef_size;
            }
            
            shift=mc_PutVarInt(buf,16,vChunkDefs.size());
            payload->resize(extra_size+1+shift+size*vChunkDefs.size());
            ptrOut=&(*payload)[0];

            if(ef)
            {
                shift=mc_PutVarInt(buf,16,ef_size);
                *ptrOut=MC_RDT_ENTERPRISE_FEATURES;
                ptrOut++;
                memcpy(ptrOut,buf,shift);
                ptrOut+=shift;
                memcpy(ptrOut,ef,ef_size);
                ptrOut+=ef_size;
            }
            
            shift=mc_PutVarInt(buf,16,vChunkDefs.size());
            *ptrOut=MC_RDT_CHUNK_IDS;
            ptrOut++;
            memcpy(ptrOut,buf,shift);
            ptrOut+=shift;

            for(int i=0;i<(int)vChunkDefs.size();i++)
            {
                memcpy(ptrOut,&vChunkDefs[i],size);
                ptrOut+=size;
            }                        
        }
    }
}

bool mc_RelayProcess_Chunk_Query(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,string& strError)
{
    unsigned char *ptr;
    int shift,count,size,subscriber_ef_length,publisher_ef_length;
    vector <mc_ChunkEntityKey> vToRelay;
    vector <mc_ChunkEntityKey> vToRespond;
    map<uint160,int> mapReadPermissionCache;
    mc_ChunkEntityKey chunk;
    mc_ChunkDBRow chunk_def;
    
    unsigned char *subscriber_ef=NULL;
    unsigned char *publisher_ef=NULL;
    subscriber_ef_length=0;
    publisher_ef_length=0;
        
    size=sizeof(mc_ChunkEntityKey);
    ptr=ptrStart;
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_ENTERPRISE_FEATURES:
                if(mc_gState->m_Features->ReadPermissions())
                {
                    ptr++;
                    subscriber_ef_length=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                    ptr+=shift;
                    if(subscriber_ef_length > MC_CCW_MAX_EF_SIZE)
                    {
                        strError="Bad enterprise features object";
                        return false;                                            
                    }
                    subscriber_ef=ptr;
                    publisher_ef=pEF->OFF_SupportedEnterpriseFeatures(subscriber_ef,subscriber_ef_length,&publisher_ef_length);                    
                    ptr+=subscriber_ef_length;
                }
                else
                {
                    strError=strprintf("Request format (%d, %d) not supported in this protocol version",MC_RMT_CHUNK_QUERY,*ptr);
                    return false;                    
                }
                break;
            case MC_RDT_CHUNK_IDS:
                ptr++;
                count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                ptr+=shift;
                if(count*size != (ptrEnd-ptr))
                {
                    strError="Bad chunk ids query";
                    return false;                    
                }                
                for(int c=0;c<count;c++)
                {
                    string strErrorToIgnore;
                    chunk=*(mc_ChunkEntityKey*)ptr;
                    if( (mc_IsReadPermissionedStream(&chunk,mapReadPermissionCache,NULL) == 0) ||
                        ((pEF->LIC_VerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_DELIVER,strErrorToIgnore) != 0) && 
                         (pEF->LIC_VerifyFeature(MC_EFT_NETWORK_SIGNED_RECEIVE,strErrorToIgnore) != 0) &&                          
                            (publisher_ef != NULL) ))
                    {
                        if(pwalletTxsMain->m_ChunkDB->GetChunkDef(&chunk_def,chunk.m_Hash,NULL,NULL,-1) == MC_ERR_NOERROR)
                        {
                            if(chunk_def.m_Size != chunk.m_Size)
                            {
                                chunk.m_Flags |= MC_CCF_WRONG_SIZE;
                                chunk.m_Size=chunk_def.m_Size;
                            }
                            vToRespond.push_back(chunk);
                        }                    
                        else
                        {
                            vToRelay.push_back(chunk);                                                
                        }
                    }
                    else
                    {
                        vToRelay.push_back(chunk);                                                                        
                    }
                    ptr+=size;
                }

                mc_RelayPayload_ChunkIDs(payload_response,vToRespond,size,publisher_ef,publisher_ef_length);
                mc_RelayPayload_ChunkIDs(payload_relay,vToRelay,size,subscriber_ef,subscriber_ef_length);
                break;
            default:
                strError=strprintf("Unsupported request format (%d, %d)",MC_RMT_CHUNK_QUERY,*ptr);
                return false;
        }
    }
    
    return true;
}


bool mc_RelayProcess_Chunk_Request(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,
        map<uint160,int>& mapReadPermissionCache,bool* read_permissioned,string& strError)
{
    unsigned char *ptr;
    int shift,count,size;
    mc_ChunkEntityKey chunk;
    mc_ChunkDBRow chunk_def;
    const unsigned char *chunk_found;
    unsigned char buf[16];
    size_t chunk_bytes;
    unsigned char *ptrOut;
    
    uint32_t total_size=0;
    uint32_t max_total_size=MAX_SIZE-OFFCHAIN_MSG_PADDING;
    uint32_t expiration=0;
    
    if(read_permissioned)
    {
        *read_permissioned=false;
    }
    
    mc_gState->m_TmpBuffers->m_RelayTmpBuffer->Clear();
    mc_gState->m_TmpBuffers->m_RelayTmpBuffer->AddElement();
            
    size=sizeof(mc_ChunkEntityKey);
    ptr=ptrStart;
    while(ptr<ptrEnd)
    {
        switch(*ptr)
        {
            case MC_RDT_EXPIRATION:
                ptr++;
                expiration=(uint32_t)mc_GetLE(ptr,sizeof(expiration));
                ptr+=sizeof(expiration);
                if(expiration+1 < pRelayManager->m_LastTime)
                {
                    strError="Less than 1s for request expiration";
                    return false;                                        
                }
                if(expiration-35 > pRelayManager->m_LastTime)                   // We are supposed to store query_hit record only for 30s, something is wrong
                {
                    strError="Expiration is too far in the future";
                    return false;                                                            
                }
                max_total_size=pwalletTxsMain->m_ChunkCollector->m_MaxMBPerSecond*(expiration-pRelayManager->m_LastTime)*1024*1024;
                break;
            case MC_RDT_CHUNK_IDS:
                ptr++;
                count=(int)mc_GetVarInt(ptr,ptrEnd-ptr,-1,&shift);
                ptr+=shift;
                if(count*size != (ptrEnd-ptr))
                {
                    strError="Bad chunk ids request";
                    return false;                    
                }                
                for(int c=0;c<count;c++)
                {
                    chunk=*(mc_ChunkEntityKey*)ptr;
                    if(read_permissioned)
                    {
                        if(mc_IsReadPermissionedStream(&chunk,mapReadPermissionCache,NULL) != 0)
                        {
                            *read_permissioned=true;
                            if(!pEF->LIC_VerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_DELIVER,strError))
                            {
                                strError="Request for chunk in read-permissioned stream";
                                return false;                    
                            }
                        }
                    }
                    else
                    {
                        unsigned char* ptrhash=chunk.m_Hash;
                        if(fDebug)LogPrint("chunks","Request for chunk: %s\n",(*(uint256*)ptrhash).ToString().c_str());
                        if(pwalletTxsMain->m_ChunkDB->GetChunkDef(&chunk_def,chunk.m_Hash,NULL,NULL,-1) == MC_ERR_NOERROR)
                        {
                            if(chunk_def.m_Size != chunk.m_Size)
                            {
                                strError="Bad chunk size";
                                return false;                    
                            }
                            total_size+=chunk_def.m_Size+size;
                            if(total_size > MAX_SIZE-OFFCHAIN_MSG_PADDING)
                            {
                                strError="Total size of requested chunks is too big for message";
                                return false;                                                
                            }
                            if(total_size > max_total_size)
                            {
                                strError="Total size of requested chunks is too big for response expiration";
                                return false;                                                
                            }
                            
                            unsigned char salt[MC_CDB_CHUNK_SALT_SIZE];
                            uint32_t salt_size;
                            
                            chunk_found=pwalletTxsMain->m_ChunkDB->GetChunk(&chunk_def,0,-1,&chunk_bytes,salt,&salt_size);
                            mc_gState->m_TmpBuffers->m_RelayTmpBuffer->SetData((unsigned char*)&chunk,size);
                            mc_gState->m_TmpBuffers->m_RelayTmpBuffer->SetData(salt,salt_size);
                            mc_gState->m_TmpBuffers->m_RelayTmpBuffer->SetData(chunk_found,chunk_bytes);
                        }                    
                        else
                        {
                            strError="Chunk not found";
                            return false;                    
                        }
                    }
                    ptr+=size;
                }
                
                if(!read_permissioned)
                {
                    chunk_found=mc_gState->m_TmpBuffers->m_RelayTmpBuffer->GetData(0,&chunk_bytes);
                    shift=mc_PutVarInt(buf,16,count);
                    payload_response->resize(1+shift+chunk_bytes);
                    ptrOut=&(*payload_response)[0];

                    *ptrOut=MC_RDT_CHUNKS;
                    ptrOut++;
                    memcpy(ptrOut,buf,shift);
                    ptrOut+=shift;
                    memcpy(ptrOut,chunk_found,chunk_bytes);
                    ptrOut+=chunk_bytes;
                }
                
                break;
            default:
                if(read_permissioned)
                {
                    *read_permissioned=true;
                    if(!pEF->LIC_VerifyFeature(MC_EFT_STREAM_READ_RESTRICTED_DELIVER,strError))
                    {
                        strError=strprintf("Unsupported request format (%d, %d)",MC_RMT_CHUNK_REQUEST,*ptr);
                    }
                    return false;
                }
        }
    }

    if(total_size > max_total_size)
    {
        strError="Total size of requested chunks is too big for response expiration";
        return false;                                                
    }
    
    return true;
}

bool mc_Chunk_RelayResponse(uint32_t msg_type_stored, CNode *pto_stored,
                             uint32_t msg_type_in, uint32_t  flags, vector<unsigned char>& vPayloadIn,vector<CScript>& vSigScriptsIn,vector<CScript>& vSigScriptsToVerify,
                             uint32_t* msg_type_response,uint32_t  *flags_response,vector<unsigned char>& vPayloadResponse,vector<CScript>& vSigScriptsRespond,
                             uint32_t* msg_type_relay,uint32_t  *flags_relay,vector<unsigned char>& vPayloadRelay,vector<CScript>& vSigScriptsRelay,string& strError)
{
    unsigned char *ptr;
    unsigned char *ptrEnd;
    vector<unsigned char> *payload_relay_ptr=NULL;
    vector<unsigned char> *payload_response_ptr=NULL;
    
    if(msg_type_response)
    {
        payload_response_ptr=&vPayloadResponse;
    }
    
    if(msg_type_relay)
    {
        payload_relay_ptr=&vPayloadRelay;
    }
    
    ptr=&vPayloadIn[0];
    ptrEnd=ptr+vPayloadIn.size();
            
    strError="";
    switch(msg_type_in)
    {
        case MC_RMT_CHUNK_QUERY:
            if(msg_type_stored)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr == NULL)
            {
                if(payload_relay_ptr)
                {
                    vPayloadRelay=vPayloadIn;
                    *msg_type_relay=msg_type_in;                    
                }
            }
            else
            {
                if(mc_RelayProcess_Chunk_Query(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,strError))
                {
                    if(payload_response_ptr && (payload_response_ptr->size() != 0))
                    {
                        *msg_type_response=MC_RMT_CHUNK_QUERY_HIT;
                    }
                    if(payload_relay_ptr && (payload_relay_ptr->size() != 0))
                    {
                        *msg_type_relay=MC_RMT_CHUNK_QUERY;
                    }
                }
            }            
            break;
        case MC_RMT_CHUNK_QUERY_HIT:
            if(msg_type_stored != MC_RMT_CHUNK_QUERY)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr)
            {
                *msg_type_response=MC_RMT_ADD_RESPONSE;
            }
            break;
        case MC_RMT_CHUNK_REQUEST:
            if(msg_type_stored != MC_RMT_CHUNK_QUERY_HIT)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr == NULL)
            {
                if(payload_relay_ptr)
                {
                    vPayloadRelay=vPayloadIn;
                    *msg_type_relay=msg_type_in;                    
                }
            }
            else
            {
                map<uint160,int> mapReadPermissionCache;
                bool read_permissioned;
                if(!mc_RelayProcess_Chunk_Request(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,mapReadPermissionCache,&read_permissioned,strError))
                {
                    if(!read_permissioned)
                    {
                        goto exitlbl;                            
                    }
                }
                vSigScriptsToVerify.clear();
                if(read_permissioned)
                {
                    if(!pEF->OFF_ProcessChunkRequest(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,mapReadPermissionCache,strError))
                    {
                        goto exitlbl;                                                    
                    }
                    if(!pEF->OFF_GetScriptsToVerify(mapReadPermissionCache,vSigScriptsIn,vSigScriptsToVerify,strError))
                    {
                        goto exitlbl;                            
                    }
                }
                else
                {
                    if(!mc_RelayProcess_Chunk_Request(ptr,ptrEnd,payload_response_ptr,payload_relay_ptr,mapReadPermissionCache,NULL,strError))
                    {
                        goto exitlbl;                            
                    }                    
                }
                
                if(payload_response_ptr && (payload_response_ptr->size() != 0))
                {
                    vSigScriptsRespond.clear();
                    *msg_type_response=MC_RMT_CHUNK_RESPONSE;
                }
                if(payload_relay_ptr && (payload_relay_ptr->size() != 0))
                {
                    vSigScriptsRelay=vSigScriptsIn;
                    *msg_type_relay=MC_RMT_CHUNK_REQUEST;
                }
            }         
            
            break;
        case MC_RMT_CHUNK_RESPONSE:
            if(msg_type_stored != MC_RMT_CHUNK_REQUEST)
            {
                strError=strprintf("Unexpected response message type (%s,%s)",mc_MsgTypeStr(msg_type_stored).c_str(),mc_MsgTypeStr(msg_type_in).c_str());
                goto exitlbl;
            } 
            if(payload_response_ptr)
            {
                *msg_type_response=MC_RMT_ADD_RESPONSE;
            }

            break;
    }
    
exitlbl:
            
    if(strError.size())
    {
        return false;
    }

    return true;
}
