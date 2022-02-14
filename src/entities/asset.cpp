// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"

#define MC_AST_ASSET_REF_TYPE_OFFSET        32
#define MC_AST_ASSET_REF_TYPE_SIZE           4
#define MC_AST_ASSET_SCRIPT_TYPE_OFFSET     44
#define MC_AST_ASSET_SCRIPT_TYPE_SIZE        4

#define MC_AST_ASSET_MAX_MEMPOOL_SCRIPT_SIZE   280                              // 384 - 96 bytes (data) - 8 bytes (position fields)

#define MC_AST_ASSET_SCRIPT_POS_EXTENDED_SCRIPT         0
#define MC_AST_ASSET_SCRIPT_POS_MAX                     1

unsigned char entitylist_key_prefix[MC_ENT_KEY_SIZE];

void mc_EntityDBRow::Zero()
{
    memset(this,0,sizeof(mc_EntityDBRow));
}

void mc_EntityLedgerRow::Zero()
{
    memset(this,0,sizeof(mc_EntityLedgerRow));
}

void mc_EntityDetails::Zero()
{
    m_LedgerRow.ReleaseScriptPointer();
    memset(this,0,sizeof(mc_EntityDetails));
    m_LedgerRow.Zero();    
}

void mc_EntityLedgerRow::ReleaseScriptPointer()
{
    if(m_Script)
    {
        if(m_Script != m_StaticScript)
        {
            mc_gState->m_Assets->m_LedgerRowScripts->ReleasePointer(this);
        }
    }    
}

void mc_EntityLedgerRow::Destroy()
{
    ReleaseScriptPointer();
}

void mc_EntityLedgerRow::Copy(mc_EntityLedgerRow *row)
{
    ReleaseScriptPointer();
    Zero();
    memcpy(this,row,sizeof(mc_EntityLedgerRow));
    m_Script=(unsigned char *)mc_gState->m_Assets->m_LedgerRowScripts->GetPointer(this,m_StaticScript,row->m_Script,m_ScriptSize);
}

void mc_EntityLedgerRow::SetScript(const void *lpData, int size)
{
    ReleaseScriptPointer();
    m_ScriptSize=size;
    m_Script=(unsigned char *)mc_gState->m_Assets->m_LedgerRowScripts->GetPointer(this,m_StaticScript,lpData,m_ScriptSize);    
}

/** Set initial database object values */

void mc_EntityDB::Zero()
{
    m_FileName[0]=0;
    m_DB=0;
    m_KeyOffset=0;
    m_KeySize=36;                                   
    m_ValueOffset=36;
    m_ValueSize=28;  
    m_TotalSize=m_KeySize+m_ValueSize;
}

/** Set database file name */

void mc_EntityDB::SetName(const char* name)
{
    mc_GetFullFileName(name,"entities",".db",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_FileName);
}

/** Open database */

int mc_EntityDB::Open() 
{
    
    m_DB=new mc_Database;
    
    m_DB->SetOption("KeySize",0,m_KeySize);
    m_DB->SetOption("ValueSize",0,m_ValueSize);
        
    return m_DB->Open(m_FileName,MC_OPT_DB_DATABASE_CREATE_IF_MISSING | MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_LEVELDB | MC_OPT_DB_DATABASE_THREAD_SAFE);
}

/** Close database */

int mc_EntityDB::Close()
{
    if(m_DB)
    {
        m_DB->Close();
        delete m_DB;    
        m_DB=NULL;
    }
    return 0;
}
    
/** Set initial ledger values */

void mc_EntityLedger::Zero()
{
    m_FileName[0]=0;
    m_FileHan=0;
    m_KeyOffset=0;
    m_KeySize=36;                                   
    m_ValueOffset=36;
    m_ValueSize=60;  
    m_TotalSize=m_KeySize+m_ValueSize;
    memset(m_ZeroBuffer,0,m_TotalSize);                                         // Allocated for 96, check if m_TotalSize changed
    m_MaxScriptMemPoolSize=MC_AST_ASSET_MAX_MEMPOOL_SCRIPT_SIZE;
    m_MemPoolSize=m_TotalSize+2*sizeof(int32_t)+sizeof(unsigned char*)+m_MaxScriptMemPoolSize;         // 256
}

/** Set ledger file name */

void mc_EntityLedger::SetName(const char* name)
{
    mc_GetFullFileName(name,"entities",".dat",MC_FOM_RELATIVE_TO_DATADIR,m_FileName);
}

/** Open ledger file */

int mc_EntityLedger::Open()
{
    if(m_FileHan>0)
    {
        return m_FileHan;
    }
    
    m_FileHan=open(m_FileName,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    return m_FileHan;            
}

void mc_EntityLedger::Flush()
{
    if(m_FileHan>0)
    {
        __US_FlushFile(m_FileHan);
    }    
}

/** Close ledger file */

int mc_EntityLedger::Close()
{
    if(m_FileHan>0)
    {
        close(m_FileHan);
    }    
    m_FileHan=0;
    return 0;    
}

int mc_EntityLedger::GetRow(int64_t pos, mc_EntityLedgerRow* row)
{
    unsigned char m_RowScript[MC_ENT_SCRIPT_ALLOC_SIZE];
    int size;
    int tail;
    if(m_FileHan<=0)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    if(lseek64(m_FileHan,pos,SEEK_SET) != pos)
    {
        return MC_ERR_NOT_FOUND;
    }
    
    row->ReleaseScriptPointer();
    row->Zero();

    if(read(m_FileHan,(unsigned char*)row+m_KeyOffset,m_TotalSize) != m_TotalSize)
    {
        return MC_ERR_FILE_READ_ERROR;
    }
    
    size=row->m_ScriptSize+row->m_ExtendedScript;
    if(size>0)
    {
        tail=size;
        size=mc_AllocSize(size,m_TotalSize,1);
        tail=size-tail;
        if(row->m_ScriptSize)
        {
            if(read(m_FileHan,m_RowScript,row->m_ScriptSize) != row->m_ScriptSize)
            {
                return MC_ERR_FILE_READ_ERROR;
            }        
            row->m_Script=(unsigned char *)mc_gState->m_Assets->m_LedgerRowScripts->GetPointer(row,row->m_StaticScript,m_RowScript,row->m_ScriptSize);            
        }
        if(row->m_ExtendedScript)
        {
/*            
            mc_gState->m_Assets->m_RowExtendedScript->Clear();
            mc_gState->m_Assets->m_RowExtendedScript->Resize(row->m_ExtendedScript,0);
            if(read(m_FileHan,mc_gState->m_Assets->m_RowExtendedScript->m_lpData,row->m_ExtendedScript) != row->m_ExtendedScript)
 */
            mc_Script *script=mc_gState->m_Assets->m_RowExtendedScripts->SetScriptPointer(MC_AST_ASSET_SCRIPT_POS_EXTENDED_SCRIPT,row->m_ExtendedScript);
            if(read(m_FileHan,script->m_lpData,row->m_ExtendedScript) != row->m_ExtendedScript)
            {
                return MC_ERR_FILE_READ_ERROR;
            }        
        }
        if(tail)
        {
            if(read(m_FileHan,m_ZeroBuffer,tail) != tail)
            {
                return MC_ERR_FILE_READ_ERROR;
            }        
            memset(m_ZeroBuffer,0,tail);
        }
    }
    
    return MC_ERR_NOERROR;
}

int64_t mc_EntityLedger::GetSize()
{
    int64_t pos;
    
    mc_EntityLedgerRow aldRow;
    if(m_FileHan<=0)
    {
        return 0;
    }

    GetRow(0,&aldRow);
    pos=aldRow.m_PrevPos;
    GetRow(pos,&aldRow);
    pos+=mc_AllocSize(m_TotalSize+aldRow.m_ScriptSize+aldRow.m_ExtendedScript,m_TotalSize,1);
    
    return pos;
}

int mc_EntityLedger::SetRow(int64_t pos, mc_EntityLedgerRow* row)
{
    int size;
    int tail;
    
    if(m_FileHan<=0)
    {
        return -1;
    }
    
    if(lseek64(m_FileHan,pos,SEEK_SET) < 0)
    {
        return -1;
    }
       
    size=row->m_ScriptSize;

    const unsigned char *extended_script=NULL;
    size_t extended_script_size=0;

    if(row->m_ScriptMemPoolPos)
    {
        row->SetScript(mc_gState->m_Assets->m_ExtendedScripts->GetData(row->m_ScriptMemPoolPos,NULL),row->m_ScriptSize);
    }
    
    if(row->m_ExtendedScriptMemPoolPos)
    {
        extended_script=mc_gState->m_Assets->m_ExtendedScripts->GetData(row->m_ExtendedScriptMemPoolPos,&extended_script_size);
        row->m_ExtendedScript=extended_script_size;
        size+=extended_script_size;               
    }
    
    if(size>=0)
    {
        tail=size;
        size=mc_AllocSize(size,m_TotalSize,1);
        tail=size-tail;
        if(write(m_FileHan,row,m_TotalSize) != m_TotalSize)
        {
            return -1;
        }        
        if(size)
        {
            if(row->m_ScriptSize)
            {
                if(write(m_FileHan,row->m_Script,row->m_ScriptSize) != row->m_ScriptSize)
                {
                    return -1;
                }        
            }
            if(extended_script_size)
            {
                if(write(m_FileHan,extended_script,extended_script_size) != (int)extended_script_size)
                {
                    return -1;
                }                        
            }
            if(tail)
            {
                if(write(m_FileHan,m_ZeroBuffer,tail) != tail)
                {
                    return -1;
                }                                        
            }
        }
    }
    else
    {
        return -1;        
    }
    
    return m_TotalSize+size;
}

int mc_EntityLedger::SetZeroRow(mc_EntityLedgerRow* row)
{
    return SetRow(0,row);
}


int mc_AssetDB::Zero()
{
    m_Database = NULL;
    m_Ledger = NULL;
    m_MemPool = NULL;
    m_TmpRelevantEntities = NULL;
    m_ShortTxIDCache = NULL;
    m_ExtendedScripts=NULL;
//    m_RowExtendedScript=NULL;
    m_LedgerRowScripts=NULL;
    m_RowExtendedScripts=NULL;
    
    m_Name[0]=0x00; 
    m_Block=-1;    
    m_PrevPos=-1;
    m_Pos=0;
    m_DBRowCount=0;     
    m_Flags=0;
    
    m_Semaphore=NULL;
    m_LockedBy=0;
    
    m_ThreadRollBackPos=NULL;
    
    return MC_ERR_NOERROR;
}

int mc_AssetDB::Initialize(const char *name,uint32_t mode,int32_t version)
{
    int err,value_len,take_it;    
    int32_t adbBlock,aldBlock;
    uint64_t adbLastPos,aldLastPos,this_pos;
    
    unsigned char *ptr;

    mc_EntityDBRow adbRow;
    mc_EntityLedgerRow aldRow;
        
    strcpy(m_Name,name);
    
    memset(entitylist_key_prefix,0,MC_ENT_KEY_SIZE);
    entitylist_key_prefix[MC_ENT_KEY_SIZE-16]=MC_ENT_TYPE_ENTITYLIST;
    
    err=MC_ERR_NOERROR;
    
    m_Ledger=new mc_EntityLedger;
    m_Database=new mc_EntityDB;
     
    m_Ledger->SetName(name);
    m_Database->SetName(name);
    
    strcpy(m_Name,name);

    err=m_Database->Open();
    
    if(err)
    {
        return err;
    }
    
    adbBlock=-1;    
    adbLastPos=0;
    
    adbRow.Zero();
    
    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
    if(err)
    {
        return err;
    }

    if(ptr)                                                                     
    {
        memcpy((char*)&adbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        adbBlock=adbRow.m_Block;
        adbLastPos=adbRow.m_LedgerPos;
    }
    else
    {
        adbRow.Zero();
        adbRow.m_Block=(uint32_t)adbBlock;
        adbRow.m_LedgerPos=adbLastPos;
#ifndef MAC_OSX
        adbRow.m_Flags|=MC_ENT_FLAG_ENTITYLIST;
#endif        
        err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,(char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,0);
        if(err)
        {
            return err;
        }
        
        err=m_Database->m_DB->Commit(0);//MC_OPT_DB_DATABASE_TRANSACTIONAL
        if(err)
        {
            return err;
        }
    }
    m_Flags=adbRow.m_Flags;
    
    m_MemPool=new mc_Buffer;    
    err=m_MemPool->Initialize(m_Ledger->m_KeySize,m_Ledger->m_MemPoolSize,MC_BUF_MODE_MAP);
    
    m_TmpRelevantEntities=new mc_Buffer;
    err=m_TmpRelevantEntities->Initialize(MC_AST_SHORT_TXID_SIZE,MC_AST_SHORT_TXID_SIZE,MC_BUF_MODE_MAP);
    
    m_ShortTxIDCache=new mc_Buffer;    
    err=m_ShortTxIDCache->Initialize(MC_AST_SHORT_TXID_SIZE,MC_AST_SHORT_TXID_SIZE+MC_PLS_SIZE_ENTITY,MC_BUF_MODE_MAP);
    
    m_ExtendedScripts=new mc_Script;
//    m_RowExtendedScript=new mc_Script;

    m_LedgerRowScripts=new mc_TSHeap;
    m_LedgerRowScripts->Initialize(MC_ENT_SCRIPT_STATIC_SIZE,MC_ENT_SCRIPT_ALLOC_SIZE,0);
//    m_LedgerRowScripts->Initialize(32,MC_ENT_SCRIPT_ALLOC_SIZE,0);

    m_RowExtendedScripts=new mc_TSScriptHeap;
    m_RowExtendedScripts->Initialize(MC_AST_ASSET_SCRIPT_POS_MAX,0);
    
    
    m_Block=adbBlock;
    m_PrevPos=adbLastPos;            
    m_Pos=adbLastPos+m_Ledger->m_TotalSize;            
    
    aldBlock=-1;
    aldLastPos=0;
    if(m_Ledger->Open() <= 0)
    {
        return MC_ERR_DBOPEN_ERROR;
    }
    int64_t modeversion=0;
    if(m_Ledger->GetRow(0,&aldRow) == 0)
    {
        aldBlock=aldRow.m_Block;
        aldLastPos=aldRow.m_PrevPos;
        m_PrevPos=adbLastPos;       
        modeversion=aldRow.m_Quantity;
        m_Version=mc_GetLE(&modeversion,sizeof(int32_t));
        m_Mode=mc_GetLE((unsigned char*)&modeversion+sizeof(int32_t),sizeof(uint32_t));
        if(m_Ledger->GetRow(m_PrevPos,&aldRow))
        {
            return MC_ERR_CORRUPTED;
        }
        
        m_Pos=m_PrevPos+mc_AllocSize(m_Ledger->m_TotalSize+aldRow.m_ScriptSize+aldRow.m_ExtendedScript,m_Ledger->m_TotalSize,1);
    }
    else
    {
        aldRow.Zero();
        aldRow.m_Block=(uint32_t)aldBlock;
        aldRow.m_PrevPos=aldLastPos;
        m_Version=version;
        m_Mode=mode;
        mc_PutLE(&modeversion,&m_Version,sizeof(int32_t));
        mc_PutLE((unsigned char*)&modeversion+sizeof(int32_t),&m_Mode,sizeof(uint32_t));
        aldRow.m_Quantity=modeversion;
        m_Ledger->SetZeroRow(&aldRow);
    }        
    if(m_Pos != m_Ledger->GetSize())
    {
        m_Ledger->Close();
        return MC_ERR_CORRUPTED;        
    }

    m_Ledger->Close();
    
    if(adbBlock < aldBlock)
    {
        if(m_Ledger->Open() <= 0)
        {
            return MC_ERR_DBOPEN_ERROR;
        }
    
        this_pos=aldLastPos;
        take_it=1;

        while(take_it && (this_pos>0))
        {            
            m_Ledger->GetRow(this_pos,&aldRow);
        
            if(aldRow.m_Block <= adbBlock)
            {
                take_it=0;
            }
            if(take_it)
            {
                this_pos=aldRow.m_PrevPos;
            }
        }

        aldLastPos=this_pos;

        m_Ledger->GetRow(0,&aldRow);
        aldRow.m_Block=adbBlock;
        aldRow.m_PrevPos=m_PrevPos;
        m_Ledger->SetZeroRow(&aldRow);
        
        m_Ledger->Close();  

        aldBlock=adbBlock;
    }
    
    if(adbBlock != aldBlock)
    {
        return MC_ERR_CORRUPTED;
    }

    if(adbLastPos != aldLastPos)
    {
        return MC_ERR_CORRUPTED;
    }
    
    m_Semaphore=__US_SemCreate();
    if(m_Semaphore == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    m_ThreadRollBackPos=new mc_Buffer;
    m_ThreadRollBackPos->Initialize(sizeof(uint64_t),sizeof(mc_RollBackPos)+sizeof(uint64_t),MC_BUF_MODE_MAP);    
    m_ThreadRollBackPos->Realloc(MC_PRM_MAX_THREADS);
    
    for(int mprow=0;mprow<MC_PRM_MAX_THREADS;mprow++)
    {
        mc_RollBackPos *rollback_pos=(mc_RollBackPos*)(m_ThreadRollBackPos->GetRow(mprow)+sizeof(uint64_t));    
        rollback_pos->Zero();
    }

    return MC_ERR_NOERROR;
}

mc_RollBackPos *mc_AssetDB::GetRollBackPos()
{
    uint64_t thread_id=__US_ThreadID();
    int mprow=m_ThreadRollBackPos->Seek(&thread_id);
    if(mprow < 0)
    {
        return NULL;
    }
    
    return (mc_RollBackPos*)(m_ThreadRollBackPos->GetRow(mprow)+sizeof(uint64_t));    
    
}

void mc_AssetDB::RemoveFiles()
{
    mc_EntityDBRow adbRow;
    if(m_Database)
    {
        adbRow.Zero();
        m_Database->m_DB->Delete((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,0);
        m_Database->m_DB->Commit(0);
    }
    if(m_Ledger)
    {
        m_Ledger->Close();
        remove(m_Ledger->m_FileName);
    }
}

int mc_AssetDB::Destroy()
{
    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
    
    if(m_Database)
    {
        m_Database->Close();        
        delete m_Database;
    }
    
    if(m_Ledger)
    {
        delete m_Ledger;
    }
    
    if(m_MemPool)
    {
        delete m_MemPool;
    }  
    
    if(m_TmpRelevantEntities)
    {
        delete m_TmpRelevantEntities;
    }  
    
    if(m_ShortTxIDCache)
    {
        delete m_ShortTxIDCache;
    }  
    
    if(m_ExtendedScripts)
    {
        delete m_ExtendedScripts;
    }
/*    
    if(m_RowExtendedScript)
    {
        delete m_RowExtendedScript;
    }
*/    
    if(m_LedgerRowScripts)
    {
        delete m_LedgerRowScripts;
    }
    
    if(m_RowExtendedScripts)
    {
        delete m_RowExtendedScripts;
    }
    
    if(m_ThreadRollBackPos)
    {
        delete m_ThreadRollBackPos;
        m_ThreadRollBackPos=NULL;
    }
    
    Zero();
    
    return MC_ERR_NOERROR;
}

/** Locking assets object */

void mc_AssetDB::Lock(int write_mode)
{        
    uint64_t this_thread;
    this_thread=__US_ThreadID();
    
    if(this_thread == m_LockedBy)
    {
        return;
    }
    
    __US_SemWait(m_Semaphore); 
    m_LockedBy=this_thread;
}

/** Unlocking assets object */

void mc_AssetDB::UnLock()
{    
    m_LockedBy=0;
    __US_SemPost(m_Semaphore);
}


int mc_AssetDB::SetCheckPoint()
{
    Lock(1);
    
    m_CheckPointPos=m_Pos;
    m_CheckPointMemPoolSize=m_MemPool->GetCount();
    
    UnLock();
    
    return MC_ERR_NOERROR;
}

int mc_AssetDB::RollBackToCheckPoint()
{
    Lock(1);

    m_Pos=m_CheckPointPos;
    m_MemPool->SetCount(m_CheckPointMemPoolSize);
    
    UnLock();
    
    return MC_ERR_NOERROR;
}


int mc_AssetDB::SetRollBackPos(int block,int offset,int inmempool)
{
    mc_RollBackPos *rollback_pos=GetRollBackPos();
    if(rollback_pos == NULL)
    {
        Lock(1);
        uint64_t thread_id=__US_ThreadID();
        m_ThreadRollBackPos->Add(&thread_id,NULL);        
        UnLock();        
        rollback_pos=GetRollBackPos();
        if(rollback_pos == NULL)
        {
            return MC_ERR_INTERNAL_ERROR;
        }
        rollback_pos->Zero();
    }
    
    rollback_pos->m_Block=block;
    rollback_pos->m_Offset=offset;
    rollback_pos->m_InMempool=inmempool;
    
/*    
    Lock(1);

    m_RollBackPos.m_Block=block;
    m_RollBackPos.m_Offset=offset;
    m_RollBackPos.m_InMempool=inmempool;
    
    UnLock();
*/    
    return MC_ERR_NOERROR;
}

void mc_AssetDB::ResetRollBackPos()
{
    mc_RollBackPos *rollback_pos=GetRollBackPos();
    if(rollback_pos == NULL)
    {
        return;
    }
    rollback_pos->Zero();
    
/*    
    Lock(1);

    m_RollBackPos.Zero();
    
    UnLock();    
 */ 
}



int mc_AssetDB::GetEntity(mc_EntityLedgerRow* row)
{    
    int err,value_len,mprow;
    int result;
    mc_EntityDBRow adbRow;
    mc_RollBackPos *rollback_pos=GetRollBackPos();
    
    unsigned char *ptr;

    adbRow.Zero();
    memcpy(adbRow.m_Key,row->m_Key,MC_ENT_KEY_SIZE);
    adbRow.m_KeyType=row->m_KeyType;
    ptr=(unsigned char*)m_Database->m_DB->Read((char*)row+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
    if(err)
    {
        return 0;
    }
    
    if(ptr)
    {         
        memcpy((char*)&adbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        
        if(m_Ledger->Open() <= 0)
        {
            return 0;
        }

        result=1;        
        if(m_Ledger->GetRow(adbRow.m_LedgerPos,row))
        {
            result=0;
        }
        
        row->m_ChainPos=adbRow.m_ChainPos;
        m_Ledger->Close();
        
        if(rollback_pos)
        {
            if(rollback_pos->InBlock())
            {
                if(rollback_pos->IsOut(row->m_Block,row->m_Offset))
                {
                    result=0;                
                }
            }
        }
        return result;
    }
    
    if((rollback_pos == NULL) || (rollback_pos->InBlock() == 0))
    {
        mprow=m_MemPool->Seek((unsigned char*)row);
        if(mprow>=0)
        {
            if( (((mc_EntityLedgerRow*)(m_MemPool->GetRow(mprow)))->m_KeyType  & MC_ENT_KEYTYPE_MASK) != MC_ENT_KEYTYPE_TXID)
            {
                mprow--;
                if(mprow>=0)
                {
                    if( (((mc_EntityLedgerRow*)(m_MemPool->GetRow(mprow)))->m_KeyType  & MC_ENT_KEYTYPE_MASK) != MC_ENT_KEYTYPE_TXID)
                    {
                        mprow--;                    
                        if(mprow>=0)
                        {
                            if( (((mc_EntityLedgerRow*)(m_MemPool->GetRow(mprow)))->m_KeyType  & MC_ENT_KEYTYPE_MASK) != MC_ENT_KEYTYPE_TXID)
                            {
                                mprow--;                    
                                if(mprow>=0)
                                {
                                    if( (((mc_EntityLedgerRow*)(m_MemPool->GetRow(mprow)))->m_KeyType  & MC_ENT_KEYTYPE_MASK) != MC_ENT_KEYTYPE_TXID)
                                    {
                                        mprow=-1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if(mprow<0)
            {
                return 0;
            }
            
            GetFromMemPool(row,mprow);
            return 1;
        }
    }

    return 0;
}

void mc_EntityDetails::Copy(mc_EntityDetails *entity)
{    
    memcpy(this,entity,sizeof(mc_EntityDetails));
    m_LedgerRow.SetScript(entity->GetScript(),entity->GetScriptSize());
}

void mc_EntityDetails::Set(mc_EntityLedgerRow* row)
{    
    uint32_t block,i;
    int offset,script_size;
    uint32_t value_offset;
    size_t value_size;
    unsigned char dname_buf[6];
    
    Zero();
    
    m_LedgerRow.Copy(row);
    
    block=row->m_Block;
    offset=row->m_Offset;    
    script_size=row->m_ScriptSize;
    
    m_Flags=0;
    
    if(offset)
    {
        mc_PutLE(m_Ref,&block,4);
        mc_PutLE(m_Ref+4,&offset,4);
        for(i=0;i<MC_ENT_REF_PREFIX_SIZE;i++)
        {
            m_Ref[8+i]=*(row->m_Key+MC_ENT_KEY_SIZE-1-i);
        }
        m_Flags |= MC_ENT_FLAG_OFFSET_IS_SET;
    }

    m_Permissions=0;
    m_Restrictions=0;
    m_ScriptPermissions=0;
    
    switch(m_LedgerRow.m_EntityType)
    {
        case MC_ENT_TYPE_ASSET:
            m_Permissions |= MC_PTP_ADMIN | MC_PTP_ISSUE;
            if(mc_gState->m_Features->PerAssetPermissions())
            {
                m_Permissions |= MC_PTP_ACTIVATE;                
            }
            break;
        case MC_ENT_TYPE_STREAM:
            m_Permissions |= MC_PTP_ADMIN | MC_PTP_ACTIVATE | MC_PTP_WRITE;
            if(mc_gState->m_Features->ReadPermissions())
            {
                m_Permissions |= MC_PTP_READ;                
            }
            break;
        case MC_ENT_TYPE_VARIABLE:
            m_Permissions |= MC_PTP_ADMIN | MC_PTP_ACTIVATE | MC_PTP_WRITE;
            break;
        case MC_ENT_TYPE_LIBRARY:
            m_Permissions |= MC_PTP_ADMIN | MC_PTP_ACTIVATE | MC_PTP_WRITE;
            break;
        default:
            if(m_LedgerRow.m_EntityType <= MC_ENT_TYPE_STREAM_MAX)
            {
                m_Permissions = MC_PTP_WRITE | MC_PTP_ACTIVATE;
                if(mc_gState->m_Features->ReadPermissions())
                {
                    m_Permissions |= MC_PTP_READ;                
                }
            }
            break;            
    }
            
    if(script_size)
    {
        value_offset=mc_FindSpecialParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,MC_ENT_SPRM_NAME,&value_size);
        if(value_offset == m_LedgerRow.m_ScriptSize)
        {
            strcpy((char*)dname_buf+1,"name");
            dname_buf[0]=0xff;
            value_offset=mc_FindNamedParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,(char*)dname_buf,&value_size);
        }
        if(value_offset < m_LedgerRow.m_ScriptSize)
        {            
            if(value_size == 2)
            {
                if((char)m_LedgerRow.m_Script[value_offset] == '*')
                {
                    value_offset=m_LedgerRow.m_ScriptSize;
                    value_size=0;
                }
            }
        }
        if(value_offset < m_LedgerRow.m_ScriptSize)
        {            
            if(value_size > MC_ENT_MAX_NAME_SIZE)
            {
                value_size=MC_ENT_MAX_NAME_SIZE;
                memcpy(m_Name,m_LedgerRow.m_Script+value_offset,value_size);
                m_Name[value_size]=0;
            }
            else
            {
                memcpy(m_Name,m_LedgerRow.m_Script+value_offset,value_size);                
            }
            mc_StringLowerCase(m_Name,value_size);
            m_Flags |= MC_ENT_FLAG_NAME_IS_SET;
        }
        
        value_offset=mc_FindSpecialParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,MC_ENT_SPRM_PERMISSIONS,&value_size);
        if(value_offset <= m_LedgerRow.m_ScriptSize)
        {
            if( (value_offset != m_LedgerRow.m_ScriptSize) || (mc_gState->m_Features->FixedLegacyPermissionRestrictionFlag() == 0)) 
            {
                m_Permissions |= MC_PTP_SPECIFIED;
                if((value_size>0) && (value_size<=4))
                {
                    m_ScriptPermissions=(uint32_t)mc_GetLE(m_LedgerRow.m_Script+value_offset,value_size);
                    m_Permissions |= m_ScriptPermissions;
                }
            }
        }
                            
        value_offset=mc_FindSpecialParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,MC_ENT_SPRM_RESTRICTIONS,&value_size);
        if(value_offset <= m_LedgerRow.m_ScriptSize)
        {
            if( (value_offset != m_LedgerRow.m_ScriptSize) || (mc_gState->m_Features->FixedLegacyPermissionRestrictionFlag() == 0))
            {
                if((value_size>0) && (value_size<=4))
                {
                    m_Restrictions |= (uint32_t)mc_GetLE(m_LedgerRow.m_Script+value_offset,value_size);
                }
            }
        }
        
    }
    
    mc_ZeroABRaw(m_FullRef);
    memcpy(m_FullRef+MC_AST_SHORT_TXID_OFFSET,m_LedgerRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    mc_SetABRefType(m_FullRef,MC_AST_ASSET_REF_TYPE_SHORT_TXID);
}

void mc_AssetDB::AddToMemPool(mc_EntityLedgerRow* row)
{
    if((row->m_ScriptSize>m_Ledger->m_MaxScriptMemPoolSize) || 
       ((row->m_Script != NULL) && (row->m_Script != row->m_StaticScript)) )
    {
        if(row->m_ScriptMemPoolPos == 0)
        {
            row->m_ScriptMemPoolPos=m_ExtendedScripts->GetNumElements();
            mc_gState->m_Assets->m_ExtendedScripts->AddElement();
            mc_gState->m_Assets->m_ExtendedScripts->SetData(row->m_Script,row->m_ScriptSize);            
        }
    }

    m_MemPool->Add((unsigned char*)row,(unsigned char*)row+m_Ledger->m_ValueOffset);
}

void mc_AssetDB::GetFromMemPool(mc_EntityLedgerRow *row,int mprow)
{
    row->ReleaseScriptPointer();
    row->Zero();
    memcpy(row,m_MemPool->GetRow(mprow),m_Ledger->m_MemPoolSize);
    row->m_Script=row->m_StaticScript;
    
    if(row->m_ScriptMemPoolPos)
    {
        row->SetScript(m_ExtendedScripts->GetData(row->m_ScriptMemPoolPos,NULL),row->m_ScriptSize);
    }               
}


int mc_AssetDB::InsertEntity(const void* txid, int offset, int entity_type, const void *script,size_t script_size, const void* special_script, size_t special_script_size,int32_t extended_script_row,int update_mempool,uint32_t flags)
{
    mc_EntityLedgerRow aldRow;
    mc_EntityDetails details;
    
    Lock(1);
    int err=MC_ERR_NOERROR;
    
    int pass;
    uint32_t value_offset;
    size_t value_size;
    char stream_name[MC_ENT_MAX_NAME_SIZE+1];
    uint32_t upgrade_start_block;
    unsigned char full_script[MC_ENT_SCRIPT_ALLOC_SIZE];
    
    aldRow.Zero();
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;
    aldRow.m_Block=m_Block+1;
    aldRow.m_Offset=offset;
    if(offset<0)
    {
        aldRow.m_Offset=-(m_MemPool->GetCount()+1);
    }
    aldRow.m_Quantity=0;
    aldRow.m_EntityType=entity_type;
    aldRow.m_FirstPos=-(m_MemPool->GetCount()+1);//-1;
    aldRow.m_LastPos=0;
    aldRow.m_ChainPos=-1;
    aldRow.m_PrevPos=-1;   
    aldRow.m_ExtendedScript=0;
    if(extended_script_row)
    {
        size_t extended_script_size=0;        
        m_ExtendedScripts->GetData(extended_script_row,&extended_script_size);     
        aldRow.m_ExtendedScript=extended_script_size;
        aldRow.m_ExtendedScriptMemPoolPos=extended_script_row;
    }
    
    mc_Script *lpDetails;
    lpDetails=new mc_Script;
    lpDetails->AddElement();

    if(special_script_size)
    {
        lpDetails->SetData((const unsigned char*)special_script,special_script_size);
    }
        
    if(script)
    {            
        value_offset=mc_FindSpecialParamInDetailsScript((unsigned char*)script,script_size,MC_ENT_SPRM_NAME,&value_size);
        if(value_offset != script_size)
        {
            if(value_size)
            {
                if(*((unsigned char*)script+value_offset+value_size-1))
                {
                    if(value_size > MC_ENT_MAX_NAME_SIZE)
                    {
                        if(mc_gState->m_Features->FixedIn1001120003())
                        {
                            err=MC_ERR_ERROR_IN_SCRIPT;                                            
                            goto exitlbl;
                        }
                        value_size=MC_ENT_MAX_NAME_SIZE;
                    }
                    memcpy(stream_name,(unsigned char*)script+value_offset,value_size);
                    stream_name[value_size]=0x00;
                    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(unsigned char*)stream_name,strlen(stream_name)+1);            
                }
            }
        }
    }

    upgrade_start_block=0;
    if(entity_type == MC_ENT_TYPE_UPGRADE)
    {
        if(script)
        {
            if(mc_gState->m_Features->ParameterUpgrades() == 0)
            {
                value_offset=mc_FindSpecialParamInDetailsScript((unsigned char*)script,script_size,MC_ENT_SPRM_UPGRADE_PROTOCOL_VERSION,&value_size);
                if(value_offset == script_size)
                {
                    err=MC_ERR_ERROR_IN_SCRIPT;                                            
                    goto exitlbl;
                }
                if( (value_size <=0) || (value_size > 4) )
                {
                    err=MC_ERR_ERROR_IN_SCRIPT;                                            
                    goto exitlbl;
                }
                if((int)mc_GetLE((unsigned char*)script+value_offset,value_size) < 0)
                {
                    err=MC_ERR_ERROR_IN_SCRIPT;                                            
                    goto exitlbl;
                }
            }
            value_offset=mc_FindSpecialParamInDetailsScript((unsigned char*)script,script_size,MC_ENT_SPRM_UPGRADE_START_BLOCK,&value_size);
            if(value_offset != script_size)
            {
                if( (value_size <=0) || (value_size > 4) )
                {
                    err=MC_ERR_ERROR_IN_SCRIPT;                                            
                    goto exitlbl;
                }
                upgrade_start_block=(uint32_t)mc_GetLE((unsigned char*)script+value_offset,value_size);
            }
        }
    }

    
    if(lpDetails->m_Size)
    {
        memcpy(full_script,lpDetails->GetData(0,NULL),lpDetails->m_Size);        
    }    

    aldRow.m_ScriptSize=script_size+lpDetails->m_Size;
    if(script_size)
    {
        memcpy(full_script+lpDetails->m_Size,script,script_size);        
    }
    
    aldRow.SetScript(full_script,aldRow.m_ScriptSize);
    
    delete lpDetails;
    
    details.Set(&aldRow);
    details.m_Flags |= flags;
    
    for(pass=0;pass<1+update_mempool;pass++)
    {
        memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
        memcpy(aldRow.m_Key,details.m_LedgerRow.m_Key,MC_ENT_KEY_SIZE);
        aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;

        if(pass)
        {
            AddToMemPool(&aldRow);
        }
        else
        {
            if(GetEntity(&aldRow))
            {
                err=MC_ERR_FOUND;                                            
                goto exitlbl;
            }            
        }

        if((details.m_Flags & MC_ENT_FLAG_NO_OFFSET_KEY) == 0)
        {
            if(details.m_Flags & MC_ENT_FLAG_OFFSET_IS_SET)
            {
                memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
                memcpy(aldRow.m_Key,details.m_Ref,MC_ENT_REF_SIZE);
                aldRow.m_KeyType=MC_ENT_KEYTYPE_REF;

                if(pass)
                {
                    AddToMemPool(&aldRow);                
                }
                else
                {
                    if(GetEntity(&aldRow))
                    {
                        err=MC_ERR_FOUND;                                            
                        goto exitlbl;
                    }            
                }
            }
        }

        if(details.m_Flags & MC_ENT_FLAG_NAME_IS_SET)
        {
            memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
            memcpy(aldRow.m_Key,details.m_Name,MC_ENT_MAX_NAME_SIZE);
            aldRow.m_KeyType=MC_ENT_KEYTYPE_NAME;
                        
            if(pass)
            {
                AddToMemPool(&aldRow);
            }
            else
            {
                if(GetEntity(&aldRow))
                {
                    err=MC_ERR_FOUND;                                            
                    goto exitlbl;
                }            
            }
        }

        memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
        memcpy(aldRow.m_Key,details.m_LedgerRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        aldRow.m_KeyType=MC_ENT_KEYTYPE_SHORT_TXID;

        if(pass)
        {
            AddToMemPool(&aldRow);
        }
        else
        {
            if(GetEntity(&aldRow))
            {
                err=MC_ERR_FOUND;                                            
                goto exitlbl;
            }            
        }
    }    

    if(entity_type == MC_ENT_TYPE_UPGRADE)
    {
        err=mc_gState->m_Permissions->SetApproval((unsigned char*)txid+MC_AST_SHORT_TXID_OFFSET,1,NULL,upgrade_start_block,mc_TimeNowAsUInt(),MC_PFL_ENTITY_GENESIS,update_mempool,offset);
        goto exitlbl;
    }
    
    err=UpdateEntityLists(txid,offset,entity_type);    
    
exitlbl:
    
    UnLock();
    return err;
}


int mc_AssetDB::InsertAsset(const void* txid, int offset, int asset_type, uint64_t quantity, const char *name, int multiple, const void* script, size_t script_size, const void* special_script, size_t special_script_size,int32_t extended_script_row,int update_mempool)
{
    mc_EntityLedgerRow aldRow;
    mc_EntityDetails details;
    
    Lock(1);
    int err=MC_ERR_NOERROR;
    
    int pass;
    uint32_t value_offset;
    size_t value_size;
    int add_param;
    unsigned char full_script[MC_ENT_SCRIPT_ALLOC_SIZE];
    
    aldRow.Zero();
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;
    aldRow.m_Block=m_Block+1;
    aldRow.m_Offset=offset;
    if(offset<0)
    {
        aldRow.m_Offset=-(m_MemPool->GetCount()+1);
    }
    aldRow.m_Quantity=quantity;
    aldRow.m_EntityType=asset_type;
    aldRow.m_FirstPos=-(m_MemPool->GetCount()+1);//-1;                          // Unconfirmed issue, from 10007 we can create followons for them, so we should differentiate
    aldRow.m_LastPos=0;
    aldRow.m_ChainPos=-1;
    aldRow.m_PrevPos=-1;
    aldRow.m_ExtendedScript=0;
    if(extended_script_row)
    {
        size_t extended_script_size=0;        
        m_ExtendedScripts->GetData(extended_script_row,&extended_script_size);     
        aldRow.m_ExtendedScript=extended_script_size;
        aldRow.m_ExtendedScriptMemPoolPos=extended_script_row;
    }
    
    mc_Script *lpDetails;
    lpDetails=new mc_Script;
    lpDetails->AddElement();

    if(special_script_size)
    {
        lpDetails->SetData((const unsigned char*)special_script,special_script_size);
    }

    add_param=true;
    if(script)
    {
        if(mc_FindSpecialParamInDetailsScript((unsigned char*)script,script_size,MC_ENT_SPRM_ASSET_MULTIPLE,&value_size) != script_size)
        {
            add_param=false;                                
        }
    }

    if(add_param)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_MULTIPLE,(unsigned char*)&multiple,sizeof(multiple));
    }
    
    add_param=true;
    if(script)
    {            
        value_offset=mc_FindSpecialParamInDetailsScript((unsigned char*)script,script_size,MC_ENT_SPRM_NAME,&value_size);
        if(value_offset != script_size)
        {
            if(value_size)
            {
                if(*((unsigned char*)script+value_offset+value_size-1) == 0)
                {
                    add_param=false;                                                        
                }
            }
        }
    }

    if(add_param)
    {
        if(name && (strlen(name) > 0))
        {
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,(unsigned char*)name,strlen(name)+1);            
        }
    }
    
    if(lpDetails->m_Size)
    {
        memcpy(full_script,lpDetails->GetData(0,NULL),lpDetails->m_Size);        
    }    

    aldRow.m_ScriptSize=script_size+lpDetails->m_Size;
    if(script_size)
    {
        memcpy(full_script+lpDetails->m_Size,script,script_size);        
    }

    aldRow.SetScript(full_script,aldRow.m_ScriptSize);
    
    delete lpDetails;
    
    details.Set(&aldRow);
        
    for(pass=0;pass<1+update_mempool;pass++)
    {
        memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
        memcpy(aldRow.m_Key,details.m_LedgerRow.m_Key,MC_ENT_KEY_SIZE);
        aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;

        if(pass)
        {
            AddToMemPool(&aldRow);                
        }
        else
        {
            if(GetEntity(&aldRow))
            {
                err=MC_ERR_FOUND;                                            
                goto exitlbl;
            }            
        }


        if(details.m_Flags & MC_ENT_FLAG_OFFSET_IS_SET)
        {
            memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
            memcpy(aldRow.m_Key,details.m_Ref,MC_ENT_REF_SIZE);
            aldRow.m_KeyType=MC_ENT_KEYTYPE_REF;
            
            if(pass)
            {
                AddToMemPool(&aldRow);                
            }
            else
            {
                if(GetEntity(&aldRow))
                {
                    err=MC_ERR_FOUND;                                            
                    goto exitlbl;
                }            
            }
        }

        if(details.m_Flags & MC_ENT_FLAG_NAME_IS_SET)
        {
            memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
            memcpy(aldRow.m_Key,details.m_Name,MC_ENT_MAX_NAME_SIZE);
            aldRow.m_KeyType=MC_ENT_KEYTYPE_NAME;
                        
            if(pass)
            {
                AddToMemPool(&aldRow);                
            }
            else
            {
                if(GetEntity(&aldRow))
                {
                    err=MC_ERR_FOUND;                                            
                    goto exitlbl;
                }            
            }
        }
        
        memset(aldRow.m_Key,0,MC_ENT_KEY_SIZE);
        memcpy(aldRow.m_Key,details.m_LedgerRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        aldRow.m_KeyType=MC_ENT_KEYTYPE_SHORT_TXID;

        if(pass)
        {
            AddToMemPool(&aldRow);                
        }
        else
        {
            if(GetEntity(&aldRow))
            {
                err=MC_ERR_FOUND;                                            
                goto exitlbl;
            }            
        }
        
    }    

    err=UpdateEntityLists(txid,offset,asset_type);    
    
exitlbl:
    
    UnLock();
    return err;
}

int mc_AssetDB::InsertAssetFollowOn(const void* txid, int offset, uint64_t quantity, const void* script, size_t script_size, const void* special_script, size_t special_script_size,int32_t extended_script_row, const void* original_txid, int update_mempool)
{
    mc_EntityLedgerRow aldRow;
    
    Lock(1);
    int err=MC_ERR_NOERROR;
    
    int pass;
    int64_t size,first_pos,last_pos,tot_pos;
    uint64_t value_offset;
    size_t value_size;
    int64_t total;
    uint32_t entity_type;
    int last_found=0;
    int64_t gap_size=0;
    int lrow=m_MemPool->GetCount()-1;
    int last_last=0;
    unsigned char full_script[MC_ENT_SCRIPT_ALLOC_SIZE];

    aldRow.Zero();
    memcpy(aldRow.m_Key,original_txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;
    if(!GetEntity(&aldRow))
    {        
        err=MC_ERR_NOT_FOUND;                                            
        goto exitlbl;
    }    
    
    value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_FOLLOW_ONS,&value_size);
    if(value_offset == aldRow.m_ScriptSize)
    {
        err=MC_ERR_NOT_ALLOWED;                                            
        goto exitlbl;
    }
    if( (value_size==0) || (value_size > 4))
    {
        err=MC_ERR_INVALID_PARAMETER_VALUE;                                            
        goto exitlbl;
    }
    if(mc_GetLE(aldRow.m_Script+value_offset,value_size) == 0)
    {
        err=MC_ERR_NOT_ALLOWED;                                            
        goto exitlbl;
    }
    
    total=GetTotalQuantity(&aldRow,NULL);
    if((int64_t)(total+quantity)<0)
    {
        err=MC_ERR_INVALID_PARAMETER_VALUE;                                            
        goto exitlbl;
    }        
    
    
    first_pos=aldRow.m_FirstPos;
    last_pos=aldRow.m_ChainPos;
    entity_type=aldRow.m_EntityType;
    
    tot_pos=m_Pos;
/*    
    for(int i=0;i<m_MemPool->GetCount();i++)
    {
        if( (((mc_EntityLedgerRow*)(m_MemPool->GetRow(i)))->m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
        {
            mc_EntityLedgerRow* row=(mc_EntityLedgerRow*)(m_MemPool->GetRow(i));
            size=m_Ledger->m_TotalSize+mc_AllocSize(row->m_ScriptSize+row->m_ExtendedScript,m_Ledger->m_TotalSize,1);
            if( ((mc_EntityLedgerRow*)(m_MemPool->GetRow(i)))->m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON )
            {
                if(((mc_EntityLedgerRow*)(m_MemPool->GetRow(i)))->m_FirstPos == first_pos)
                {
                    last_pos=tot_pos;
                }
            }
            tot_pos+=size;
        }        
    }
*/    
    
    while(lrow>=0)
    {
        if( (((mc_EntityLedgerRow*)(m_MemPool->GetRow(lrow)))->m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
        {
            mc_EntityLedgerRow* row=(mc_EntityLedgerRow*)(m_MemPool->GetRow(lrow));
            size=m_Ledger->m_TotalSize+mc_AllocSize(row->m_ScriptSize+row->m_ExtendedScript,m_Ledger->m_TotalSize,1);
            if(last_found)
            {
                gap_size+=size;
            }
            if( (row->m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON) || (first_pos < 0) )
            {
                if(row->m_FirstPos == first_pos)
                {
                    if(last_found)
                    {
                        last_pos=last_last+gap_size;
                        lrow=-1;
                    }
                    else
                    {
                        last_found=1;
                        last_last=row->m_LastPos;
                    }
                }
            }
        }        
        lrow--;
    }
    if(last_found)
    {
        if(last_pos == aldRow.m_ChainPos)                                       // Single followon in memory
        {
            last_pos=tot_pos+gap_size;
        }
    }
    
    aldRow.ReleaseScriptPointer();
    aldRow.Zero();
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_FOLLOW_ON | MC_ENT_KEYTYPE_TXID;    
    if( (entity_type == MC_ENT_TYPE_VARIABLE) || (entity_type == MC_ENT_TYPE_LIBRARY) )
    {
        aldRow.m_KeyType |= MC_ENT_KEYTYPE_FOLLOW_MULTI;
    }
    aldRow.m_Block=m_Block+1;
    aldRow.m_Offset=offset;
    if(offset<0)
    {
        aldRow.m_Offset=-(m_MemPool->GetCount()+1);
    }
    aldRow.m_Quantity=quantity;
    aldRow.m_EntityType=entity_type;
    aldRow.m_FirstPos=first_pos;
    aldRow.m_LastPos=last_pos;
    aldRow.m_ChainPos=-1;
    aldRow.m_PrevPos=-1;
    aldRow.m_ExtendedScript=0;
    if(extended_script_row)
    {
        size_t extended_script_size=0;        
        m_ExtendedScripts->GetData(extended_script_row,&extended_script_size);     
        aldRow.m_ExtendedScript=extended_script_size;
        aldRow.m_ExtendedScriptMemPoolPos=extended_script_row;
    }
    
    mc_Script *lpDetails;
    lpDetails=new mc_Script;
    lpDetails->AddElement();

    if(special_script_size)
    {
        lpDetails->SetData((const unsigned char*)special_script,special_script_size);
    }
    
    if(lpDetails->m_Size)
    {
        memcpy(full_script,lpDetails->GetData(0,NULL),lpDetails->m_Size);        
    }    

    aldRow.m_ScriptSize=script_size+lpDetails->m_Size;
    if(script_size)
    {
        memcpy(full_script+lpDetails->m_Size,script,script_size);        
    }
    
    aldRow.SetScript(full_script,aldRow.m_ScriptSize);
    
    delete lpDetails;
    
    for(pass=1;pass<1+update_mempool;pass++)
    {
        if(pass)
        {
            AddToMemPool(&aldRow);                
        }
        else
        {
            if(GetEntity(&aldRow))
            {
                err=MC_ERR_FOUND;                                            
                goto exitlbl;
            }            
        }
    }    

exitlbl:
    
    UnLock();
    return err;
}

void mc_SetEntityListKey(unsigned char *key,int entity_type)
{
    memcpy(key,entitylist_key_prefix,MC_ENT_KEY_SIZE);
    mc_PutLE(key+MC_ENT_KEY_SIZE-12,&entity_type,4);
}

int mc_AssetDB::FindEntityList(mc_EntityDetails *entity,int entity_type)
{
    unsigned char key[MC_ENT_KEY_SIZE];
    mc_SetEntityListKey(key,entity_type);
    
    return FindEntityByTxID(entity,key);            
}

int mc_AssetDB::UpdateEntityLists(const void* txid,int offset,int entity_type)
{
    if( (m_Flags & MC_ENT_FLAG_ENTITYLIST) == 0)
    {
        return MC_ERR_NOERROR;        
    }
    
    unsigned char entitylist_key[MC_ENT_KEY_SIZE];
    mc_SetEntityListKey(entitylist_key,entity_type);
    
    mc_EntityLedgerRow aldRow;
    int64_t last_total,left_position;
    int32_t chain_size;
    int64_t size,first_pos,last_pos,tot_pos;
    
    last_total=0;
    chain_size=0;
    left_position=0;
    
    aldRow.Zero();
    memcpy(aldRow.m_Key,entitylist_key,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;
    if(!GetEntity(&aldRow))
    {                
        aldRow.Zero();
        memcpy(aldRow.m_Key,entitylist_key,MC_ENT_KEY_SIZE);
        aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;
        aldRow.m_Block=m_Block+1;
        aldRow.m_Offset=offset;
        if(offset<0)
        {
            aldRow.m_Offset=-(m_MemPool->GetCount()+1);
        }
        
        aldRow.m_Quantity=0;
        aldRow.m_EntityType=MC_ENT_TYPE_ENTITYLIST;
        aldRow.m_FirstPos=-(m_MemPool->GetCount()+1);
        aldRow.m_LastPos=0;
        aldRow.m_ChainPos=-1;
        aldRow.m_PrevPos=-1;
        aldRow.m_ExtendedScript=0;
    
        mc_Script *lpDetails;
        lpDetails=new mc_Script;
        lpDetails->AddElement();

        unsigned char b=MC_ENT_FOMD_ALLOWED_INSTANT;        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_FOLLOW_ONS,&b,1);
        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ENTITY_TXID,(unsigned char*)txid,MC_ENT_KEY_SIZE);    
        
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_TOTAL,(unsigned char*)&last_total,sizeof(last_total));                            
        
        if(chain_size >= 0)
        {
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_CHAIN_INDEX,(unsigned char*)&chain_size,sizeof(chain_size));                            
        }
        
        if(left_position > 0)
        {
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LEFT_POSITION,(unsigned char*)&left_position,sizeof(left_position));                            
        }
        
        if(lpDetails->m_Size)
        {
            aldRow.SetScript(lpDetails->GetData(0,NULL),lpDetails->m_Size); 
        }    
    
        aldRow.m_ScriptSize=lpDetails->m_Size;    
        delete lpDetails;        
        
        AddToMemPool(&aldRow);                        

        return MC_ERR_NOERROR;
    }    
    
    last_total=GetTotalQuantity(&aldRow,&chain_size);
    
    mc_PutLE(entitylist_key+MC_ENT_KEY_SIZE-8,&chain_size,sizeof(int32_t));    
    
    left_position=mc_gState->m_Assets->GetChainLeftPosition(&aldRow,chain_size);

    first_pos=aldRow.m_FirstPos;
    last_pos=aldRow.m_ChainPos;
    
    tot_pos=m_Pos;
    
    int last_found=0;
    int64_t gap_size=0;
    int lrow=m_MemPool->GetCount()-1;
    int last_last=0;
    while(lrow>=0)
    {
        if( (((mc_EntityLedgerRow*)(m_MemPool->GetRow(lrow)))->m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
        {
            mc_EntityLedgerRow* row=(mc_EntityLedgerRow*)(m_MemPool->GetRow(lrow));
            size=m_Ledger->m_TotalSize+mc_AllocSize(row->m_ScriptSize+row->m_ExtendedScript,m_Ledger->m_TotalSize,1);
            if(last_found)
            {
                gap_size+=size;
            }
            if( (row->m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON) || (first_pos < 0) )
            {
                if(row->m_FirstPos == first_pos)
                {
                    if(last_found)
                    {
                        last_pos=last_last+gap_size;
                        lrow=-1;
                    }
                    else
                    {
                        last_found=1;
                        last_last=row->m_LastPos;
                    }
                }
            }
        }        
        lrow--;
    }
    if(last_found)
    {
        if(last_pos == aldRow.m_ChainPos)                                       // Single followon in memory
        {
            last_pos=tot_pos+gap_size;
        }
    }
    
    aldRow.ReleaseScriptPointer();
    aldRow.Zero();
    memcpy(aldRow.m_Key,entitylist_key,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_FOLLOW_ON | MC_ENT_KEYTYPE_TXID | MC_ENT_KEYTYPE_FOLLOW_MULTI;    
    aldRow.m_Block=m_Block+1;
    aldRow.m_Offset=offset;
    if(offset<0)
    {
        aldRow.m_Offset=-(m_MemPool->GetCount()+1);
    }
    aldRow.m_Quantity=0;
    aldRow.m_EntityType=MC_ENT_TYPE_ENTITYLIST;
    aldRow.m_FirstPos=first_pos;
    aldRow.m_LastPos=last_pos;
    aldRow.m_ChainPos=-1;
    aldRow.m_PrevPos=-1;
    aldRow.m_ExtendedScript=0;
    
    mc_Script *lpDetails;
    lpDetails=new mc_Script;
    lpDetails->AddElement();

    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ENTITY_TXID,(unsigned char*)txid,MC_ENT_KEY_SIZE);

    lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ASSET_TOTAL,(unsigned char*)&last_total,sizeof(last_total));                            

    if(chain_size >= 0)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_CHAIN_INDEX,(unsigned char*)&chain_size,sizeof(chain_size));                            
    }

    if(left_position > 0)
    {
        lpDetails->SetSpecialParamValue(MC_ENT_SPRM_LEFT_POSITION,(unsigned char*)&left_position,sizeof(left_position));                            
    }
        
    if(lpDetails->m_Size)
    {
        aldRow.SetScript(lpDetails->GetData(0,NULL),lpDetails->m_Size); 
    }    
        
    aldRow.m_ScriptSize=lpDetails->m_Size;
    
    delete lpDetails;
    
    AddToMemPool(&aldRow);                        
    
    return MC_ERR_NOERROR;
}


int mc_AssetDB::Commit()
{ 
    int i,size,err,value_len;
    
    Lock(1);
    
    mc_EntityDBRow adbRow;
    mc_EntityLedgerRow aldRow;
    mc_EntityLedgerRow aldGenesisRow;
    mc_EntityDetails details;
    unsigned char *ptr;

    err=MC_ERR_NOERROR;    
    
    if(m_Ledger->Open() <= 0)
    {
        err=MC_ERR_DBOPEN_ERROR;                                            
        goto exitlbl;
    }

    if(m_MemPool->GetCount())
    {
        if(err == MC_ERR_NOERROR)
        {
            size=0;
            for(i=0;i<m_MemPool->GetCount();i++)
            {
                if(err == MC_ERR_NOERROR)
                {
                    GetFromMemPool(&aldRow,i);
                    aldGenesisRow.ReleaseScriptPointer();
                    aldGenesisRow.Zero();
                    if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
                    {
                        m_Pos+=size;
                        aldRow.m_PrevPos=m_PrevPos;
                        if(aldRow.m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON)
                        {
                            if(aldRow.m_FirstPos < 0)
                            {
                                GetFromMemPool(&aldGenesisRow,-aldRow.m_FirstPos-1);
                                aldRow.m_LastPos=aldGenesisRow.m_ChainPos;
                                aldGenesisRow.m_ChainPos=m_Pos;
                                memcpy(m_MemPool->GetRow(-aldRow.m_FirstPos-1),&aldGenesisRow,m_Ledger->m_MemPoolSize);     
                                aldRow.m_FirstPos=aldGenesisRow.m_FirstPos;
                            }
                            else
                            {
                                err=m_Ledger->GetRow(aldRow.m_FirstPos,&aldGenesisRow);                                
                            }
                        }
                        else
                        {
                            if(aldRow.m_FirstPos < 0)
                            {
                                aldRow.m_FirstPos=m_Pos;
                            }                            
                        }
                        if(aldRow.m_ChainPos < 0)
                        {
                            aldRow.m_ChainPos=m_Pos;
                        }
                        memcpy(m_MemPool->GetRow(i),&aldRow,m_Ledger->m_MemPoolSize);
                        m_PrevPos=m_Pos;
                        if(aldRow.m_Offset < 0)
                        {
                            err=MC_ERR_INTERNAL_ERROR;                                            
                            goto exitlbl;
                        }
                        else
                        {
                            size=m_Ledger->SetRow(m_PrevPos,&aldRow);
                            if(size<0)
                            {
                                err=MC_ERR_INTERNAL_ERROR;                                            
                                goto exitlbl;
                            }
                        }                    
                    }
                }
                
                if(err == MC_ERR_NOERROR)
                {
                    adbRow.Zero();
                    memcpy(adbRow.m_Key,aldRow.m_Key,MC_ENT_KEY_SIZE);
                    adbRow.m_KeyType=aldRow.m_KeyType;
                    adbRow.m_EntityType=aldRow.m_EntityType;
                    adbRow.m_Block=aldRow.m_Block;
                    adbRow.m_LedgerPos=m_Pos;
                    adbRow.m_ChainPos=m_Pos;

                    err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                    
                    if(err == MC_ERR_NOERROR)
                    {
                        if(aldGenesisRow.m_KeyType)
                        {
                            details.Set(&aldGenesisRow);
                            adbRow.Zero();
                            memcpy(adbRow.m_Key,aldGenesisRow.m_Key,MC_ENT_KEY_SIZE);
                            adbRow.m_KeyType=aldGenesisRow.m_KeyType;
                            adbRow.m_EntityType=aldGenesisRow.m_EntityType;
                            adbRow.m_Block=aldGenesisRow.m_Block;
                            adbRow.m_LedgerPos=aldGenesisRow.m_FirstPos;
                            adbRow.m_ChainPos=m_Pos;

                            err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                        (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                            
                            memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                            memcpy(adbRow.m_Key,details.m_LedgerRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                            adbRow.m_KeyType=MC_ENT_KEYTYPE_SHORT_TXID;
                            err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                            (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                            
                            if(details.m_Flags & MC_ENT_FLAG_OFFSET_IS_SET)
                            {
                                memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                                memcpy(adbRow.m_Key,details.m_Ref,MC_ENT_REF_SIZE);
                                adbRow.m_KeyType=MC_ENT_KEYTYPE_REF;                                    
                                err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                            }
                            if(details.m_Flags & MC_ENT_FLAG_NAME_IS_SET)
                            {
                                memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                                memcpy(adbRow.m_Key,details.m_Name,MC_ENT_MAX_NAME_SIZE);
                                adbRow.m_KeyType=MC_ENT_KEYTYPE_NAME;                                    
                                err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                            }
                        }
                    }
                }                       
            }
            m_Pos+=size;
        }        
    }
    
    if(err == MC_ERR_NOERROR)
    {
        m_Ledger->GetRow(0,&aldRow);
        aldRow.m_Block=m_Block+1;
        aldRow.m_PrevPos=m_PrevPos;
        m_Ledger->SetZeroRow(&aldRow);
    }
    
    m_Ledger->Flush();
    m_Ledger->Close();    
    
    if(err == MC_ERR_NOERROR)
    {
        adbRow.Zero();
        
        ptr=(unsigned char*)m_Database->m_DB->Read((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
        if(ptr)
        {
            memcpy((char*)&adbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        }

        
        adbRow.m_Block=m_Block+1;
        adbRow.m_LedgerPos=m_PrevPos;
        err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                        (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
    }        
    
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
    }    
    if(err)
    {
        RollBackInternal(m_Block);        
    }
    else
    {
        m_MemPool->Clear();
        m_Block++;
    }
    
exitlbl:
    
    UnLock();
    return err;
}

int mc_AssetDB::RollBack()
{
    return RollBack(m_Block-1);
}

int mc_AssetDB::RollBack(int block)
{
    Lock(1);
    
    int err=RollBackInternal(block);
    
    UnLock();
    return err;    
}

int mc_AssetDB::RollBackInternal(int block)
{
    int err;
    int take_it,value_len;
    int64_t this_pos,prev_pos,new_chain_pos;
    mc_EntityDBRow adbRow;
    mc_EntityLedgerRow aldRow;
    mc_EntityDetails details;
    unsigned char *ptr;
    
    err=MC_ERR_NOERROR;
    

    ClearMemPoolInternal();
    
    if(m_Ledger->Open() <= 0)
    {
        return MC_ERR_DBOPEN_ERROR;
    }
    
    this_pos=m_PrevPos;
    take_it=1;
    
    while(take_it && (this_pos>0))
    {
        m_Ledger->GetRow(this_pos,&aldRow);
        prev_pos=aldRow.m_PrevPos;
        details.Set(&aldRow);
        
        if(details.m_LedgerRow.m_Block > block)
        {
            adbRow.Zero();
            memcpy(adbRow.m_Key,aldRow.m_Key,MC_ENT_KEY_SIZE);
            adbRow.m_KeyType=aldRow.m_KeyType;
            err=m_Database->m_DB->Delete((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);

            adbRow.Zero();
            memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
            memcpy(adbRow.m_Key,aldRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
            adbRow.m_KeyType=MC_ENT_KEYTYPE_SHORT_TXID;
            err=m_Database->m_DB->Delete((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);                
            
            if((adbRow.m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON) == 0)
            {
                if( (details.m_Flags & MC_ENT_FLAG_OFFSET_IS_SET))
                {
                    memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                    memcpy(adbRow.m_Key,details.m_Ref,MC_ENT_REF_SIZE);
                    adbRow.m_KeyType=MC_ENT_KEYTYPE_REF;                                    
                    err=m_Database->m_DB->Delete((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                }

                if(details.m_Flags & MC_ENT_FLAG_NAME_IS_SET)
                {
                    memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                    memcpy(adbRow.m_Key,details.m_Name,MC_ENT_MAX_NAME_SIZE);
                    adbRow.m_KeyType=MC_ENT_KEYTYPE_NAME;                                    
                    err=m_Database->m_DB->Delete((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                }            
            }
            
            if(err == MC_ERR_NOERROR)
            {
                if( aldRow.m_KeyType  & MC_ENT_KEYTYPE_FOLLOW_ON )
                {
                    new_chain_pos=aldRow.m_LastPos;
                    err=m_Ledger->GetRow(aldRow.m_FirstPos,&aldRow);                        
                    details.Set(&aldRow);

                    adbRow.Zero();

                    memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                    memcpy(adbRow.m_Key,details.m_LedgerRow.m_Key,MC_ENT_KEY_SIZE);
                    adbRow.m_KeyType=MC_ENT_KEYTYPE_TXID;

                    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);

                    if(ptr)
                    {         
                        memcpy((char*)&adbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
                        adbRow.m_ChainPos=new_chain_pos;
                        err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                        (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                        
                        memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                        memcpy(adbRow.m_Key,details.m_LedgerRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
                        adbRow.m_KeyType=MC_ENT_KEYTYPE_SHORT_TXID;
                        err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                        (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                        if(details.m_Flags & MC_ENT_FLAG_OFFSET_IS_SET)
                        {
                            memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                            memcpy(adbRow.m_Key,details.m_Ref,MC_ENT_REF_SIZE);
                            adbRow.m_KeyType=MC_ENT_KEYTYPE_REF;                                    
                            err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                            (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                        }
                        if(details.m_Flags & MC_ENT_FLAG_NAME_IS_SET)
                        {
                            memset(adbRow.m_Key,0,MC_ENT_KEY_SIZE);
                            memcpy(adbRow.m_Key,details.m_Name,MC_ENT_MAX_NAME_SIZE);
                            adbRow.m_KeyType=MC_ENT_KEYTYPE_NAME;                                    
                            err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                            (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                        }
                    }
                    else
                    {
                        err=MC_ERR_INTERNAL_ERROR;
                    }
                }
            }
        }
        else
        {
            take_it=0;
        }
        if(err)
        {
            take_it=0;
        }
        if(take_it)
        {
            this_pos=prev_pos;
        }
    }

    m_PrevPos=this_pos;
    
    if(err == MC_ERR_NOERROR)
    {
        adbRow.Zero();
        
        ptr=(unsigned char*)m_Database->m_DB->Read((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
        if(ptr)
        {
            memcpy((char*)&adbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        }
            
        adbRow.m_Block=block;
        adbRow.m_LedgerPos=m_PrevPos;
        if(block<0)
        {
#ifndef MAC_OSX
            adbRow.m_Flags|=MC_ENT_FLAG_ENTITYLIST;
#endif        
            m_Flags=adbRow.m_Flags;
        }
        
        err=m_Database->m_DB->Write((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                        (char*)&adbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);        
    }        
    
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
    }    
    
    if(err == MC_ERR_NOERROR)
    {
        m_Ledger->GetRow(0,&aldRow);
        aldRow.m_Block=block;
        aldRow.m_PrevPos=m_PrevPos;
        m_Ledger->SetZeroRow(&aldRow);
    }
    
    if(err == MC_ERR_NOERROR)
    {
        m_Block=block;
        m_Ledger->GetRow(m_PrevPos,&aldRow);
        m_Pos=m_PrevPos+mc_AllocSize(m_Ledger->m_TotalSize+aldRow.m_ScriptSize+aldRow.m_ExtendedScript,m_Ledger->m_TotalSize,1);            
    }
    
    m_Ledger->Close();
    return err;   
}

int mc_AssetDB::ClearMemPool()
{
    Lock(1);
    
    int err=ClearMemPoolInternal();
    
    UnLock();
    return err;        
}

int mc_AssetDB::ClearMemPoolInternal()
{
    mc_EntityLedgerRow aldRow;
    
    if(m_MemPool)
    {
        if(m_MemPool->GetCount())
        {
            m_MemPool->Clear();
            if(m_Ledger->Open() <= 0)
            {
                return MC_ERR_DBOPEN_ERROR;
            }
        
            if(m_Ledger->GetRow(0,&aldRow))
            {
                return MC_ERR_CORRUPTED;        
            }
    
            m_PrevPos=aldRow.m_PrevPos;
            if(m_Ledger->GetRow(m_PrevPos,&aldRow))
            {
                return MC_ERR_CORRUPTED;        
            }
            
            m_Pos=m_PrevPos+mc_AllocSize(m_Ledger->m_TotalSize+aldRow.m_ScriptSize+aldRow.m_ExtendedScript,m_Ledger->m_TotalSize,1);            
            
            m_Ledger->Close();
        }
    }
    
    if(m_ExtendedScripts)
    {
        m_ExtendedScripts->Clear();
        m_ExtendedScripts->AddElement();
    }
    
    return MC_ERR_NOERROR;
}


int mc_AssetDB::FindEntityByTxID(mc_EntityDetails *entity,const unsigned char* txid)
{
    mc_EntityLedgerRow aldRow;

    Lock(0);
    int res=0;
    
    entity->Zero();
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;

    if(GetEntity(&aldRow))            
    {
        entity->Set(&aldRow);
        if(m_TmpRelevantEntities->GetCount())
        {
            m_TmpRelevantEntities->Add(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        }
        res=1;
        goto exitlbl;
    }            

    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID | MC_ENT_KEYTYPE_FOLLOW_ON;

    if(GetEntity(&aldRow))            
    {
        entity->Set(&aldRow);
        res=1;
        goto exitlbl;
    }            
    
    
exitlbl:
            
    UnLock();
    return res;        
}

int mc_AssetDB::FindEntityByShortTxID (mc_EntityDetails *entity, const unsigned char* short_txid)
{
    Lock(0);
    
    int res=FindEntityByShortTxIDInternal(entity,short_txid);
    
    UnLock();
    return res;        
}

int mc_AssetDB::FindEntityByShortTxIDInternal (mc_EntityDetails *entity, const unsigned char* short_txid)
{
    mc_EntityLedgerRow aldRow;

    int res=0;
    
    entity->Zero();
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,short_txid,MC_AST_SHORT_TXID_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_SHORT_TXID;

    if(GetEntity(&aldRow))            
    {
        entity->Set(&aldRow);
        if(m_TmpRelevantEntities->GetCount())
        {
            m_TmpRelevantEntities->Add(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        }
        res=1;
        goto exitlbl;
    }            

exitlbl:
            
    return res;        
}


unsigned char *mc_AssetDB::CachedTxIDFromShortTxID(unsigned char *short_txid)
{
    int row;
    mc_EntityDetails entity;
    
    Lock(1);
    unsigned char *ptr=NULL;    
    
    row=m_ShortTxIDCache->Seek(short_txid);
    if(row >= 0)
    {
        ptr=m_ShortTxIDCache->GetRow(row)+m_ShortTxIDCache->m_KeySize;
        goto exitlbl;
    }
    
    if(FindEntityByShortTxIDInternal(&entity,short_txid))
    {
        m_ShortTxIDCache->Add(short_txid,entity.GetTxID());
        ptr=m_ShortTxIDCache->GetRow(m_ShortTxIDCache->GetCount()-1)+m_ShortTxIDCache->m_KeySize;        
        goto exitlbl;
    }
    
exitlbl:
            
    UnLock();
    return ptr;        
}


int mc_AssetDB::FindEntityByRef (mc_EntityDetails *entity,const unsigned char* asset_ref)
{
    mc_EntityLedgerRow aldRow;

    Lock(0);
    int res=0;
    
    entity->Zero();
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,asset_ref,MC_ENT_REF_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_REF;

    if(GetEntity(&aldRow))            
    {
        entity->Set(&aldRow);
        if(m_TmpRelevantEntities->GetCount())
        {
            m_TmpRelevantEntities->Add(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        }
        res=1;
        goto exitlbl;
    }            

exitlbl:
            
    UnLock();
    return res;        
}

int mc_AssetDB::FindEntityByFullRef (mc_EntityDetails *entity, unsigned char* full_ref)
{
    entity->Zero();
    switch(mc_GetABRefType(full_ref))
    {
        case MC_AST_ASSET_REF_TYPE_REF:
            return FindEntityByRef(entity,full_ref);
        case MC_AST_ASSET_REF_TYPE_SHORT_TXID:
            return FindEntityByShortTxID(entity,full_ref+MC_AST_SHORT_TXID_OFFSET);
        case MC_AST_ASSET_REF_TYPE_TXID:
            return FindEntityByTxID(entity,full_ref);            
    }
    return false;
}


int mc_AssetDB::FindEntityByName(mc_EntityDetails *entity,const char* name)
{
    mc_EntityLedgerRow aldRow;

    Lock(0);
    int res=0;
    
    entity->Zero();
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,name,strlen(name));
    mc_StringLowerCase((char*)(aldRow.m_Key),MC_ENT_MAX_NAME_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_NAME;

    if(GetEntity(&aldRow))            
    {
        entity->Set(&aldRow);
        if(m_TmpRelevantEntities->GetCount())
        {
            m_TmpRelevantEntities->Add(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        }
        res=1;
        goto exitlbl;
    }            

exitlbl:
            
    UnLock();
    return res;        
}

int mc_AssetDB::FindLastEntityByGenesis(mc_EntityDetails *last_entity, mc_EntityDetails *genesis_entity)
{
    Lock(0);
    
    int res=FindLastEntityByGenesisInternal(last_entity,genesis_entity);
    
    UnLock();
    return res;        
}

int mc_AssetDB::FindLastEntityByGenesisInternal(mc_EntityDetails *last_entity, mc_EntityDetails *genesis_entity)
{
    mc_EntityLedgerRow aldRow;
    mc_RollBackPos *rollback_pos=GetRollBackPos();

    last_entity->Zero();
    
    int i,result;
    int64_t pos,first_pos;
    
    pos=genesis_entity->m_LedgerRow.m_ChainPos;
    first_pos=genesis_entity->m_LedgerRow.m_FirstPos;
    
    for(i=m_MemPool->GetCount()-1;i>=0;i--)
    {
        GetFromMemPool(&aldRow,i);

        if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
        {
            if(aldRow.m_FirstPos == first_pos)
            {
                pos=aldRow.m_LastPos;
                if((rollback_pos == NULL) || (rollback_pos->InBlock() == 0))
                {
                    last_entity->Set(&aldRow);                
                    return 1;
                }
            }
        }
    }
        
    result=0;
    if(pos > 0)
    {
        m_Ledger->Open();
        while(pos>0)
        {
            m_Ledger->GetRow(pos,&aldRow);
            if( (rollback_pos == NULL) || (rollback_pos->InBlock() == 0) || ((rollback_pos->IsOut(aldRow.m_Block,aldRow.m_Offset)) == 0) )
            {
                last_entity->Set(&aldRow);                
                pos=-1;
                result=1;
            }
            else
            {
                if(pos != first_pos)
                {
                    pos=aldRow.m_LastPos;
                }
                else
                {
                    pos=-1;
                }
            }
        }
        m_Ledger->Close();
    }

    return result;
}

int mc_AssetDB::FindLastEntity(mc_EntityDetails *last_entity, mc_EntityDetails *entity)
{
    mc_EntityDetails genesis_entity;
    
    Lock(0);
    
    genesis_entity.Zero();
    if(entity->IsFollowOn())
    {
        FindEntityByFollowOnInternal(&genesis_entity,entity->GetTxID());
    }
    else
    {
        genesis_entity.Copy(entity);
    }
    
    int res=FindLastEntityByGenesisInternal(last_entity,&genesis_entity);
    
    UnLock();
    return res;        
}

int mc_AssetDB::FindEntityByFollowOn(mc_EntityDetails *entity,const unsigned char* txid)
{
    Lock(0);
    
    int res=FindEntityByFollowOnInternal(entity,txid);
    
    UnLock();
    return res;        
}

int mc_AssetDB::FindEntityByFollowOnInternal(mc_EntityDetails *entity,const unsigned char* txid)
{
    mc_EntityLedgerRow aldRow;

    entity->Zero();
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_FOLLOW_ON | MC_ENT_KEYTYPE_TXID;

    if(GetEntity(&aldRow))            
    {
        m_Ledger->Open();
        
        if(aldRow.m_FirstPos < 0)
        {
            GetFromMemPool(&aldRow,-aldRow.m_FirstPos-1);
        }
        else
        {
            m_Ledger->GetRow(aldRow.m_FirstPos,&aldRow);
        }
        m_Ledger->Close();
        entity->Set(&aldRow);
        if(m_TmpRelevantEntities->GetCount())
        {
            m_TmpRelevantEntities->Add(entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET);
        }
        return 1;
    }            

    return 0;
}

const unsigned char* mc_EntityDetails::GetParamUpgrades(int *size)
{
    uint32_t value_offset;
    size_t value_size;
    
    if(m_LedgerRow.m_ScriptSize)
    {
        value_offset=mc_FindSpecialParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,MC_ENT_SPRM_UPGRADE_CHAIN_PARAMS,&value_size);
        if(value_offset != m_LedgerRow.m_ScriptSize)
        {
            *size=(int)value_size;
            return m_LedgerRow.m_Script+value_offset;
        }
    }
    
    *size=0;
    return NULL;
}

const char* mc_EntityDetails::GetName()
{
    uint32_t value_offset;
    size_t value_size;
    unsigned char dname_buf[6];
    
    if(m_LedgerRow.m_ScriptSize)
    {
        value_offset=mc_FindSpecialParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,MC_ENT_SPRM_NAME,&value_size);
        if(value_offset == m_LedgerRow.m_ScriptSize)
        {
            strcpy((char*)dname_buf+1,"name");
            dname_buf[0]=0xff;
            value_offset=mc_FindNamedParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,(char*)dname_buf,&value_size);
        }
        if(value_offset < m_LedgerRow.m_ScriptSize)
        {            
            if(value_size == 2)
            {
                if((char)m_LedgerRow.m_Script[value_offset] == '*')
                {
                    value_offset=m_LedgerRow.m_ScriptSize;
                    value_size=0;
                }
            }
        }
        if(value_offset < m_LedgerRow.m_ScriptSize)
        {            
            return (char*)(m_LedgerRow.m_Script+value_offset);
        }
    }
    
    return m_Name;
}

const char* mc_EntityDetails::GetUpdateName(size_t* bytes)
{
    return (char *)GetSpecialParam(MC_ENT_SPRM_UPDATE_NAME,bytes);
}

const unsigned char* mc_EntityDetails::GetParentTxID()
{
    unsigned char *ptr;
    size_t value_size;
    ptr=(unsigned char *)GetSpecialParam(MC_ENT_SPRM_PARENT_ENTITY,&value_size);
    if(ptr)
    {
        if(value_size == sizeof(uint256))
        {
            return ptr;
        }
    }
    return NULL;
}


const unsigned char* mc_EntityDetails::GetTxID()
{
    return m_LedgerRow.m_Key;
}

const unsigned char* mc_EntityDetails::GetRef()
{
    return m_Ref;
}

int mc_EntityDetails::IsUnconfirmedGenesis()
{
    if( (m_LedgerRow.m_Block > mc_gState->m_Assets->m_Block) && (m_LedgerRow.m_Offset >= 0) )                     // Can happen only when called from filter
    {
        return 1;
    }

    return ((int)mc_GetLE(m_Ref+4,4)<0) ? 1 : 0; 
}

const unsigned char* mc_EntityDetails::GetFullRef()
{
    return m_FullRef;
}

const unsigned char* mc_EntityDetails::GetShortRef()
{
    return GetTxID()+MC_AST_SHORT_TXID_OFFSET;
}

const unsigned char* mc_EntityDetails::GetScript()
{
    return m_LedgerRow.m_Script;
}

uint32_t mc_EntityDetails::GetScriptSize()
{
    return m_LedgerRow.m_ScriptSize;
}

int mc_EntityDetails::GetAssetMultiple()
{
    int multiple;
    size_t size;
    void* ptr;
    
    multiple=1; 

    ptr=NULL;
    ptr=(void*)GetSpecialParam(MC_ENT_SPRM_ASSET_MULTIPLE,&size);
    
    if(ptr)
    {
        if(size==sizeof(multiple))
        {
            multiple=(int)mc_GetLE(ptr,size);
        }
    }
        
    if(multiple <= 0)
    {
        multiple=1;
    }
    
    return multiple;
}

uint32_t mc_EntityDetails::GetFilterType()
{
    uint32_t filter_type=0;
    size_t size;
    void* ptr;
    
    ptr=NULL;
    ptr=(void*)GetSpecialParam(MC_ENT_SPRM_FILTER_TYPE,&size);
    
    if(ptr)
    {
        if(size==sizeof(filter_type))
        {
            filter_type=(uint32_t)mc_GetLE(ptr,size);
        }
    }
            
    return filter_type;
}

int mc_EntityDetails::IsFollowOn()
{
    if(m_LedgerRow.m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON)
    {
        return 1;
    }
    return 0;
}

int64_t mc_EntityDetails::MaxTotalIssuance()
{
    unsigned char *ptr;
    size_t bytes;
    if(mc_gState->m_Features->NFTokens())
    {
        ptr=(unsigned char *)GetSpecialParam(MC_ENT_SPRM_ASSET_MAX_TOTAL,&bytes);
        if(ptr)
        {
            if((bytes>0) && (bytes<=8))
            {
                return mc_GetLE(ptr,bytes);
            }
        }
    }
    return MC_ENT_DEFAULT_MAX_ASSET_TOTAL;    
}

int64_t mc_EntityDetails::MaxSingleIssuance()
{
    unsigned char *ptr;
    size_t bytes;
    if(mc_gState->m_Features->NFTokens())
    {
        ptr=(unsigned char *)GetSpecialParam(MC_ENT_SPRM_ASSET_MAX_ISSUE,&bytes);
        if(ptr)
        {
            if((bytes>0) && (bytes<=8))
            {
                return mc_GetLE(ptr,bytes);
            }
        }
    }
    return MC_ENT_DEFAULT_MAX_ASSET_TOTAL;    
}

int mc_EntityDetails::FollowonMode()
{
    unsigned char *ptr;
    size_t bytes;
    ptr=(unsigned char *)GetSpecialParam(MC_ENT_SPRM_FOLLOW_ONS,&bytes);
    if(ptr)
    {
        if((bytes>0) && (bytes<=4))
        {
            return (int)mc_GetLE(ptr,bytes);
        }
    }
    return 0;
}

int mc_EntityDetails::AllowedFollowOns()
{
    int mode=FollowonMode();
    int result=mode & (MC_ENT_FOMD_ALLOWED_INSTANT | MC_ENT_FOMD_ALLOWED_WITH_APPROVAL);
    
    if(mc_gState->m_Features->NFTokens())
    {
        if(mode & (MC_ENT_FOMD_CAN_OPEN | MC_ENT_FOMD_CAN_CLOSE))
        {
            int open=mc_gState->m_Permissions->DetailsFlag(GetTxID(),MC_PDF_ASSET_OPEN);
            if(mode & MC_ENT_FOMD_CAN_OPEN)
            {
                if(open)
                {
                    if(result == 0)
                    {
                        result=MC_ENT_FOMD_ALLOWED_INSTANT;
                    }
                    return result;
                }
            }
            if(mode & MC_ENT_FOMD_CAN_CLOSE)
            {
                if(open == 0)
                {
                    return 0;
                }
            }            
        }
    }
    
    return result;
}

int mc_EntityDetails::ApproveRequired()
{
    return FollowonMode() & MC_ENT_FOMD_ALLOWED_WITH_APPROVAL;    
}

int mc_EntityDetails::AnyoneCanIssueMore()
{
    if(mc_gState->m_Features->AnyoneCanIssueMore() == 0)
    {
        return 0;
    }
    return FollowonMode() & MC_ENT_FOMD_ANYONE_CAN_ISSUEMORE;
}

int mc_EntityDetails::IsNFTAsset()
{
    if(mc_gState->m_Features->NFTokens() == 0)
    {
        return 0;
    }
    return FollowonMode() & MC_ENT_FOMD_NON_FUNGIBLE_TOKENS;    
}

int mc_EntityDetails::AdminCanOpen()
{
    if(mc_gState->m_Features->NFTokens() == 0)
    {
        return 0;
    }
    return FollowonMode() & MC_ENT_FOMD_CAN_OPEN;    
}

int mc_EntityDetails::AdminCanClose()
{
    if(mc_gState->m_Features->NFTokens() == 0)
    {
        return 0;
    }
    return FollowonMode() & MC_ENT_FOMD_CAN_CLOSE;    
}

uint32_t mc_EntityDetails::Permissions()
{
    return m_Permissions;
}

uint32_t mc_EntityDetails::Restrictions()
{
    return m_Restrictions;
}


int mc_EntityDetails::AnyoneCanWrite()
{
    unsigned char *ptr;
    size_t bytes;

    if(m_Permissions & MC_PTP_SPECIFIED)
    {
        if(mc_gState->m_Features->OffChainData())
        {            
            if(m_ScriptPermissions & MC_PTP_WRITE)
            {
                return 0;
            }
            return 1;            
        }
    }
    
    ptr=(unsigned char *)GetSpecialParam(MC_ENT_SPRM_ANYONE_CAN_WRITE,&bytes);
    if(ptr)
    {
        if((bytes>0) && (bytes<=4))
        {
            return (int)mc_GetLE(ptr,bytes);
        }
    }
    return 0;
}

int mc_EntityDetails::AnyoneCanRead()
{
    if(m_LedgerRow.m_EntityType != MC_ENT_TYPE_STREAM)
    {
        return 1;
    }
    if(mc_gState->m_Features->ReadPermissions())
    {
        if(m_Permissions & MC_PTP_SPECIFIED)
        {
            if(m_ScriptPermissions & MC_PTP_READ)
            {
                return 0;
            }
        }
    }
    
    return 1;
}

uint32_t mc_EntityDetails::UpgradeStartBlock()
{
    unsigned char *ptr;
    size_t bytes;
    ptr=(unsigned char *)GetSpecialParam(MC_ENT_SPRM_UPGRADE_START_BLOCK,&bytes);
    if(ptr)
    {
        if((bytes>0) && (bytes<=4))
        {
            return (int)mc_GetLE(ptr,bytes);
        }
    }
    return 0;
}

int mc_EntityDetails::UpgradeProtocolVersion()
{
    unsigned char *ptr;
    size_t bytes;
    int version;
    ptr=(unsigned char *)GetSpecialParam(MC_ENT_SPRM_UPGRADE_PROTOCOL_VERSION,&bytes);
    if(ptr)
    {
        if((bytes>0) && (bytes<=4))
        {
            version=(int)mc_GetLE(ptr,bytes);
            if(version > 0)
            {
                return version;
            }
        }
    }
    return 0;
}


uint64_t mc_EntityDetails::GetQuantity()
{
    return m_LedgerRow.m_Quantity;
}

uint32_t mc_EntityDetails::GetEntityType()
{
    return m_LedgerRow.m_EntityType;
}

int32_t mc_EntityDetails::NextParam(uint32_t offset,uint32_t* param_value_start,size_t *bytes)
{
    int32_t new_offset;
    
    new_offset=(int32_t)mc_GetParamFromDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,offset,param_value_start,bytes);
    if(new_offset == (int32_t)m_LedgerRow.m_ScriptSize)
    {
        new_offset = -1;
    }
    
    return new_offset;
}

const void* mc_EntityDetails::GetSpecialParam(uint32_t param,size_t* bytes)
{
    return GetSpecialParam(param,bytes,0);
}

const void* mc_EntityDetails::GetSpecialParam(uint32_t param,size_t* bytes,int check_extended_script)
{
    uint32_t offset;
    offset=mc_FindSpecialParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,param,bytes);
    if(offset == m_LedgerRow.m_ScriptSize)
    {
        if(check_extended_script)
        {
            if(m_LedgerRow.m_ExtendedScript)
            {
                if(mc_gState->m_Features->ExtendedEntityDetails())
                {
                    if(m_LedgerRow.m_ExtendedScriptMemPoolPos == 0)
                    {                        
                        mc_Script *script=mc_gState->m_Assets->m_RowExtendedScripts->GetScriptPointer(MC_AST_ASSET_SCRIPT_POS_EXTENDED_SCRIPT);
                        if(script)
                        {
//                            offset=mc_FindSpecialParamInDetailsScript(mc_gState->m_Assets->m_RowExtendedScript->m_lpData,m_LedgerRow.m_ExtendedScript,param,bytes);
                            offset=mc_FindSpecialParamInDetailsScript(script->m_lpData,m_LedgerRow.m_ExtendedScript,param,bytes);
                            if((int)offset != m_LedgerRow.m_ExtendedScript)
                            {
                                return script->m_lpData+offset;
//                                return mc_gState->m_Assets->m_RowExtendedScript->m_lpData+offset;
                            }                
                        }
                    }
                    else
                    {
                        const unsigned char *ptr;
                        size_t script_bytes;
                        ptr=mc_gState->m_Assets->m_ExtendedScripts->GetData(m_LedgerRow.m_ExtendedScriptMemPoolPos,&script_bytes);
                        offset=mc_FindSpecialParamInDetailsScript(ptr,script_bytes,param,bytes);
                        if(offset != script_bytes)
                        {
                            return ptr+offset;
                        }                
                        
                    }
                }            
            }
        }
        return NULL;
    }
    return m_LedgerRow.m_Script+offset;
}

const void* mc_EntityDetails::GetParam(const char *param,size_t* bytes)
{
    uint32_t offset;
    offset=mc_FindNamedParamInDetailsScript(m_LedgerRow.m_Script,m_LedgerRow.m_ScriptSize,param,bytes);
    if(offset == m_LedgerRow.m_ScriptSize)
    {
        return NULL;
    }
    return m_LedgerRow.m_Script+offset;
}

void mc_AssetDB::Dump()
{    
    mc_EntityDBRow adbRow;
    mc_EntityLedgerRow aldRow;
    unsigned char *ptr;       
    int dbvalue_len,err,i;
    int64_t pos,total;
    int size,row_size;
    
    Lock(0);
    
    printf("\nDB\n");
    adbRow.Zero();
    
    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(err)
    {
        goto exitlbl;
    }

    row_size=m_Database->m_TotalSize;
    while(row_size > 64)
    {
        row_size /= 2;
    }
    if(ptr)
    {
        memcpy((unsigned char*)&adbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueOffset);    
        while(ptr)
        {            
            mc_MemoryDumpCharSize((unsigned char*)&adbRow,0,m_Database->m_TotalSize,row_size);        
            ptr=(unsigned char*)m_Database->m_DB->MoveNext(&err);
            if(ptr)
            {
                memcpy((unsigned char*)&adbRow,ptr,m_Database->m_TotalSize);                    
            }
        }
    }
    
    printf("Ledger\n");
    
    m_Ledger->Open();

    row_size=m_Ledger->m_TotalSize;
    while(row_size > 64)
    {
        row_size /= 2;
    }
    
    total=m_Ledger->GetSize();
    pos=0;
    
    while(pos<total)
    {        
        m_Ledger->GetRow(pos,&aldRow);

        size=mc_AllocSize(m_Ledger->m_TotalSize+aldRow.m_ScriptSize+aldRow.m_ExtendedScript,m_Ledger->m_TotalSize,1);
        mc_DumpSize("",&aldRow,size,row_size);
        pos+=size;
    }
    
    m_Ledger->Close();
    
    printf("MemPool\n");
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        GetFromMemPool(&aldRow,i);        
        size=mc_AllocSize(m_Ledger->m_TotalSize+aldRow.m_ScriptSize,m_Ledger->m_TotalSize,1);
        mc_DumpSize("",&aldRow,size,row_size);
        pos+=size;
    }
    
exitlbl:    
        
    UnLock();
}

uint32_t mc_AssetDB::MaxEntityType()
{
    if(mc_gState->m_Features->Filters() == 0)
    {
        return MC_ENT_TYPE_UPGRADE;
    }
    if(mc_gState->m_Features->LicenseTokens() == 0)
    {
        return MC_ENT_TYPE_FILTER;
    }
    if(mc_gState->m_Features->Variables() == 0)
    {
        return MC_ENT_TYPE_LICENSE_TOKEN;
    }
    else
    {
        if(mc_gState->m_Features->Libraries() == 0)
        {
            return MC_ENT_TYPE_VARIABLE;
        }        
    }
    if(mc_gState->m_Features->NFTokens() == 0)
    {
        return MC_ENT_TYPE_LIBRARY;
    }
    return MC_ENT_TYPE_MAX; 
}

int mc_AssetDB::MaxScriptSize()
{
    if(mc_gState->m_Features->Filters() == 0)
    {
        return MC_ENT_MAX_SCRIPT_SIZE_BEFORE_FILTERS;
    }
    return MC_ENT_MAX_SCRIPT_SIZE; 
}

int mc_AssetDB::MaxStoredIssuers()
{
    return MC_ENT_MAX_STORED_ISSUERS; 
}

mc_Buffer *mc_AssetDB::GetEntityList(mc_Buffer *old_result,const void* txid,uint32_t entity_type)
{    
    mc_EntityDBRow adbRow;
    mc_EntityLedgerRow aldRow;
    
    Lock(0);
    
    unsigned char *ptr;
    int dbvalue_len,err,i;        
    mc_Buffer *result=NULL;    
    
    if(old_result)
    {
        result=old_result;
    }
    else
    {
        result=new mc_Buffer;
        result->Initialize(MC_ENT_KEY_SIZE,MC_ENT_KEY_SIZE,MC_BUF_MODE_DEFAULT);
    }
    
    if(txid)
    {
        result->Add(txid,(unsigned char *)txid+MC_ENT_KEY_SIZE);
        goto exitlbl;
    }
    
    adbRow.Zero();

    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&adbRow+m_Database->m_KeyOffset,m_Database->m_ValueOffset,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(err)
    {
        delete result;
        result=NULL;
        goto exitlbl;
    }

    if(ptr)
    {
        memcpy((unsigned char*)&adbRow+m_Database->m_ValueOffset,ptr,dbvalue_len);   
        while(ptr)
        {
            if(adbRow.m_KeyType == MC_ENT_KEYTYPE_TXID)
            {
                if( (entity_type == 0) || (adbRow.m_EntityType == entity_type) )
                {
                    result->Add(adbRow.m_Key,NULL);
                }
            }
            ptr=(unsigned char*)m_Database->m_DB->MoveNext(&err);
            if(ptr)
            {
                memcpy((unsigned char*)&adbRow,ptr,m_Database->m_TotalSize);                    
            }
        }
    }
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        GetFromMemPool(&aldRow,i);
        if(aldRow.m_KeyType == MC_ENT_KEYTYPE_TXID)
        {
            if( (entity_type == 0) || (aldRow.m_EntityType == entity_type) )
            {
                result->Add(aldRow.m_Key,NULL);
            }
        }
    }

exitlbl:    
        
    UnLock();
    return result;
}

int64_t mc_AssetDB::GetTotalQuantity(mc_EntityDetails *entity)
{
    Lock(0);
    
    int64_t res=GetTotalQuantity(&(entity->m_LedgerRow),NULL);
    
    UnLock();
    return res;        
}

int64_t mc_AssetDB::GetTotalQuantity(mc_EntityDetails *entity,int32_t *chain_size)
{
    Lock(0);
    
    int64_t res=GetTotalQuantity(&(entity->m_LedgerRow),chain_size);
    
    UnLock();
    return res;        
}

int64_t mc_AssetDB::GetChainLeftPosition(mc_EntityDetails *entity,int32_t index)
{
    Lock(0);
    
    int64_t res=GetChainLeftPosition(&(entity->m_LedgerRow),index);
    
    UnLock();
    return res;        
}

int64_t mc_AssetDB::GetChainLeftPosition(mc_EntityLedgerRow *row,int32_t index)
{
    mc_EntityLedgerRow aldRow;
    int64_t left_position;
    
    if(index == 0)
    {
        return 0;
    }
    
    if(index % 2)
    {
        return 0;
    }
    int32_t x,n;
    x=index;
    n=1;
    while( (x % 2) == 0 )
    {
        x/=2;
        n*=2;
    }
    if( (index-n) == 0 )
    {
        return row->m_FirstPos;
    }
    left_position=GetChainPosition(row,index-n);
    
    if(left_position < 0)
    {
        left_position=GetChainPosition(row,index-n+1);
        GetFromMemPool(&aldRow,-left_position-1);
        left_position=aldRow.m_LastPos;
    }
    
    return left_position;
}

int64_t mc_AssetDB::GetChainPosition(mc_EntityLedgerRow *row,int32_t index)
{
    mc_EntityLedgerRow aldRow;
    int64_t pos,first_pos;
    int take_it,i;
    uint64_t value_offset;
    size_t value_size;
    int32_t row_index,max_index,x,n;

    pos=row->m_ChainPos;
    first_pos=row->m_FirstPos;

    if(index == 0)
    {
        return first_pos;
    }
    
    max_index=-1;
    for(i=m_MemPool->GetCount()-1;i>=0;i--)
    {
        GetFromMemPool(&aldRow,i);
        if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
        {
            if(aldRow.m_FirstPos == first_pos)
            {
                value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_CHAIN_INDEX,&value_size);
                if(value_offset < aldRow.m_ScriptSize)
                {
                    if( (value_size>0) && (value_size <= 4))
                    {
                        row_index=mc_GetLE(aldRow.m_Script+value_offset,value_size);
                        if(row_index == index)
                        {
                            return -i-1;
                        }
                        if(row_index > max_index)
                        {
                            max_index=row_index;
                        }
                    }
                }
                pos=aldRow.m_LastPos;
            }
        }
    }

    if( (max_index >= 0) && (index > max_index) )
    {
        return 0;
    }
    
    take_it=1;

    if(first_pos >= 0)
    {
        m_Ledger->Open();
        
        while(take_it)
        {
            m_Ledger->GetRow(pos,&aldRow);
            
            row_index=-1;
            value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_CHAIN_INDEX,&value_size);
            if(value_offset < aldRow.m_ScriptSize)
            {
                if( (value_size>0) && (value_size <= 4))
                {
                    row_index=mc_GetLE(aldRow.m_Script+value_offset,value_size);
                }
            }
            if(row_index == index)
            {
                take_it=false;
            }
            if(take_it)
            {
                if(row_index < 0)
                {
                    pos=0;
                }
                if(row_index % 2)
                {
                    pos=aldRow.m_LastPos;
                }
                else
                {
                    x=row_index;
                    n=1;
                    while( (x % 2) == 0 )
                    {
                        x/=2;
                        n*=2;
                    }
//                    if( (x-n) < index )
                    if( (row_index-n) < index )
                    {
                        pos=aldRow.m_LastPos;
                    }
                    else
                    {
                        pos=0;
                        value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_LEFT_POSITION,&value_size);
                        if(value_offset < aldRow.m_ScriptSize)
                        {
                            if( (value_size>0) && (value_size <= 8))
                            {
                                pos=mc_GetLE(aldRow.m_Script+value_offset,value_size);
                            }
                        }
                    }
                }
            }
            if(pos == 0)
            {
                take_it=0;
            }
        }
        m_Ledger->Close();
    }
    
    return pos;
}

int64_t mc_AssetDB::GetTotalQuantity(mc_EntityLedgerRow *row,int32_t *chain_size)
{
    mc_EntityLedgerRow aldRow;
    int64_t pos,first_pos;
    int take_it,i;
    int64_t total;
    uint64_t value_offset;
    size_t value_size;

    if(chain_size)
    {
        *chain_size=0;
    }
    
    total=0;
    pos=row->m_ChainPos;
    first_pos=row->m_FirstPos;
    
    for(i=m_MemPool->GetCount()-1;i>=0;i--)
    {
        GetFromMemPool(&aldRow,i);
        if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
        {
            if(aldRow.m_FirstPos == first_pos)
            {
                total+=aldRow.m_Quantity;
                if(chain_size)
                {
                    if(*chain_size == 0)
                    {
                        *chain_size=-1;
                        value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_CHAIN_INDEX,&value_size);
                        if(value_offset < aldRow.m_ScriptSize)
                        {
                            if( (value_size>0) && (value_size <= 4))
                            {
                                *chain_size=mc_GetLE(aldRow.m_Script+value_offset,value_size)+1;
                                value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_ASSET_TOTAL,&value_size);
                                if(value_offset < aldRow.m_ScriptSize)
                                {
                                    if( (value_size>0) && (value_size <= 8))
                                    {
                                        total=mc_GetLE(aldRow.m_Script+value_offset,value_size);
                                        return total;
                                    }
                                }                                
                            }
                        }
                    }                    
                }
            }
        }
    }
        
    take_it=1;

    if(first_pos >= 0)
    {
        m_Ledger->Open();
                
        while(take_it)
        {
            m_Ledger->GetRow(pos,&aldRow);
            
            value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_ASSET_TOTAL,&value_size);
            if(value_offset < aldRow.m_ScriptSize)
            {
                if( (value_size>0) && (value_size <= 8))
                {
                    total+=mc_GetLE(aldRow.m_Script+value_offset,value_size);
                    take_it=0;
                }
            }
            
            if(chain_size)
            {
                if(*chain_size == 0)
                {
                    *chain_size=-1;
                    value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_CHAIN_INDEX,&value_size);
                    if(value_offset < aldRow.m_ScriptSize)
                    {
                        if( (value_size>0) && (value_size <= 8))
                        {
                            *chain_size=mc_GetLE(aldRow.m_Script+value_offset,value_size)+1;
                        }
                    }
                }                    
            }

            if(take_it)
            {
                total+=aldRow.m_Quantity;
                if(pos != first_pos)
                {
                    pos=aldRow.m_LastPos;
                    if(pos<=0)
                    {
                        take_it=0;
                        total=0xFFFFFFFFFFFFFFFF;
                    }
                }
                else
                {
                    take_it=0;
                }
            }
        }
        m_Ledger->Close();
    }
    
    return total;
}

int mc_AssetDB::FindActiveUpdate(mc_EntityDetails *entity, const void* txid)
{
    Lock(0);
    int res=0;
    
    mc_EntityLedgerRow aldRow;
    int64_t pos,first_pos;
    int take_it,i;
    uint64_t value_offset;
    size_t value_size;
    unsigned char filter_address[20];
    memset(filter_address,0,20);
    mc_RollBackPos *rollback_pos=GetRollBackPos();
    
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;
    
    if(GetEntity(&aldRow))            
    {
        entity->Set(&aldRow);
        if(entity->AllowedFollowOns() == 0)
        {
            res=1;
            goto exitlbl;
        }
        if(entity->ApproveRequired() == 0)
        {
            mc_EntityDetails genesis_entity;
            genesis_entity.Set(&aldRow);
            res=FindLastEntityByGenesisInternal(entity,&genesis_entity);
            goto exitlbl;
        }
        
        memcpy(filter_address,entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
        
        pos=aldRow.m_ChainPos;
        first_pos=aldRow.m_FirstPos;

        for(i=m_MemPool->GetCount()-1;i>=0;i--)
        {
            GetFromMemPool(&aldRow,i);
            if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
            {
                if(aldRow.m_FirstPos == first_pos)
                {
                    if( (aldRow.m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON) == 0)     // rollback_pos->InBlock() check below is always true in this case
                    {
                        res=1;
                        goto exitlbl;
                    }
                    pos=aldRow.m_LastPos;
                    if((rollback_pos == NULL) || (rollback_pos->InBlock() == 0))
                    {
                        value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_CHAIN_INDEX,&value_size);
                        if(value_offset < aldRow.m_ScriptSize)
                        {   
    //                        memcpy(filter_address,aldRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);
    //                        memcpy(filter_address,entity->GetTxID()+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);

                            if((value_size>0) && (value_size<=4))
                            {
                               memcpy((filter_address)+MC_AST_SHORT_TXID_SIZE,aldRow.m_Script+value_offset,value_size);
                            }                        
                            if(mc_gState->m_Permissions->FilterApproved(NULL,filter_address))
                            {
                                entity->Set(&aldRow);
                                res=1;
                                goto exitlbl;
                            }
                        }
                    }
                }
            }
        }

        if(pos > 0)
        {
            take_it=1;

            m_Ledger->Open();
            while(take_it)
            {
                m_Ledger->GetRow(pos,&aldRow);
                if(aldRow.m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON)
                {
                    if( (rollback_pos == NULL) || (rollback_pos->InBlock() == 0) || ((rollback_pos->IsOut(aldRow.m_Block,aldRow.m_Offset)) == 0) )
                    {
                        value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_CHAIN_INDEX,&value_size);
                        if(value_offset < aldRow.m_ScriptSize)
                        {   
    //                        memcpy(filter_address,aldRow.m_Key+MC_AST_SHORT_TXID_OFFSET,MC_AST_SHORT_TXID_SIZE);

                            if((value_size>0) && (value_size<=4))
                            {
                               memcpy((filter_address)+MC_AST_SHORT_TXID_SIZE,aldRow.m_Script+value_offset,value_size);
                            }                        
                            if(mc_gState->m_Permissions->FilterApproved(NULL,filter_address))
                            {
                                entity->Set(&aldRow);
                                take_it=0;
                            }
                        }
                    }
                }
                if(pos != first_pos)
                {
                    pos=aldRow.m_LastPos;
                    if(pos<=0)
                    {
                        take_it=0;
                    }
                }
                else
                {
                    take_it=0;
                }
            }
            m_Ledger->Close();
        }
        
        res=1;
        goto exitlbl;
    }

exitlbl:
    
    UnLock();
    return res;    
}

int mc_AssetDB::FindUpdateByName(mc_EntityDetails *entity, const void* txid,const char* name)
{
    Lock(0);
    int res=0;
    
    mc_EntityLedgerRow aldRow;
    int64_t pos,first_pos;
    int take_it,i;
    uint64_t value_offset;
    size_t value_size;
    
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;
    
    if(GetEntity(&aldRow))            
    {
        if(strlen(name) == 0)
        {
            goto exitlbl;
        }
        pos=aldRow.m_ChainPos;
        first_pos=aldRow.m_FirstPos;

        for(i=m_MemPool->GetCount()-1;i>=0;i--)
        {
            GetFromMemPool(&aldRow,i);
            if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
            {
                if(aldRow.m_FirstPos == first_pos)
                {
                    value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_UPDATE_NAME,&value_size);
                    if(value_offset < aldRow.m_ScriptSize)
                    {   
                        if( (value_size == strlen(name)) && (memcmp(name,aldRow.m_Script+value_offset,value_size) == 0) )
                        {
                            goto exitlbl;
                        }
                    }
                }
            }
        }

        if(pos > 0)
        {
            take_it=1;

            m_Ledger->Open();
            while(take_it)
            {
                m_Ledger->GetRow(pos,&aldRow);
                value_offset=mc_FindSpecialParamInDetailsScript(aldRow.m_Script,aldRow.m_ScriptSize,MC_ENT_SPRM_UPDATE_NAME,&value_size);
                if(value_offset < aldRow.m_ScriptSize)
                {   
                    if( (value_size == strlen(name)) && (memcmp(name,aldRow.m_Script+value_offset,value_size) == 0) )
                    {
                        m_Ledger->Close();
                        goto exitlbl;
                    }
                }
                if(pos != first_pos)
                {
                    pos=aldRow.m_LastPos;
                    if(pos<=0)
                    {
                        take_it=0;
                    }
                }
                else
                {
                    take_it=0;
                }
            }
            aldRow.m_EntityType=MC_ENT_TYPE_NONE;
            m_Ledger->Close();
        }
        else
        {
            aldRow.m_EntityType=MC_ENT_TYPE_NONE;            
        }
    }
        
exitlbl:
        
    if(aldRow.m_EntityType != MC_ENT_TYPE_NONE)
    {
        entity->Set(&aldRow);
        res=1;
    }
        
    UnLock();
    return res;
}


mc_Buffer *mc_AssetDB::GetFollowOns(const void* txid)
{
    Lock(0);
    
    mc_EntityLedgerRow aldRow;
    int64_t pos,first_pos;
    int take_it,i;
    mc_Buffer *result;
    result=new mc_Buffer;
        
    result->Initialize(MC_ENT_KEY_SIZE,MC_ENT_KEY_SIZE,MC_BUF_MODE_DEFAULT);

    result->Clear();
    
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;

    if(GetEntity(&aldRow))            
    {
        pos=aldRow.m_ChainPos;
        first_pos=aldRow.m_FirstPos;

        for(i=m_MemPool->GetCount()-1;i>=0;i--)
        {
            GetFromMemPool(&aldRow,i);
            if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
            {
                if(aldRow.m_FirstPos == first_pos)
                {
                    result->Add(aldRow.m_Key,NULL);                
                }
            }
        }

        if(pos > 0)
        {
            take_it=1;

            m_Ledger->Open();
            while(take_it)
            {
                m_Ledger->GetRow(pos,&aldRow);
                result->Add(aldRow.m_Key,NULL);
                if(pos != first_pos)
                {
                    pos=aldRow.m_LastPos;
                    if(pos<=0)
                    {
                        take_it=0;
                        result->Clear();
                    }
                }
                else
                {
                    take_it=0;
                }
            }
            m_Ledger->Close();
        }
    }
    
    if(result->GetCount() == 0)
    {
        delete result;
        result=NULL;
    }
            
    UnLock();
    return result;    
}

int mc_AssetDB::FindEntityByPosition(mc_EntityDetails *entity, int64_t pos)
{
    int result=MC_ERR_NOERROR;
    
    Lock(0);
    mc_EntityLedgerRow aldRow;
    
    aldRow.Zero();
    if(pos>0)
    {
        m_Ledger->Open();
        m_Ledger->GetRow(pos,&aldRow);
        m_Ledger->Close();        
    }
    else
    {
        if(pos<0)
        {
            GetFromMemPool(&aldRow,-pos-1);
        }
        else
        {
            result=MC_ERR_NOT_FOUND;
        }
    }
    entity->Set(&aldRow);
    entity->m_ThisPos=pos;
    
    UnLock();
    
    return result;
}


void mc_AssetDB::ReloadDetailsIfNeeded(mc_EntityDetails *entity)
{
    Lock(0);
    
    mc_EntityLedgerRow aldRow;
    if(entity->m_ThisPos > 0)
    {
        if( (entity->m_LedgerRow.m_ExtendedScript != 0) ||
           ((entity->m_LedgerRow.m_Script != NULL) && (entity->m_LedgerRow.m_Script != entity->m_LedgerRow.m_StaticScript)) )
        {
            int64_t pos=entity->m_ThisPos;
            m_Ledger->Open();
            m_Ledger->GetRow(pos,&aldRow);
            entity->Set(&aldRow);
            entity->m_ThisPos=pos;
            m_Ledger->Close();
        }        
    }
    
    UnLock();
}
/*
mc_Buffer *mc_AssetDB::GetFollowOnsByLastEntity(mc_EntityDetails *last_entity,int count,int start)
{
    Lock(0);
    
    mc_EntityDetails entity;
    mc_EntityLedgerRow aldRow;
    int64_t pos,first_pos;
    int take_it,i,size;
    mc_Buffer *result;
    
    result=new mc_Buffer;
        
    result->Initialize(sizeof(mc_EntityDetails),sizeof(mc_EntityDetails),MC_BUF_MODE_DEFAULT);

    result->Clear();
    
    aldRow.Zero();
    
    size=0;
    pos=0;
    first_pos=last_entity->m_LedgerRow.m_FirstPos;
    if(count > 0)
    {
        pos=GetChainPosition(&(last_entity->m_LedgerRow),start+count-1);
    }

    if((pos != 0) && (count > 0))
    {
        if(pos < 0)
        {
            i=-pos-1;
            while(i >= 0)
            {
                GetFromMemPool(&aldRow,i);
                if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
                {
                    if(aldRow.m_FirstPos == first_pos)
                    {
                        entity.Set(&aldRow);
                        entity.m_ThisPos=-i-1;
                        pos=aldRow.m_LastPos;
                        result->Add(&entity,NULL);                
                        size++;
                        if(size >= count)
                        {
                            i=0;
                        }
                    }
                }
                i--;
            }
        }
        
        if( (pos > 0) && (size < count) )
        {
            take_it=1;

            m_Ledger->Open();
            while(take_it)
            {
                m_Ledger->GetRow(pos,&aldRow);
                entity.Set(&aldRow);
                entity.m_ThisPos=pos;
                result->Add(&entity,NULL);                
                mc_DumpSize("A",entity.m_LedgerRow.m_Script,entity.m_LedgerRow.m_ScriptSize,32);
//                result->Add(aldRow.m_Key,NULL);
                size++;
                if(pos != first_pos)
                {
                    pos=aldRow.m_LastPos;
                    if(pos<=0)
                    {
                        take_it=0;
                        result->Clear();
                    }
                }
                else
                {
                    take_it=0;
                }
                if(size >= count)
                {
                    take_it=0;                    
                }
            }
            m_Ledger->Close();
        }
    }
    
    UnLock();
    return result;    
}
*/

mc_Buffer *mc_AssetDB::GetFollowOnPositionsByLastEntity(mc_EntityDetails *last_entity,int count,int start)
{
    Lock(0);
    
    mc_EntityDetails entity;
    mc_EntityLedgerRow aldRow;
    int64_t pos,first_pos;
    int take_it,i,size;
    mc_Buffer *result;
    
    result=new mc_Buffer;
        
    result->Initialize(sizeof(int64_t),sizeof(int64_t),MC_BUF_MODE_DEFAULT);

    result->Clear();
    
    aldRow.Zero();
    
    size=0;
    pos=0;
    first_pos=last_entity->m_LedgerRow.m_FirstPos;
    if(count > 0)
    {
        pos=GetChainPosition(&(last_entity->m_LedgerRow),start+count-1);
    }

    if((pos != 0) && (count > 0))
    {
        if(pos < 0)
        {
            i=-pos-1;
            while(i >= 0)
            {
                GetFromMemPool(&aldRow,i);
                if( (aldRow.m_KeyType  & MC_ENT_KEYTYPE_MASK) == MC_ENT_KEYTYPE_TXID)
                {
                    if(aldRow.m_FirstPos == first_pos)
                    {
                        entity.Set(&aldRow);
                        entity.m_ThisPos=-i-1;
                        pos=aldRow.m_LastPos;
                        result->Add(&(entity.m_ThisPos),NULL);                
                        size++;
                        if(size >= count)
                        {
                            i=0;
                        }
                    }
                }
                i--;
            }
        }
        
        if( (pos > 0) && (size < count) )
        {
            take_it=1;

            m_Ledger->Open();
            while(take_it)
            {
                m_Ledger->GetRow(pos,&aldRow);
                entity.Set(&aldRow);
                entity.m_ThisPos=pos;
                result->Add(&(entity.m_ThisPos),NULL);                
                size++;
                if(pos != first_pos)
                {
                    pos=aldRow.m_LastPos;
                    if(pos<=0)
                    {
                        take_it=0;
                        result->Clear();
                    }
                }
                else
                {
                    take_it=0;
                }
                if(size >= count)
                {
                    take_it=0;                    
                }
            }
            m_Ledger->Close();
        }
    }
    
    UnLock();
    return result;    
}

int mc_AssetDB::HasFollowOns(const void* txid)
{
    Lock(0);
    int res=0;
    
    mc_EntityLedgerRow aldRow;
    int64_t first_pos;
    int i;
    
    aldRow.Zero();
    
    memcpy(aldRow.m_Key,txid,MC_ENT_KEY_SIZE);
    aldRow.m_KeyType=MC_ENT_KEYTYPE_TXID;

    if(GetEntity(&aldRow))            
    {
        if(aldRow.m_FirstPos >= 0)
        {
            if(aldRow.m_FirstPos != aldRow.m_ChainPos)
            {
                res=1;
                goto exitlbl;
            }
        }
        first_pos=aldRow.m_FirstPos;

        for(i=0;i<m_MemPool->GetCount();i++)
        {
            GetFromMemPool(&aldRow,i);
            
            if(aldRow.m_FirstPos != (-i-1))
            {
                if(aldRow.m_FirstPos == first_pos)
                {
                    if(aldRow.m_KeyType & MC_ENT_KEYTYPE_FOLLOW_ON)
                    {
                        res=1;
                        goto exitlbl;
                    }
                }
            }
        }
    }
    
exitlbl:
    
    UnLock();
    return res;    
}

void mc_AssetDB::FreeEntityList(mc_Buffer *assets)
{
    if(assets)
    {
        delete assets;
    }
}

void mc_AssetDB::ThreadCleanse(uint64_t thread_id)
{
    m_LedgerRowScripts->ReleaseAll(thread_id);
}


uint32_t mc_GetABScriptType(void *ptr)
{
    return (uint32_t)mc_GetLE((unsigned char*)ptr+MC_AST_ASSET_SCRIPT_TYPE_OFFSET,MC_AST_ASSET_SCRIPT_TYPE_SIZE);
}

void mc_SetABScriptType(void *ptr,uint32_t type)
{
    mc_PutLE((unsigned char*)ptr+MC_AST_ASSET_SCRIPT_TYPE_OFFSET,&type,MC_AST_ASSET_SCRIPT_TYPE_SIZE);
}

uint32_t mc_GetABRefType(void *ptr)
{
    return (uint32_t)mc_GetLE((unsigned char*)ptr+MC_AST_ASSET_REF_TYPE_OFFSET,MC_AST_ASSET_REF_TYPE_SIZE);    
}

void mc_SetABRefType(void *ptr,uint32_t type)
{
    mc_PutLE((unsigned char*)ptr+MC_AST_ASSET_REF_TYPE_OFFSET,&type,MC_AST_ASSET_REF_TYPE_SIZE);    
}

int64_t mc_GetABQuantity(void *ptr)
{
    return (int64_t)mc_GetLE((unsigned char*)ptr+MC_AST_ASSET_QUANTITY_OFFSET,MC_AST_ASSET_QUANTITY_SIZE);        
}

void mc_SetABQuantity(void *ptr,int64_t quantity)
{
    mc_PutLE((unsigned char*)ptr+MC_AST_ASSET_QUANTITY_OFFSET,&quantity,MC_AST_ASSET_QUANTITY_SIZE);        
}

unsigned char* mc_GetABRef(void *ptr)
{
    return (unsigned char*)ptr;
}

void mc_SetABRef(void *ptr,void *ref)
{
    memcpy(ptr,ref,MC_AST_ASSET_BUFFER_REF_SIZE);
}

void mc_ZeroABRaw(void *ptr)
{
    memset(ptr,0,MC_AST_ASSET_FULLREF_SIZE);
}

void mc_InitABufferMap(mc_Buffer *buf)
{
    buf->Initialize(MC_AST_ASSET_QUANTITY_OFFSET,MC_AST_ASSET_FULLREF_BUF_SIZE,MC_BUF_MODE_MAP);    
}

void mc_InitABufferDefault(mc_Buffer *buf)
{
    buf->Initialize(MC_AST_ASSET_QUANTITY_OFFSET,MC_AST_ASSET_FULLREF_BUF_SIZE,MC_BUF_MODE_DEFAULT);    
}


