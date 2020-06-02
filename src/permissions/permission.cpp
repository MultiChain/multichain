// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"

unsigned char null_entity[MC_PLS_SIZE_ENTITY];
unsigned char upgrade_entity[MC_PLS_SIZE_ENTITY];
unsigned char adminminerlist_entity[MC_PLS_SIZE_ENTITY];

int mc_IsNullEntity(const void* lpEntity)
{
    if(lpEntity == NULL)
    {
        return 1;
    }
    if(memcmp(lpEntity,null_entity,MC_PLS_SIZE_ENTITY) == 0)
    {
        return 1;        
    }
    return 0;
}

int mc_IsUpgradeEntity(const void* lpEntity)
{
    if(lpEntity == NULL)
    {
        return 0;
    }
    if(memcmp(lpEntity,upgrade_entity,MC_PLS_SIZE_ENTITY) == 0)
    {
        return 1;        
    }
    return 0;
}


void mc_PermissionDBRow::Zero()
{
    memset(this,0,sizeof(mc_PermissionDBRow));
}

void mc_BlockMinerDBRow::Zero()
{
    memset(this,0,sizeof(mc_BlockMinerDBRow));    
}

void mc_AdminMinerGrantDBRow::Zero()
{
    memset(this,0,sizeof(mc_AdminMinerGrantDBRow));        
}

int mc_PermissionDBRow::InBlockRange(uint32_t block)
{
    if((block+1) >= m_BlockFrom)
    {
        if((block+1) < m_BlockTo)
        {
            return 1;
        }        
    }
    return 0;
}


void mc_PermissionLedgerRow::Zero()
{
    memset(this,0,sizeof(mc_PermissionLedgerRow));
}

void mc_BlockLedgerRow::Zero()
{
    memset(this,0,sizeof(mc_BlockLedgerRow));    
}

void mc_PermissionDetails::Zero()
{
    memset(this,0,sizeof(mc_PermissionDetails));    
}


/** Set initial database object values */

void mc_PermissionDB::Zero()
{
    m_FileName[0]=0;
    m_DB=0;
    m_KeyOffset=0;
    m_KeySize=MC_PLS_SIZE_ENTITY+24;                                            // Entity,address,type
    m_ValueOffset=56;
    m_ValueSize=24;    
    m_TotalSize=m_KeySize+m_ValueSize;
}

/** Set database file name */

void mc_PermissionDB::SetName(const char* name)
{
    mc_GetFullFileName(name,"permissions",".db",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR,m_FileName);
}

/** Open database */

int mc_PermissionDB::Open() 
{
    
    m_DB=new mc_Database;
    
    m_DB->SetOption("KeySize",0,m_KeySize);
    m_DB->SetOption("ValueSize",0,m_ValueSize);
        
    return m_DB->Open(m_FileName,MC_OPT_DB_DATABASE_CREATE_IF_MISSING | MC_OPT_DB_DATABASE_TRANSACTIONAL | MC_OPT_DB_DATABASE_LEVELDB);
}

/** Close database */

int mc_PermissionDB::Close()
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

void mc_PermissionLedger::Zero()
{
    m_FileName[0]=0;
    m_FileHan=0;
    m_KeyOffset=0;
    m_KeySize=MC_PLS_SIZE_ENTITY+32;                                            // Entity,address,type,prevrow
    m_ValueOffset=MC_PLS_SIZE_ENTITY+32;
    m_ValueSize=64;    
    m_TotalSize=m_KeySize+m_ValueSize;
}

/** Set ledger file name */

void mc_PermissionLedger::SetName(const char* name)
{
    mc_GetFullFileName(name,"permissions",".dat",MC_FOM_RELATIVE_TO_DATADIR,m_FileName);
}

/** Open ledger file */

int mc_PermissionLedger::Open()
{
    if(m_FileHan>0)
    {
        return m_FileHan;
    }
    
    m_FileHan=open(m_FileName,_O_BINARY | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    return m_FileHan;            
}

void mc_PermissionLedger::Flush()
{
    if(m_FileHan>0)
    {
        __US_FlushFile(m_FileHan);
    }    
}



/** Close ledger file */

int mc_PermissionLedger::Close()
{
    if(m_FileHan>0)
    {
        close(m_FileHan);
    }    
    m_FileHan=0;
    return 0;    
}

/** Returns ledger row */

int mc_PermissionLedger::GetRow(uint64_t RowID, mc_PermissionLedgerRow* row)
{
    int64_t off;
    off=RowID*m_TotalSize;
    
    if(m_FileHan<=0)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    if(lseek64(m_FileHan,off,SEEK_SET) != off)
    {
        return MC_ERR_NOT_FOUND;
    }
    
    row->Zero();
    
    if(read(m_FileHan,(unsigned char*)row+m_KeyOffset,m_TotalSize) != m_TotalSize)
    {
        return MC_ERR_FILE_READ_ERROR;
    }
    
    return MC_ERR_NOERROR;
    
}

/** Returns ledger size*/

uint64_t mc_PermissionLedger::GetSize()
{
    if(m_FileHan<=0)
    {
        return 0;
    }
    
    return lseek64(m_FileHan,0,SEEK_END);    
}

/** Writes row into ledger without specifying position */

int mc_PermissionLedger::WriteRow(mc_PermissionLedgerRow* row)
{
    if(m_FileHan<=0)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    if(write(m_FileHan,(unsigned char*)row+m_KeyOffset,m_TotalSize) != m_TotalSize)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    return MC_ERR_NOERROR;
}

/** Writes row into ledger to specified position */

int mc_PermissionLedger::SetRow(uint64_t RowID, mc_PermissionLedgerRow* row) 
{
    if(m_FileHan<=0)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    int64_t off;
    off=RowID*m_TotalSize;
    
    if(lseek64(m_FileHan,off,SEEK_SET) != off)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
   
    return WriteRow(row);
}

/** Setting initial values */

int mc_Permissions::Zero()
{
    m_Database=NULL;
    m_Ledger=NULL;
    m_MemPool = NULL;
    m_TmpPool = NULL;
    m_CopiedMemPool=NULL;
    m_Name[0]=0x00; 
    m_LogFileName[0]=0x00;
    m_Block=-1;    
    m_Row=0;
    m_AdminCount=0;
    m_MinerCount=0;
//    m_DBRowCount=0;            
    m_CheckPointRow=0;
    m_CheckPointAdminCount=0;
    m_CheckPointMinerCount=0;
    m_CheckPointMemPoolSize=0;
    m_CopiedBlock=0;
    m_CopiedRow=0;
    m_ForkBlock=0;
    m_CopiedAdminCount=0;
    m_CopiedMinerCount=0;
    m_ClearedAdminCount=0;
    m_ClearedMinerCount=0;
    m_ClearedMinerCountForMinerVerification=0;
    m_TmpSavedAdminCount=0;
    m_TmpSavedMinerCount=0;

    m_Semaphore=NULL;
    m_LockedBy=0;
    
    m_MempoolPermissions=NULL;
    m_MempoolPermissionsToReplay=NULL;
    m_CheckForMempoolFlag=0;
    m_RollBackPos.Zero();
    
    return MC_ERR_NOERROR;
}

/** Initialization */

int mc_Permissions::Initialize(const char *name,int mode)
{
    int err,value_len,take_it;    
    int32_t pdbBlock,pldBlock;
    uint64_t pdbLastRow,pldLastRow,this_row;
    uint64_t ledger_size;
    mc_BlockLedgerRow pldBlockRow;
    char block_row_addr[32];
    char msg[256];
    
    unsigned char *ptr;

    mc_PermissionDBRow pdbRow;
    mc_PermissionLedgerRow pldRow;
        
    strcpy(m_Name,name);
    memset(null_entity,0,MC_PLS_SIZE_ENTITY);
    memset(upgrade_entity,0,MC_PLS_SIZE_ENTITY);
    upgrade_entity[0]=MC_PSE_UPGRADE;
    memset(adminminerlist_entity,0,MC_PLS_SIZE_ENTITY);
    adminminerlist_entity[0]=MC_PSE_ADMINMINERLIST;
    
    err=MC_ERR_NOERROR;
    
    m_Ledger=new mc_PermissionLedger;
    m_Database=new mc_PermissionDB;
     
    m_Ledger->SetName(name);
    m_Database->SetName(name);
    mc_GetFullFileName(name,"permissions",".log",MC_FOM_RELATIVE_TO_LOGDIR | MC_FOM_CREATE_DIR,m_LogFileName);
    
    err=m_Database->Open();
    
    if(err)
    {
        LogString("Initialize: Cannot open database");
        return err;
    }
    
    pdbBlock=-1;    
    pdbLastRow=1;
 
    pdbRow.Zero();
    
    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
    if(err)
    {
        LogString("Initialize: Cannot read from database");
        return err;
    }

    if(ptr)                                                                     
    {
        memcpy((char*)&pdbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        pdbBlock=pdbRow.m_BlockTo;
        pdbLastRow=pdbRow.m_LedgerRow;
    }
    else
    {
        pdbRow.Zero();
        pdbRow.m_BlockTo=(uint32_t)pdbBlock;
        pdbRow.m_LedgerRow=pdbLastRow;
        
        err=m_Database->m_DB->Write((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,(char*)&pdbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,0);
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
    
    m_TmpPool=new mc_Buffer;
    m_TmpPool->Initialize(m_Database->m_ValueOffset,sizeof(mc_PermissionLedgerRow),MC_BUF_MODE_MAP);
        
    m_MemPool=new mc_Buffer;
    
    err=m_MemPool->Initialize(m_Ledger->m_KeySize,m_Ledger->m_TotalSize,MC_BUF_MODE_MAP);
    
    m_CopiedMemPool=new mc_Buffer;
    
    err=m_CopiedMemPool->Initialize(m_Ledger->m_KeySize,m_Ledger->m_TotalSize,0);
    
    m_MempoolPermissions=new mc_Buffer;
    
    err=m_MempoolPermissions->Initialize(sizeof(mc_MempoolPermissionRow),sizeof(mc_MempoolPermissionRow),0);

    m_MempoolPermissionsToReplay=new mc_Buffer;
    
    err=m_MempoolPermissionsToReplay->Initialize(sizeof(mc_MempoolPermissionRow),sizeof(mc_MempoolPermissionRow),0);
    
    pldBlock=-1;
    pldLastRow=1;
    
    if(m_Ledger->Open() <= 0)
    {
        return MC_ERR_DBOPEN_ERROR;
    }
    
    if(m_Ledger->GetRow(0,&pldRow) == 0)
    {
        pldBlock=pldRow.m_BlockTo;
        pldLastRow=pldRow.m_PrevRow;
    }
    else
    {
        pldRow.Zero();
        pldRow.m_BlockTo=(uint32_t)pldBlock;
        pldRow.m_PrevRow=pldLastRow;
        m_Ledger->SetRow(0,&pldRow);
    }        
    
    ledger_size=m_Ledger->GetSize()/m_Ledger->m_TotalSize;
    m_Row=ledger_size;

    m_Ledger->Close();
    if(pdbBlock < pldBlock)
    {
        sprintf(msg,"Initialize: Database corrupted, blocks, Ledger: %d, DB: %d, trying to repair.",pldBlock,pdbBlock);
        LogString(msg);
        if(m_Ledger->Open() <= 0)
        {
            LogString("Error: Repair: couldn't open ledger");
            return MC_ERR_DBOPEN_ERROR;
        }
    
        this_row=m_Row-1;
        take_it=1;
        if(this_row == 0)
        {
            take_it=0;
        }
    
        while(take_it && (this_row>0))
        {
            m_Ledger->GetRow(this_row,&pldRow);
        
            if((int32_t)pldRow.m_BlockReceived <= pdbBlock)
            {
                take_it=0;
            }
            if(take_it)
            {
                this_row--;
            }            
        }
        
        this_row++;

        m_Ledger->GetRow(0,&pldRow);

        pldRow.m_BlockTo=pdbBlock;
        pldRow.m_PrevRow=this_row;
        m_Ledger->SetRow(0,&pldRow);        

        m_Ledger->Close();  

        pldBlock=pdbBlock;
        pldLastRow=this_row;        
    }
    
    if(pdbBlock != pldBlock)
    {        
        sprintf(msg,"Initialize: Database corrupted, blocks, Ledger: %d, DB: %d",pldBlock,pdbBlock);
        LogString(msg);
        return MC_ERR_CORRUPTED;
    }

    if(pdbLastRow != pldLastRow)
    {
        sprintf(msg,"Initialize: Database corrupted, rows, Ledger: %ld, DB: %ld",pldLastRow,pdbLastRow);
        LogString(msg);
        return MC_ERR_CORRUPTED;
    }

    if(pldLastRow > ledger_size)
    {
        sprintf(msg,"Initialize: Database corrupted, size, last row: %ld, file size: %ld",pldLastRow,ledger_size);
        LogString(msg);
        return MC_ERR_CORRUPTED;        
    }
    
    m_Block=pdbBlock;
    m_Row=pdbLastRow;            

    err=UpdateCounts();
    if(err)
    {
        LogString("Error: Cannot initialize AdminMiner list");            
        return MC_ERR_DBOPEN_ERROR;            
    }
    
    m_ClearedAdminCount=m_AdminCount;
    m_ClearedMinerCount=m_MinerCount;
    m_ClearedMinerCountForMinerVerification=m_MinerCount;

    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: Couldn't open ledger");
        return MC_ERR_DBOPEN_ERROR;
    }
    
    if(m_Row-1 > 0)                                                             // Last row can contain wrong admin/mine count in case of hard crash
    {
        sprintf(block_row_addr,"Block %08X row",m_Block);
        m_Ledger->GetRow(m_Row-1,(mc_PermissionLedgerRow*)&pldBlockRow);        
        if(memcmp((char*)pldBlockRow.m_Address,block_row_addr,strlen(block_row_addr)))
        {
            m_Ledger->Close();  
            LogString("Error: Last ledger row doesn't contain block information");
            return MC_ERR_DBOPEN_ERROR;            
        }
        pldBlockRow.m_AdminCount=m_AdminCount;
        pldBlockRow.m_MinerCount=m_MinerCount;
        m_Ledger->SetRow(m_Row-1,(mc_PermissionLedgerRow*)&pldBlockRow);
        m_Ledger->GetRow(m_Row-2,(mc_PermissionLedgerRow*)&pldBlockRow);        
        pldBlockRow.m_AdminCount=m_AdminCount;
        pldBlockRow.m_MinerCount=m_MinerCount;
        m_Ledger->SetRow(m_Row-2,(mc_PermissionLedgerRow*)&pldBlockRow);
        m_Ledger->Close();  
    }
    
    
    m_Semaphore=__US_SemCreate();
    if(m_Semaphore == NULL)
    {
        LogString("Initialize: Cannot initialize semaphore");
        return MC_ERR_INTERNAL_ERROR;
    }

    sprintf(msg,"Initialized: Admin count: %d, Miner count: %d, ledger rows: %ld",m_AdminCount,m_MinerCount,m_Row);
    LogString(msg);
    return MC_ERR_NOERROR;
}

void mc_Permissions::MempoolPermissionsCopy()
{
    m_MempoolPermissionsToReplay->Clear();
    if(m_MempoolPermissions->GetCount())
    {
        m_MempoolPermissionsToReplay->SetCount(m_MempoolPermissions->GetCount());
        memcpy(m_MempoolPermissionsToReplay->GetRow(0),m_MempoolPermissions->GetRow(0),m_MempoolPermissions->m_Size);
        m_MempoolPermissions->Clear();
    }
}

int mc_Permissions::MempoolPermissionsCheck(int from, int to)
{
    int i;
    mc_MempoolPermissionRow *row;
    for(i=from;i<to;i++)
    {
        row=(mc_MempoolPermissionRow*)m_MempoolPermissionsToReplay->GetRow(i);
        switch(row->m_Type)
        {
            case MC_PTP_SEND:
                if(CanSend(row->m_Entity,row->m_Address) == 0)
                {
                    return 0;
                }
                break;
            case MC_PTP_RECEIVE:
                if(CanReceive(row->m_Entity,row->m_Address) == 0)
                {
                    return 0;
                }
                break;
            case MC_PTP_WRITE:
                if(CanWrite(row->m_Entity,row->m_Address) == 0)
                {
                    return 0;
                }
                break;
        }
    }
    
    for(i=from;i<to;i++)
    {
        m_MempoolPermissions->Add(m_MempoolPermissionsToReplay->GetRow(i));
    }
    
    return 1;
}


/** Logging message */

void mc_Permissions::LogString(const char *message)
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

/** Freeing objects */

int mc_Permissions::Destroy()
{
    int removefiles;
    mc_PermissionDBRow pdbRow;
    
    removefiles=0;
    if(m_Ledger)
    {
        m_Ledger->Open();
        if(m_Ledger->GetSize() == m_Ledger->m_TotalSize)
        {
            removefiles=1;
        }
        m_Ledger->Close();
    }
    
    if(m_Semaphore)
    {
        __US_SemDestroy(m_Semaphore);
    }
    
    if(m_Database)
    {
        if(removefiles)
        {
            pdbRow.Zero();
            m_Database->m_DB->Delete((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,0);
            m_Database->m_DB->Commit(0);
        }
        m_Database->Close();        
        delete m_Database;
    }
    
    if(m_Ledger)
    {
        if(removefiles)
        {
            m_Ledger->Close();
            remove(m_Ledger->m_FileName);
        }
        delete m_Ledger;
    }
    
    if(m_MemPool)
    {
        delete m_MemPool;
    }  
    if(m_TmpPool)
    {
        delete m_TmpPool;
    }  
    if(m_CopiedMemPool)
    {
        delete m_CopiedMemPool;        
    }

    if(m_MempoolPermissions)
    {
        delete m_MempoolPermissions;
    }
    
    if(m_MempoolPermissionsToReplay)
    {
        delete m_MempoolPermissionsToReplay;
    }
    
    Zero();
    
    return MC_ERR_NOERROR;
}
  
/** Locking permissions object */

void mc_Permissions::Lock(int write_mode)
{        
    uint64_t this_thread;
    this_thread=__US_ThreadID();
    
    if(this_thread == m_LockedBy)
    {
        LogString("Secondary lock!!!");
        return;
    }
    
    __US_SemWait(m_Semaphore); 
    m_LockedBy=this_thread;
}

/** Unlocking permissions object */

void mc_Permissions::UnLock()
{    
    m_LockedBy=0;
    __US_SemPost(m_Semaphore);
}

int mc_MemcmpCheckSize(const void *s1,const char *s2,size_t s1_size)
{
    if(strlen(s2) != s1_size)
    {
        return 1;
    }
    return memcmp(s1,s2,s1_size);
}

uint32_t mc_Permissions::GetPossiblePermissionTypes(const void* entity_details)
{
    uint32_t full_type;
    mc_EntityDetails *entity;
    entity=(mc_EntityDetails *)entity_details;
    
    if(entity)
    {
        if(entity->GetEntityType())
        {
            full_type = entity->Permissions();
            if(entity->GetEntityType() == MC_ENT_TYPE_ASSET)
            {
                if(mc_gState->m_Features->FixedIn20005())               
                {
                    full_type |= MC_PTP_SEND | MC_PTP_RECEIVE;
                }
            }
            full_type |= GetCustomLowPermissionTypes();
            full_type |= GetCustomHighPermissionTypes();
            return full_type;
        }
    }
    
    full_type = MC_PTP_GLOBAL_ALL;
    full_type |= GetCustomLowPermissionTypes();
    full_type |= GetCustomHighPermissionTypes();
    
    return full_type;
}

uint32_t mc_Permissions::GetPossiblePermissionTypes(uint32_t entity_type)
{
    uint32_t full_type;
    
    full_type=0;
    switch(entity_type)
    {
        case MC_ENT_TYPE_ASSET:
            full_type = MC_PTP_ISSUE | MC_PTP_ADMIN;
            break;
        case MC_ENT_TYPE_STREAM:
            full_type = MC_PTP_WRITE | MC_PTP_ACTIVATE | MC_PTP_ADMIN;
            if(mc_gState->m_Features->ReadPermissions())
            {
                full_type |= MC_PTP_READ;
            }
            break;
        case MC_ENT_TYPE_VARIABLE:
            full_type = MC_PTP_WRITE | MC_PTP_ACTIVATE | MC_PTP_ADMIN;
            break;
        case MC_ENT_TYPE_NONE:
            full_type = MC_PTP_GLOBAL_ALL;
            break;
        default:
            if(entity_type <= MC_ENT_TYPE_STREAM_MAX)
            {
                full_type = MC_PTP_WRITE | MC_PTP_ACTIVATE | MC_PTP_ADMIN;
                if(mc_gState->m_Features->ReadPermissions())
                {
                    full_type |= MC_PTP_READ;
                }
            }
            break;
    }
    return full_type;
}

uint32_t mc_Permissions::GetCustomLowPermissionTypes()
{
    if(mc_gState->m_Features->CustomPermissions())
    {
        return MC_PTP_CUSTOM1 | MC_PTP_CUSTOM2 | MC_PTP_CUSTOM3;        
    }
    return MC_PTP_NONE;
}

uint32_t mc_Permissions::GetCustomHighPermissionTypes()
{
    if(mc_gState->m_Features->CustomPermissions())
    {
        return MC_PTP_CUSTOM4 | MC_PTP_CUSTOM5 | MC_PTP_CUSTOM6;        
    }
    return MC_PTP_NONE;    
}


/** Return ORed MC_PTP_ constants by textual value */

uint32_t mc_Permissions::GetPermissionType(const char *str,const void* entity_details)
{
    return GetPermissionType(str,GetPossiblePermissionTypes(entity_details));
}

uint32_t mc_Permissions::GetPermissionType(const char *str,uint32_t full_type)
{
    uint32_t result,perm_type;
    char* ptr;
    char* start;
    char* ptrEnd;
    char c;
        
    ptr=(char*)str;
    ptrEnd=ptr+strlen(ptr);
    start=ptr;
    
    result=0;
    while(ptr<=ptrEnd)
    {
        c=*ptr;
        if( (c == ',') || (c ==0x00))
        {
            if(ptr > start)
            {
                perm_type=0;
                if((mc_MemcmpCheckSize(start,"all",      ptr-start) == 0) || 
                   (mc_MemcmpCheckSize(start,"*",      ptr-start) == 0))
                {
                    perm_type=full_type;
                }
                if(mc_MemcmpCheckSize(start,"connect",  ptr-start) == 0)perm_type = MC_PTP_CONNECT;
                if(mc_MemcmpCheckSize(start,"send",     ptr-start) == 0)perm_type = MC_PTP_SEND;
                if(mc_MemcmpCheckSize(start,"receive",  ptr-start) == 0)perm_type = MC_PTP_RECEIVE;
                if(mc_MemcmpCheckSize(start,"issue",    ptr-start) == 0)perm_type = MC_PTP_ISSUE;
                if(mc_MemcmpCheckSize(start,"mine",     ptr-start) == 0)perm_type = MC_PTP_MINE;
                if(mc_MemcmpCheckSize(start,"admin",    ptr-start) == 0)perm_type = MC_PTP_ADMIN;
                if(mc_MemcmpCheckSize(start,"activate", ptr-start) == 0)perm_type = MC_PTP_ACTIVATE;
                if(mc_MemcmpCheckSize(start,"create",   ptr-start) == 0)perm_type = MC_PTP_CREATE;
                if(mc_MemcmpCheckSize(start,"write",    ptr-start) == 0)perm_type = MC_PTP_WRITE;
                if(mc_MemcmpCheckSize(start,"read",     ptr-start) == 0)perm_type = MC_PTP_READ;
                if(mc_MemcmpCheckSize(start,MC_PTN_CUSTOM1,  ptr-start) == 0)perm_type = MC_PTP_CUSTOM1;
                if(mc_MemcmpCheckSize(start,MC_PTN_CUSTOM2,  ptr-start) == 0)perm_type = MC_PTP_CUSTOM2;
                if(mc_MemcmpCheckSize(start,MC_PTN_CUSTOM3,  ptr-start) == 0)perm_type = MC_PTP_CUSTOM3;
                if(mc_MemcmpCheckSize(start,MC_PTN_CUSTOM4,  ptr-start) == 0)perm_type = MC_PTP_CUSTOM4;
                if(mc_MemcmpCheckSize(start,MC_PTN_CUSTOM5,  ptr-start) == 0)perm_type = MC_PTP_CUSTOM5;
                if(mc_MemcmpCheckSize(start,MC_PTN_CUSTOM6,  ptr-start) == 0)perm_type = MC_PTP_CUSTOM6;
                
                if(perm_type == 0)
                {
                    return 0;
                }
                result |= perm_type;
                start=ptr+1;
            }
        }
        ptr++;
    }
    
    if(result & ~full_type)
    {
        result=0;
    }
    
    return  result;
}

void mc_RollBackPos::Zero()
{
    m_Block=-1;
    m_Offset=0;
    m_InMempool=0;
}


int mc_RollBackPos::IsOut(int block,int offset)
{
    if(block == m_Block)
    {
        return (offset > m_Offset) ? 1 : 0;
    }
    
    return (block > m_Block) ? 1 : 0;
}

int mc_RollBackPos::InBlock()
{
    return (m_Block >= 0) ? 1 : 0;
}

int mc_RollBackPos::InMempool()
{
    return ( (m_Block < 0) && (m_InMempool != 0) ) ? 1 : 0;
}

int mc_RollBackPos::NotApplied()
{
    return ( (m_Block < 0) && (m_InMempool == 0) ) ? 1 : 0;
}

int mc_Permissions::SetRollBackPos(int block,int offset,int inmempool)
{
    m_RollBackPos.m_Block=block;
    m_RollBackPos.m_Offset=offset;
    m_RollBackPos.m_InMempool=inmempool;
    
    return MC_ERR_NOERROR;
}

void mc_Permissions::ResetRollBackPos()
{
    m_RollBackPos.Zero();
}

/** Rewinds permission sequence to specific position, returns true if mempool should be checked */

int mc_Permissions::RewindToRollBackPos(mc_PermissionLedgerRow *row)
{
    if(m_RollBackPos.InBlock() == 0)
    {
        return MC_ERR_NOERROR;
    }
    
    if(m_Ledger->Open() <= 0)
    {
        LogString("GetPermission: couldn't open ledger");
        return MC_ERR_DBOPEN_ERROR;
    }
    
    m_Ledger->GetRow(row->m_ThisRow,row);
    while( (row->m_PrevRow > 0 ) && m_RollBackPos.IsOut(row->m_BlockReceived,row->m_Offset) )
    {
        m_Ledger->GetRow(row->m_PrevRow,row);
    }

    if(m_RollBackPos.IsOut(row->m_BlockReceived,row->m_Offset))
    {
        row->Zero();        
    }
    
    m_Ledger->Close();
    
    return MC_ERR_NOERROR;
}

/** Returns permission value and details for key (entity,address,type) */

uint32_t mc_Permissions::GetPermission(const void* lpEntity,const void* lpAddress,uint32_t type,mc_PermissionLedgerRow *row,int checkmempool)
{
    if(lpEntity == NULL)
    {
        return GetPermission(null_entity,lpAddress,type,row,checkmempool);
    }
    
    int err,value_len,mprow;
    uint32_t result;
    mc_PermissionLedgerRow pldRow;
    mc_PermissionDBRow pdbRow;
    unsigned char *ptr;

    pdbRow.Zero();
    memcpy(&pdbRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
    memcpy(&pdbRow.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
    pdbRow.m_Type=type;
    
    pldRow.Zero();
    memcpy(&pldRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
    memcpy(&pldRow.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
    pldRow.m_Type=type;

    memcpy(row,&pldRow,sizeof(mc_PermissionLedgerRow));
    
    if(m_Database->m_DB == NULL)
    {
        LogString("GetPermission: Database not opened");
        return 0;
    }
                                                                                
    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
    if(err)
    {
        LogString("GetPermission: Cannot read from database");
        return 0;
    }
    
    result=0;
    if(ptr)
    {
        memcpy((char*)&pdbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        
//        pldRow.m_PrevRow=pdbRow.m_LedgerRow;
        pldRow.m_BlockFrom=pdbRow.m_BlockFrom;
        pldRow.m_BlockTo=pdbRow.m_BlockTo;
        pldRow.m_Flags=pdbRow.m_Flags;
        pldRow.m_ThisRow=pdbRow.m_LedgerRow;
        
        if(mc_IsUpgradeEntity(lpEntity))
        {
            if(m_Ledger->Open() <= 0)
            {
                LogString("GetPermission: couldn't open ledger");
                return 0;
            }
            m_Ledger->GetRow(pldRow.m_ThisRow,&pldRow);
            m_Ledger->Close();                
        }                
        
        if( (m_CopiedRow > 0) && ( (type == MC_PTP_ADMIN) || (type == MC_PTP_MINE) || (type == MC_PTP_BLOCK_MINER) ) )
        {
            if(m_Ledger->Open() <= 0)
            {
                LogString("GetPermission: couldn't open ledger");
                return 0;
            }
            m_Ledger->GetRow(pdbRow.m_LedgerRow,&pldRow);
            while( (pldRow.m_PrevRow > 0 ) && (pldRow.m_BlockReceived > (uint32_t)m_ForkBlock) )
            {
                m_Ledger->GetRow(pldRow.m_PrevRow,&pldRow);
            }
                        
            if(pldRow.m_BlockReceived > (uint32_t)m_ForkBlock)
            {
                ptr=NULL;
            }
            
            m_Ledger->Close();
        }        
    
        if(ptr)
        {
            if(RewindToRollBackPos(&pldRow))
            {
                return 0;                
            }            
            if(pldRow.m_Type == MC_PTP_NONE)
            {
                ptr=NULL;
            }
        }
        
        if(ptr)
        {
            row->m_BlockFrom=pldRow.m_BlockFrom;
            row->m_BlockTo=pldRow.m_BlockTo;
            row->m_ThisRow=pldRow.m_ThisRow;
            row->m_Flags=pldRow.m_Flags;     
            row->m_BlockReceived=pldRow.m_BlockReceived;
            pldRow.m_PrevRow=pldRow.m_ThisRow;        
        }
/*        
        row->m_BlockFrom=pdbRow.m_BlockFrom;
        row->m_BlockTo=pdbRow.m_BlockTo;
        row->m_ThisRow=pdbRow.m_LedgerRow;
        row->m_Flags=pdbRow.m_Flags;
 */ 
/*        
        found_in_db=1;
        row->m_FoundInDB=found_in_db;        
 */ 
    }
    if(checkmempool != 0)
    { 
        if( ( m_RollBackPos.NotApplied() != 0) ||
            ( (m_RollBackPos.InMempool() != 0) && (type != MC_PTP_FILTER) ) )
        {
            mprow=0;
            while(mprow>=0)
            {
                mprow=m_MemPool->Seek((unsigned char*)&pldRow+m_Ledger->m_KeyOffset);
                if(mprow>=0)
                {
                    memcpy((unsigned char*)row+m_Ledger->m_KeyOffset,m_MemPool->GetRow(mprow),m_Ledger->m_TotalSize);
    //                row->m_FoundInDB=found_in_db;
                    pldRow.m_PrevRow=row->m_ThisRow;
                }
            }
        }
    }
    
    result=0;
    if((uint32_t)(m_Block+1) >= row->m_BlockFrom)
    {
        if((uint32_t)(m_Block+1) < row->m_BlockTo)
        {
            result=type;
        }                                
    }

    return result;    
}

/** Returns permission value and details for NULL entity */

uint32_t mc_Permissions::GetPermission(const void* lpAddress,uint32_t type,mc_PermissionLedgerRow *row)
{
    return GetPermission(NULL,lpAddress,type,row,1);
}

/** Returns permission value for key (entity,address,type) */

uint32_t mc_Permissions::GetPermission(const void* lpEntity,const void* lpAddress,uint32_t type)
{
    mc_PermissionLedgerRow row;
    return  GetPermission(lpEntity,lpAddress,type,&row,1);
}

/** Returns permission value for NULL entity */

uint32_t mc_Permissions::GetPermission(const void* lpAddress,uint32_t type)
{
    mc_PermissionLedgerRow row;
    return  GetPermission(lpAddress,type,&row);
}

/** Returns all permissions (entity,address) ANDed with type */

uint32_t mc_Permissions::GetAllPermissions(const void* lpEntity,const void* lpAddress,uint32_t type)
{
    uint32_t result;
    
    result=0;
    
    if(type & MC_PTP_CONNECT)    result |= CanConnect(lpEntity,lpAddress);
    if(type & MC_PTP_SEND)       result |= CanSend(lpEntity,lpAddress);
    if(type & MC_PTP_RECEIVE)    result |= CanReceive(lpEntity,lpAddress);
    if(type & MC_PTP_WRITE)      result |= CanWrite(lpEntity,lpAddress);
    if(type & MC_PTP_READ)       result |= CanRead(lpEntity,lpAddress);
    if(type & MC_PTP_CREATE)     result |= CanCreate(lpEntity,lpAddress);
    if(type & MC_PTP_ISSUE)      result |= CanIssue(lpEntity,lpAddress);
    if(type & MC_PTP_ADMIN)      result |= CanAdmin(lpEntity,lpAddress);
    if(type & MC_PTP_MINE)       result |= CanMine(lpEntity,lpAddress);
    if(type & MC_PTP_ACTIVATE)   result |= CanActivate(lpEntity,lpAddress);
    
    return result;
}

/** Returns non-zero value if upgrade is approved */

int mc_Permissions::IsApproved(const void* lpUpgrade, int check_current_block)
{
    int result;
    
    Lock(0);
            
    result=IsApprovedInternal(lpUpgrade,check_current_block);
    
    UnLock();
    
    return result;    
}


int mc_Permissions::IsApprovedInternal(const void* lpUpgrade, int check_current_block)
{
    unsigned char address[MC_PLS_SIZE_ADDRESS];
    mc_PermissionLedgerRow row;
    int result;
    
    memset(address,0,MC_PLS_SIZE_ADDRESS);
    memcpy(address,lpUpgrade,MC_PLS_SIZE_UPGRADE);
    
    result=GetPermission(upgrade_entity,address,MC_PTP_UPGRADE,&row,1);
    if(check_current_block == 0)
    {
        result=0;
        if(row.m_BlockTo > row.m_BlockFrom)
        {
            result=MC_PTP_UPGRADE;
        }
    }
    
    return result;    
}


/** Returns non-zero value if (entity,address) can connect */

int mc_Permissions::CanConnectInternal(const void* lpEntity,const void* lpAddress,int with_implicit)
{
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return MC_PTP_CONNECT;
    }
    
//    if(lpEntity == NULL)
    if(mc_IsNullEntity(lpEntity))
    {
        if(MCP_ANYONE_CAN_CONNECT)
        {
            return MC_PTP_CONNECT;
        }
    }
    
    int result;
    Lock(0);
            
    result=GetPermission(lpEntity,lpAddress,MC_PTP_CONNECT);
    
    
    if(with_implicit)
    {
        if(result == 0)
        {
            result |=  GetPermission(lpEntity,lpAddress,MC_PTP_ADMIN);    
        }

        if(result == 0)
        {
            result |=  GetPermission(lpEntity,lpAddress,MC_PTP_ACTIVATE);    
        }

        if(result == 0)
        {
            result |=  GetPermission(lpEntity,lpAddress,MC_PTP_MINE);    
        }
    }
    
    if(result)
    {
        result = MC_PTP_CONNECT; 
    }
    
    UnLock();
    
    return result;
}

int mc_Permissions::CanConnect(const void* lpEntity,const void* lpAddress)
{
    return CanConnectInternal(lpEntity,lpAddress,1);
}

int mc_Permissions::CanConnectForVerify(const void* lpEntity,const void* lpAddress)
{
    return CanConnectInternal(lpEntity,lpAddress,mc_gState->m_Features->ImplicitConnectPermission());
}

/** Returns non-zero value if (entity,address) can send */

int mc_Permissions::CanSend(const void* lpEntity,const void* lpAddress)
{
    int result;
    mc_MempoolPermissionRow row;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return MC_PTP_SEND;
    }
    
    if(mc_IsNullEntity(lpEntity))
    {
        if(MCP_ANYONE_CAN_SEND)
        {
            return MC_PTP_SEND;
        }
    }
    
    Lock(0);
            
    result = GetPermission(lpEntity,lpAddress,MC_PTP_SEND);    
    
    if(result == 0)
    {
        result |=  GetPermission(lpEntity,lpAddress,MC_PTP_ISSUE);    
    }
    
    if(result == 0)
    {
        result |=  GetPermission(lpEntity,lpAddress,MC_PTP_CREATE);    
    }
    
    if(result == 0)
    {
        result |=  GetPermission(lpEntity,lpAddress,MC_PTP_ADMIN);    
    }
    
    if(result == 0)
    {
        result |=  GetPermission(lpEntity,lpAddress,MC_PTP_ACTIVATE);    
    }
        
    if(result)
    {
        result = MC_PTP_SEND; 
    }
    
    if(result)
    {
        if(m_CheckForMempoolFlag)
        {
            memcpy(&row.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
            memcpy(&row.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
            row.m_Type=MC_PTP_SEND;
            m_MempoolPermissions->Add(&row);
        }
    }
    
    UnLock();
    
    return result;
}

/** Returns non-zero value if (entity,address) can receive */

int mc_Permissions::CanReceive(const void* lpEntity,const void* lpAddress)
{
    int result;
    mc_MempoolPermissionRow row;

    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return MC_PTP_RECEIVE;
    }
    
    if(mc_IsNullEntity(lpEntity))
    {
        if(MCP_ANYONE_CAN_RECEIVE)
        {
            return MC_PTP_RECEIVE;
        }
    }

    Lock(0);
    
    result = GetPermission(lpEntity,lpAddress,MC_PTP_RECEIVE);    
    
    if(result == 0)
    {
        result |=  GetPermission(lpEntity,lpAddress,MC_PTP_ADMIN);    
    }
    
    if(result == 0)
    {
        result |=  GetPermission(lpEntity,lpAddress,MC_PTP_ACTIVATE);    
    }
    
    if(result)
    {
        result = MC_PTP_RECEIVE; 
    }
    
    if(result)
    {
        if(m_CheckForMempoolFlag)
        {
            memcpy(&row.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
            memcpy(&row.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
            row.m_Type=MC_PTP_RECEIVE;
            m_MempoolPermissions->Add(&row);
        }
    }
    
    UnLock();
    
    return result;
}

/** Returns non-zero value if (entity,address) can write */

int mc_Permissions::CanWrite(const void* lpEntity,const void* lpAddress)
{
    int result;
    mc_MempoolPermissionRow row;

    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return 0;
    }
    
    Lock(0);
    
    result = GetPermission(lpEntity,lpAddress,MC_PTP_WRITE);    
    
    if(result)
    {
        result = MC_PTP_WRITE; 
    }
    
    if(result)
    {
        if(m_CheckForMempoolFlag)
        {
            memcpy(&row.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
            memcpy(&row.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
            row.m_Type=MC_PTP_WRITE;
            m_MempoolPermissions->Add(&row);
        }
    }
    
    UnLock();
    
    return result;
}

/** Returns non-zero value if (entity,address) can write */

int mc_Permissions::CanRead(const void* lpEntity,const void* lpAddress)
{
    int result;

    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return 0;
    }
    
    Lock(0);
    
    result = GetPermission(lpEntity,lpAddress,MC_PTP_READ);    
    
    if(result)
    {
        result = MC_PTP_READ; 
    }
        
    UnLock();
    
    return result;
}

/** Returns non-zero value if filter is approved */

int mc_Permissions::FilterApproved(const void* lpEntity,const void* lpAddress)
{
    int result;

    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return 0;
    }
    
    Lock(0);
    
    result = GetPermission(lpEntity,lpAddress,MC_PTP_FILTER);    
    
    if(result)
    {
        result = MC_PTP_FILTER; 
    }
    
    UnLock();
    
    return result;
}

/** Returns non-zero value if (entity,address) can write */

int mc_Permissions::CanCreate(const void* lpEntity,const void* lpAddress)
{
    int result;

//    if(lpEntity == NULL)
    if(mc_IsNullEntity(lpEntity))
    {
        if(mc_gState->m_Features->FixedIn1001020003())
        {
            if(MCP_ANYONE_CAN_CREATE)
            {
                return MC_PTP_CREATE;
            }            
        }
        else
        {
            if(MCP_ANYONE_CAN_RECEIVE)
            {
                return MC_PTP_CREATE;
            }
        }
    }
    
    Lock(0);
    
    result = GetPermission(lpEntity,lpAddress,MC_PTP_CREATE);    
    
    if(result)
    {
        result = MC_PTP_CREATE; 
    }
    
    UnLock();
    
    return result;
}

/** Returns non-zero value if (entity,address) can issue */

int mc_Permissions::CanIssue(const void* lpEntity,const void* lpAddress)
{
//    if(lpEntity == NULL)
    if(mc_IsNullEntity(lpEntity))
    {
        if(MCP_ANYONE_CAN_ISSUE)
        {
            return MC_PTP_ISSUE;
        }
    }

    int result;
    Lock(0);
            
    result=GetPermission(lpEntity,lpAddress,MC_PTP_ISSUE);
    
    UnLock();
    
    return result;    
}

int mc_Permissions::CanCustom(const void* lpEntity,const void* lpAddress,uint32_t permission)
{
    int result;
    
    Lock(0);
            
    result=GetPermission(lpEntity,lpAddress,permission);
    
    UnLock();
    
    return result;        
}


/** Returns 1 if we are still in setup period (NULL entity only) */

int mc_Permissions::IsSetupPeriod()
{
    if(m_Block+1<mc_gState->m_NetworkParams->GetInt64Param("setupfirstblocks"))
    {
        return 1;
    }
    return 0;
}

/** Returns number of active miners (NULL entity only) */

int mc_Permissions::GetActiveMinerCount()
{
    Lock(0);
    
    int miner_count=m_MinerCount;
    int diversity;
    if(!IsSetupPeriod())
    {
        diversity=(int)mc_gState->m_NetworkParams->GetInt64Param("miningdiversity");
        if(diversity > 0)
        {
            diversity=(int)((miner_count*diversity-1)/MC_PRM_DECIMAL_GRANULARITY);
        }
        diversity++;
        miner_count-=diversity-1;
        if(miner_count<1)
        {
            miner_count=1;
        }
        if(miner_count > m_MinerCount)
        {
            miner_count=m_MinerCount;
        }
    }    
    if(m_MinerCount <= 0)
    {
        miner_count=1;
    }
    
    UnLock();
    
    return miner_count;
}

/** Returns non-zero value if (entity,address) can mine */

int mc_Permissions::CanMine(const void* lpEntity,const void* lpAddress)
{
    mc_PermissionLedgerRow row;
    
    int result;    
    int32_t last;
        
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return MC_PTP_MINE;
    }
    
    if(mc_IsNullEntity(lpEntity))
    {
        if(MCP_ANYONE_CAN_MINE)
        {
             return MC_PTP_MINE;
        }
    }
    
    Lock(0);

    int miner_count=m_ClearedMinerCount;
    int check_mempool=0;        
    
    result = GetPermission(lpEntity,lpAddress,MC_PTP_MINE,&row,check_mempool);        

    if(result)
    {
        if(mc_IsNullEntity(lpEntity))                                           // Mining diversity is checked only for NULL object
        {
            GetPermission(lpEntity,lpAddress,MC_PTP_BLOCK_MINER,&row,0);    
            last=row.m_BlockFrom;
            if(last)
            {
                if(IsBarredByDiversity(m_Block+1,last,miner_count))
                {
                    result=0;                    
                }
            }        
        }
    }
    
    UnLock();
    
    return result;
}

/** Returns non-zero value if address can mine block with specific height */

// WARNING! Possible bug in this functiom, But function is not used

int mc_Permissions::CanMineBlock(const void* lpAddress,uint32_t block)
{
    mc_PermissionLedgerRow row;
    mc_BlockLedgerRow block_row;
    int result;    
    int miner_count;
//    int diversity;
    uint32_t last;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return MC_PTP_MINE;
    }
    
    if(MCP_ANYONE_CAN_MINE)
    {
         return MC_PTP_MINE;
    }
    
    if(block == 0)                                                              
    {
        return 0;
    }
    
    Lock(0);

    result=1;
    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: CanMineBlock: couldn't open ledger");
        result=0;
    }
    
    if(result)
    {
        GetPermission(NULL,lpAddress,MC_PTP_MINE,&row,0);        
        
        if(row.m_ThisRow)
        {
            m_Ledger->GetRow(row.m_ThisRow,&row);
            while( (row.m_PrevRow > 0 ) && (row.m_BlockReceived >= block) )
            {
                m_Ledger->GetRow(row.m_PrevRow,&row);
            }
            if(row.m_BlockReceived >= block)
            {
                result=0;
            }
        }
        else
        {
            result=0;
        }
    }
    
    if(result)
    {
        if( (block < row.m_BlockFrom) || (block >= row.m_BlockTo) )
        {
            result=0;
        }        
    }
    
    if(result>0)
    {
        GetPermission(NULL,lpAddress,MC_PTP_BLOCK_MINER,&row,0);    
        
        if(row.m_ThisRow)
        {
            m_Ledger->GetRow(row.m_ThisRow,&row);
            while( (row.m_PrevRow > 0 ) && (row.m_BlockReceived >= block) )
            {
                m_Ledger->GetRow(row.m_PrevRow,&row);
            }
            if(row.m_BlockReceived < block)
            {
                last=row.m_BlockFrom;                                           // block #1 is mined by genesis admin, no false negative here
                if(last)                                                         
                {
                    block_row.Zero();
                    sprintf((char*)block_row.m_Address,"Block %08X row",block-1);
                    GetPermission(block_row.m_Address,MC_PTP_BLOCK_INDEX,(mc_PermissionLedgerRow*)&block_row);
                    m_Ledger->GetRow(block_row.m_ThisRow,(mc_PermissionLedgerRow*)&block_row);
                    
                    miner_count=block_row.m_MinerCount;                         // Probably BUG here, should be cleared miner count, but function  not used
   
                    if(IsBarredByDiversity(block,last,miner_count))
                    {
                        result=0;
                    }
                }
            }
        }
    }
    
    m_Ledger->Close();
    
    UnLock();
    
    return result;
}

int mc_Permissions::FindLastAllowedMinerRow(mc_PermissionLedgerRow *row,uint32_t block,int prev_result)
{
    mc_PermissionLedgerRow pldRow;
    int result;
    
    if(row->m_ThisRow < m_Row-m_MemPool->GetCount())
    {
        return prev_result;
    }
    
    if(row->m_BlockReceived < block)
    {
        return prev_result;        
    }
    
    result=prev_result;
    
    memcpy(&pldRow,row,sizeof(mc_PermissionLedgerRow));
    while( (pldRow.m_ThisRow >= m_Row-m_MemPool->GetCount() ) && (pldRow.m_BlockReceived >= block) && (pldRow.m_PrevRow > 0) )
    {
        memcpy(&pldRow,m_MemPool->GetRow(pldRow.m_PrevRow),sizeof(mc_PermissionLedgerRow));
    }
    
    if(pldRow.m_PrevRow <=0 )
    {
        return 0;
    }

    if(pldRow.m_ThisRow < m_Row-m_MemPool->GetCount())
    {
        if(m_Ledger->Open() <= 0)
        {
            LogString("Error: CanMineBlock: couldn't open ledger");
            return 0;
        }
    
        m_Ledger->GetRow(pldRow.m_ThisRow,&pldRow);
        m_Ledger->Close();
    }
    
    result=0;
    if((uint32_t)block >= pldRow.m_BlockFrom)
    {
        if((uint32_t)block < pldRow.m_BlockTo)
        {
            result=MC_PTP_MINE;
        }                                
    }        
    
    return result;    
}

int mc_Permissions::CanMineBlockOnFork(const void* lpAddress,uint32_t block,uint32_t last_after_fork)
{
    mc_PermissionLedgerRow row;
    uint32_t last;
    int result;
    
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return MC_PTP_MINE;
    }
    
    if(MCP_ANYONE_CAN_MINE)
    {
         return MC_PTP_MINE;
    }
    
    if(block == 0)                                                              
    {
        return 0;
    }
    
    Lock(0);

    int miner_count=m_ClearedMinerCountForMinerVerification;
    
    result=GetPermission(NULL,lpAddress,MC_PTP_MINE,&row,1);                

    result=FindLastAllowedMinerRow(&row,block,result);
    
    if(result)
    {
        
        last=last_after_fork;

        if(last == 0)
        {
            GetPermission(NULL,lpAddress,MC_PTP_BLOCK_MINER,&row,0);    

            if(row.m_ThisRow)
            {
                last=row.m_BlockFrom;
            }    
        }
    
        if(last)
        {            
            if(IsBarredByDiversity(block,last,miner_count))
            {
                result=0;
            }            
        }
    }
    
    UnLock();
    return result;
    
}

int mc_Permissions::IsBarredByDiversity(uint32_t block,uint32_t last,int miner_count)
{
    int diversity;
    if(miner_count)
    {
        if(block >= mc_gState->m_NetworkParams->GetInt64Param("setupfirstblocks"))
        {                        
            diversity=(int)mc_gState->m_NetworkParams->GetInt64Param("miningdiversity");
            if(diversity > 0)
            {
                diversity=(int)((miner_count*diversity-1)/MC_PRM_DECIMAL_GRANULARITY);
            }
            diversity++;
            if(diversity<1)
            {
                diversity=1;
            }
            if(diversity > miner_count)
            {
                diversity=miner_count;
            }
            if((int)(block-last) <= diversity-1)
            {
                return 1;
            }
        }
    }

    return 0;
}


/** Returns non-zero value if (entity,address) can admin */

int mc_Permissions::CanAdmin(const void* lpEntity,const void* lpAddress)
{
    if(m_Block == -1)
    {
        return MC_PTP_ADMIN;
    }
    
    if(mc_IsNullEntity(lpEntity))
    {
        if(MCP_ANYONE_CAN_ADMIN)
        {
            return MC_PTP_ADMIN;
        }
    }
    
    int result;
    Lock(0);
            
    result=GetPermission(lpEntity,lpAddress,MC_PTP_ADMIN);
    
    UnLock();
    
    return result;    
}

/** Returns non-zero value if (entity,address) can activate */

int mc_Permissions::CanActivate(const void* lpEntity,const void* lpAddress)
{
    if(mc_IsNullEntity(lpEntity))
    {
        if(MCP_ANYONE_CAN_ACTIVATE)
        {
            return MC_PTP_ACTIVATE;
        }
    }
    
    int result;
    Lock(0);
            
    result=GetPermission(lpEntity,lpAddress,MC_PTP_ACTIVATE);
    
    UnLock();
    
    if(result == 0)
    {
        result = CanAdmin(lpEntity,lpAddress);
    }
    
    if(result)
    {
        result = MC_PTP_ACTIVATE; 
    }
    
    return result;    
}

/** Updates admin and miner counts (NULL entity only) */

int mc_Permissions::UpdateCounts()
{
    mc_PermissionDBRow pdbRow;
    mc_PermissionDBRow pdbAdminMinerRow;
    mc_PermissionLedgerRow row;
    
    unsigned char *ptr;
    int dbvalue_len,err,result;
    uint32_t type;
    err=MC_ERR_NOERROR;
    
    pdbAdminMinerRow.Zero();
    
    m_AdminCount=0;
    m_MinerCount=0;
    
//    m_DBRowCount=0;

    memcpy(pdbAdminMinerRow.m_Entity,adminminerlist_entity,MC_PLS_SIZE_ENTITY);
    pdbAdminMinerRow.m_Type=MC_PTP_CONNECT;

    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbAdminMinerRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    
    if(ptr)
    {
        ptr=(unsigned char*)m_Database->m_DB->MoveNext(&err);
        while(ptr)
        {
            memcpy((char*)&pdbAdminMinerRow+m_Database->m_KeyOffset,ptr,m_Database->m_TotalSize);            
            if(memcmp(pdbAdminMinerRow.m_Entity,adminminerlist_entity,MC_PLS_SIZE_ENTITY) == 0)
            {                   
                if(pdbAdminMinerRow.m_Type == MC_PTP_ADMIN)
                {
                    if(GetPermission(NULL,pdbAdminMinerRow.m_Address,MC_PTP_ADMIN))
                    {
                        m_AdminCount++;                        
                    }
                }                    
                if(pdbAdminMinerRow.m_Type == MC_PTP_MINE)
                {
                    result=GetPermission(NULL,pdbAdminMinerRow.m_Address,MC_PTP_MINE,&row,1);
                    if(m_CopiedRow > 0)
                    {
                        result=FindLastAllowedMinerRow(&row,m_Block+1,result);                        
                    }
                    if(result)
                    {
                        m_MinerCount++;
                    }
                }                    
            }
            else
            {
                ptr=NULL;
            }
                    
            if(ptr)
            {
                ptr=(unsigned char*)m_Database->m_DB->MoveNext(&err);
            }
        }        
    }
    else
    {        
        pdbAdminMinerRow.Zero();
        memcpy(pdbAdminMinerRow.m_Entity,adminminerlist_entity,MC_PLS_SIZE_ENTITY);
        pdbAdminMinerRow.m_Type=MC_PTP_CONNECT;
        
        err=m_Database->m_DB->Write((char*)&pdbAdminMinerRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                    (char*)&pdbAdminMinerRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return err;
        }
        
        pdbRow.Zero();

        ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
        if(err)
        {
            return err;
        }
        if(ptr == NULL)
        {
            return MC_ERR_NOERROR;
        }

        memcpy((char*)&pdbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);

        while(ptr)
        {
//            m_DBRowCount++;
            type=pdbRow.m_Type;
            if( (type == MC_PTP_ADMIN) || (type == MC_PTP_MINE))
            {
                if(memcmp(pdbRow.m_Entity,null_entity,MC_PLS_SIZE_ENTITY) == 0)
                {                    
                    pdbAdminMinerRow.m_Type=type;
                    memcpy(pdbAdminMinerRow.m_Address,pdbRow.m_Address,MC_PLS_SIZE_ADDRESS);
                    err=m_Database->m_DB->Write((char*)&pdbAdminMinerRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                (char*)&pdbAdminMinerRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                    if(err)
                    {
                        return err;
                    }
                    
                    if(pdbRow.InBlockRange(m_Block))
                    {
                        if(type == MC_PTP_ADMIN)
                        {
                            m_AdminCount++;
                        }
                        if(type == MC_PTP_MINE)
                        {
                            m_MinerCount++;
                        }
                    }            
                }
            }
            ptr=(unsigned char*)m_Database->m_DB->MoveNext(&err);
            if(err)
            {
                LogString("Error on MoveNext");            
            }
            if(ptr)
            {
                memcpy((char*)&pdbRow+m_Database->m_KeyOffset,ptr,m_Database->m_TotalSize);            
            }
        }
        
        err=m_Database->m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            return err;
        }
    }
    
    return err;
}

/** Dumps database and ledger */

void mc_Permissions::Dump()
{    
    mc_PermissionDBRow pdbRow;
    mc_PermissionLedgerRow pldRow;
    
    unsigned char *ptr;
    int dbvalue_len,err,i;
    
    pdbRow.Zero();
    
    printf("\nDB\n");
    
    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
    if(err)
    {
        return;
    }

    if(ptr)
    {
        memcpy((char*)&pdbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        while(ptr)
        {
            mc_MemoryDumpCharSize((char*)&pdbRow+m_Database->m_KeyOffset,0,m_Database->m_TotalSize,m_Database->m_TotalSize);        
            ptr=(unsigned char*)m_Database->m_DB->MoveNext(&err);
            if(ptr)
            {
                memcpy((char*)&pdbRow+m_Database->m_KeyOffset,ptr,m_Database->m_TotalSize);            
            }
        }
    }
    
    printf("Ledger\n");

    m_Ledger->Open();
    
    for(i=0;i<(int)m_Row-m_MemPool->GetCount();i++)
    {
        m_Ledger->GetRow(i,&pldRow);
        mc_MemoryDumpCharSize((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,0,m_Ledger->m_TotalSize,m_Ledger->m_TotalSize/2);                
    }
    
    m_Ledger->Close();
    
    mc_DumpSize("MemPool",m_MemPool->GetRow(0),m_MemPool->GetCount()*m_Ledger->m_TotalSize,m_Ledger->m_TotalSize/2);
    
}

/** Returns number of admins (NULL entity only) */

int mc_Permissions::GetAdminCount()
{    
    if(MCP_ANYONE_CAN_ADMIN)
    {
        if(mc_gState->m_Features->FixedIn1000920001())
        {
            return 1;
        }
    }
    
    return m_AdminCount;
}

/** Returns number of miners (NULL entity only) */

int mc_Permissions::GetMinerCount()
{
    return m_MinerCount;
}

/** Clears memory pool, external, with lock */

int mc_Permissions::ClearMemPool()
{
    int result;
    Lock(1);
    result=ClearMemPoolInternal();
    UnLock();
    return result;        
}

/** Clears memory pool, internal, without lock */

int mc_Permissions::ClearMemPoolInternal()
{
    char msg[256];
    if(m_MemPool->GetCount())
    {
        m_Row-=m_MemPool->GetCount();
        m_MemPool->Clear();
        m_AdminCount=m_ClearedAdminCount;
        m_MinerCount=m_ClearedMinerCount;
        UpdateCounts();
        sprintf(msg,"Mempool clr : %9d, Admin count: %d, Miner count: %d, Ledger Rows: %ld",m_Block,m_AdminCount,m_MinerCount,m_Row);
        LogString(msg);
    }
    m_ClearedAdminCount=m_AdminCount;
    m_ClearedMinerCount=m_MinerCount;
    m_ClearedMinerCountForMinerVerification=m_MinerCount;
    
    return MC_ERR_NOERROR;
}

/** Copies memory pool */

int mc_Permissions::CopyMemPool()
{
    int i,err;
    unsigned char *ptr;
    char msg[256];
    
    Lock(1);
    err=MC_ERR_NOERROR;
    
    m_CopiedMemPool->Clear();
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        ptr=m_MemPool->GetRow(i);
        err=m_CopiedMemPool->Add(ptr,ptr+m_MemPool->m_KeySize);        
        if(err)
        {
            LogString("Error while copying mempool");
            goto exitlbl;
        }
    }
    
    m_CopiedAdminCount=m_AdminCount;
    m_CopiedMinerCount=m_MinerCount;
    
    if(m_MemPool->GetCount())
    {
//        UpdateCounts();
        sprintf(msg,"Mempool copy: %9d, Admin count: %d, Miner count: %d, Ledger Rows: %ld",m_Block,m_AdminCount,m_MinerCount,m_Row);
        LogString(msg);
    }

exitlbl:
    
    UnLock();
    return err;
}

/** Restores memory pool */

int mc_Permissions::RestoreMemPool()
{
    int i,err;
    unsigned char *ptr;
    char msg[256];
    
    err=MC_ERR_NOERROR;
    
    Lock(1);
    
    ClearMemPoolInternal();
    
    for(i=0;i<m_CopiedMemPool->GetCount();i++)
    {
        ptr=m_CopiedMemPool->GetRow(i);
        err=m_MemPool->Add(ptr,ptr+m_MemPool->m_KeySize);        
        if(err)
        {
            LogString("Error while restoring mempool");
            goto exitlbl;
        }
    }
    
    m_Row+=m_CopiedMemPool->GetCount();
    m_AdminCount=m_CopiedAdminCount;
    m_MinerCount=m_CopiedMinerCount;
    
    if(m_MemPool->GetCount())
    {
//        UpdateCounts();
        sprintf(msg,"Mempool rstr: %9d, Admin count: %d, Miner count: %d, Ledger Rows: %ld",m_Block,m_AdminCount,m_MinerCount,m_Row);
        LogString(msg);
    }
    
exitlbl:
    
    UnLock();

    return err;    
}

/** Sets back database pointer to point in the past, no real rollback is made */

int mc_Permissions::RollBackBeforeMinerVerification(uint32_t block)
{
    int err,take_it;
    uint64_t this_row;
    mc_PermissionLedgerRow pldRow;
    mc_BlockLedgerRow pldBlockRow;
    
    if(block > (uint32_t)m_Block)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    err=MC_ERR_NOERROR; 
    
    CopyMemPool();
//    ClearMemPool();

    Lock(1);

    if(m_MemPool->GetCount())
    {
        m_AdminCount=m_ClearedAdminCount;
        m_MinerCount=m_ClearedMinerCount;
        m_Row-=m_MemPool->GetCount();    
        m_MemPool->Clear();
    }
    
    m_CopiedBlock=m_Block;
    m_CopiedRow=m_Row;
    m_ForkBlock=block;
    m_ClearedMinerCountForMinerVerification=m_MinerCount;
    
    
    if(block == (uint32_t)m_Block)
    {
        goto exitlbl;
    }

    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: RollBackBeforeMinerVerification: couldn't open ledger");
        return MC_ERR_DBOPEN_ERROR;
    }
    
    this_row=m_Row-1;
    take_it=1;
    if(this_row == 0)
    {
        take_it=0;
    }
    
    while(take_it && (this_row>0))
    {
        m_Ledger->GetRow(this_row,&pldRow);
        
        if(pldRow.m_BlockReceived > block)
        {
            this_row--;
        }
        else
        {
            take_it=0;
        }
    }
        
    this_row++;

    m_Row=this_row;
    m_Block=block;
    
    pldBlockRow.Zero();
    sprintf((char*)pldBlockRow.m_Address,"Block %08X row",block);
    GetPermission(pldBlockRow.m_Address,MC_PTP_BLOCK_INDEX,(mc_PermissionLedgerRow*)&pldBlockRow);
    m_Ledger->GetRow(pldBlockRow.m_ThisRow,(mc_PermissionLedgerRow*)&pldBlockRow);

    m_AdminCount=pldBlockRow.m_AdminCount;
    m_MinerCount=pldBlockRow.m_MinerCount;
    m_ClearedMinerCountForMinerVerification=m_MinerCount;

    /*
    char msg[256];
    sprintf(msg,"Verifier Rollback: %9d, Admin count: %d, Miner count: %d, Ledger Rows: %ld",
            m_Block,m_AdminCount,m_MinerCount,m_Row);
    LogString(msg);
    */

exitlbl:
    
    m_Ledger->Close();  
            
    UnLock();

    return err;    
}

/** Restores chain pointers and mempool after miner verification*/

int mc_Permissions::RestoreAfterMinerVerification()
{
    int i,err;
    unsigned char *ptr;
    char msg[256];
    
    Lock(1);
    
    m_MemPool->Clear();
    
    m_Block=m_CopiedBlock;
    m_ForkBlock=0;

    m_Row=m_CopiedRow;
    m_CopiedRow=0;
    m_CopiedBlock=0;
    
    for(i=0;i<m_CopiedMemPool->GetCount();i++)
    {
        ptr=m_CopiedMemPool->GetRow(i);
        err=m_MemPool->Add(ptr,ptr+m_MemPool->m_KeySize);        
        if(err)
        {
            LogString("Error while restoring mempool");
            goto exitlbl;
        }
    }
    
    m_Row+=m_CopiedMemPool->GetCount();
    m_AdminCount=m_CopiedAdminCount;
    m_MinerCount=m_CopiedMinerCount;
            
    if(m_CopiedMemPool->GetCount())
    {
        sprintf(msg,"Mempool ramv: %9d, Admin count: %d, Miner count: %d, Ledger Rows: %ld",m_Block,m_AdminCount,m_MinerCount,m_Row);
        LogString(msg);
    }
    
exitlbl:
    UnLock();

    return MC_ERR_NOERROR;
}


/** Returns number of admins required for consensus for specific permission type (NULL entity only) */

int mc_Permissions::AdminConsensus(const void* lpEntity,uint32_t type)
{
    int consensus;
        
    if(GetAdminCount()==0)
    {
        return 1;
    }
    
    if(mc_IsNullEntity(lpEntity))
    {
        switch(type)    
        {
            case MC_PTP_ADMIN:
            case MC_PTP_MINE:
            case MC_PTP_ACTIVATE:
            case MC_PTP_ISSUE:
            case MC_PTP_CREATE:
            case MC_PTP_FILTER:
                if(IsSetupPeriod())            
                {
                    return 1;
                }

                consensus=0;
                if(type == MC_PTP_ADMIN)
                {
                    consensus=mc_gState->m_NetworkParams->GetInt64Param("adminconsensusadmin");
                }
                if(type == MC_PTP_MINE)
                {
                    consensus=mc_gState->m_NetworkParams->GetInt64Param("adminconsensusmine");
                }
                if(type == MC_PTP_ACTIVATE)
                {
                    consensus=mc_gState->m_NetworkParams->GetInt64Param("adminconsensusactivate");
                }
                if(type == MC_PTP_ISSUE)
                {
                    consensus=mc_gState->m_NetworkParams->GetInt64Param("adminconsensusissue");
                }
                if(type == MC_PTP_CREATE)
                {
                    consensus=mc_gState->m_NetworkParams->GetInt64Param("adminconsensuscreate");
                }
                if(type == MC_PTP_FILTER)
                {
                    consensus=mc_gState->m_NetworkParams->GetInt64Param("adminconsensustxfilter");
                }

                if(consensus==0)
                {
                    return 1;
                }

                return (int)((GetAdminCount()*(uint32_t)consensus-1)/MC_PRM_DECIMAL_GRANULARITY)+1;            
        }
    }

    if(mc_IsUpgradeEntity(lpEntity))
    {
        switch(type)    
        {
            case MC_PTP_UPGRADE:
                if(IsSetupPeriod())            
                {
                    return 1;
                }

                consensus=mc_gState->m_NetworkParams->GetInt64Param("adminconsensusupgrade");
                if(consensus==0)
                {
                    return 1;
                }

                return (int)((GetAdminCount()*(uint32_t)consensus-1)/MC_PRM_DECIMAL_GRANULARITY)+1;            
        }
    }
    
    return 1;
}

/** Verifies whether consensus is reached. Update blockFrom/To fields if needed. */

int mc_Permissions::VerifyConsensus(mc_PermissionLedgerRow *newRow,mc_PermissionLedgerRow *lastRow,int *remaining)
{
    int consensus,required,found,exit_now;
    uint64_t prevRow,countLedgerRows;
    mc_PermissionLedgerRow pldRow;
    mc_PermissionLedgerRow *ptr;
    
    consensus=AdminConsensus(newRow->m_Entity,newRow->m_Type);
    required=consensus;
    
    if(remaining)
    {
        *remaining=0;
    }
    
    newRow->m_Consensus=consensus;    
    
    if(required <= 1)
    {
        return MC_ERR_NOERROR;
    }
        
    countLedgerRows=m_Row-m_MemPool->GetCount();

    m_TmpPool->Clear();
    
    exit_now=0;
    memcpy(&pldRow,newRow,sizeof(mc_PermissionLedgerRow));
    while(required)
    {
        memcpy(pldRow.m_Address,pldRow.m_Admin,MC_PLS_SIZE_ADDRESS);         // Looking for row from the same admin
        
        ptr=&pldRow;        
        
        found=m_TmpPool->Seek((unsigned char*)ptr);                                           
        if(found >= 0)
        {
            ptr=(mc_PermissionLedgerRow *)m_TmpPool->GetRow(found);
            if(ptr->m_Timestamp < pldRow.m_Timestamp)
            {
                if(ptr->m_GrantFrom == newRow->m_GrantFrom)
                {
                    if(ptr->m_GrantTo == newRow->m_GrantTo)
                    {
                        required++;                                             // Duplicate record, compensate for decrementing below 
                    }            
                }                
                m_TmpPool->PutRow(found,&pldRow,(unsigned char*)&pldRow+m_Database->m_ValueOffset);
                ptr=&pldRow;
            }
            else
            {
                ptr=NULL;                                                       // Ignore this record, we already have newer
            }
        }
        else
        {
            m_TmpPool->Add(&pldRow,(unsigned char*)&pldRow+m_Database->m_ValueOffset);            
        }
        
        if(ptr)                                                                 
        {
            if(ptr->m_GrantFrom == newRow->m_GrantFrom)
            {
                if(ptr->m_GrantTo == newRow->m_GrantTo)
                {
                    if((remaining == NULL) || (pldRow.m_BlockReceived > 0))     // Avoid decrementing when checking for RequiredForConsensus
                    {
                        required--;                        
                    }
                }            
            }
        }

        if(required)
        {
            prevRow=pldRow.m_PrevRow;
        
            if(prevRow == 0)
            {
                exit_now=1;
            }
            else
            {
                if(prevRow >= countLedgerRows)                                  // Previous row in mempool
                {
                    pldRow.Zero();
                    memcpy((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,m_MemPool->GetRow(prevRow-countLedgerRows),m_Ledger->m_TotalSize);
                }
                else                                                            // Previous row in ledger
                {
                    if(m_Ledger->Open() <= 0)
                    {
                        LogString("Error: VerifyConsensus: couldn't open ledger");
                        return MC_ERR_DBOPEN_ERROR;
                    }

                    if(m_Ledger->GetRow(prevRow,&pldRow))
                    {
                        LogString("Error: VerifyConsensus: couldn't get row");
                        m_Ledger->Close();
                        return MC_ERR_DBOPEN_ERROR;
                    }            
                    
                    if(memcmp(&pldRow,newRow,m_Database->m_ValueOffset) != 0)
                    {
                        LogString("Error: VerifyConsensus: row key mismatch");
                        m_Ledger->Close();
                        return MC_ERR_DBOPEN_ERROR;                        
                    }
                    
                    if(prevRow != pldRow.m_ThisRow)
                    {
                        LogString("Error: VerifyConsensus: row id mismatch");
                        m_Ledger->Close();
                        return MC_ERR_DBOPEN_ERROR;                                                
                    }
                }                
            }
            
            if(pldRow.m_Consensus > 0)                                          // Reached previous consensus row
            {
                exit_now=1;
            }
            
            if(exit_now)                                                        // We didn't reach consensus, revert blockFrom/To to previous values
            {
                if(remaining)
                {
                    *remaining=required;
                }
                newRow->m_Consensus=0;                                          
                newRow->m_BlockFrom=lastRow->m_BlockFrom;
                newRow->m_BlockTo=lastRow->m_BlockTo;
                return MC_ERR_NOERROR;            
            }
        
        }                
        
    }
    
    m_Ledger->Close();
    
    return MC_ERR_NOERROR;
}

/** Fills details buffer for permission row */

int mc_Permissions::FillPermissionDetails(mc_PermissionDetails *plsRow,mc_Buffer *plsDetailsBuffer)
{
    int consensus,required,found,exit_now,phase,in_consensus,i;
    uint64_t prevRow,countLedgerRows;
    mc_PermissionLedgerRow pldRow;
    mc_PermissionLedgerRow pldAdmin;
    mc_PermissionDetails plsDetails;
    mc_PermissionLedgerRow *ptr;
    
    countLedgerRows=m_Row-m_MemPool->GetCount();
    consensus=AdminConsensus(plsRow->m_Entity,plsRow->m_Type);
    
    if(mc_IsUpgradeEntity(plsRow->m_Entity))
    {
        consensus=AdminConsensus(plsRow->m_Entity,MC_PTP_UPGRADE);
    }
    
    required=consensus;
    
    plsRow->m_RequiredAdmins=consensus;
    
    prevRow=plsRow->m_LastRow;

    phase=0;
    exit_now=0;
    
    int path;
    
    while(exit_now == 0)
    {    
        if(prevRow == 0)
        {
            LogString("Internal error: FillPermissionDetails: prevRow=0 ");
            return MC_ERR_INVALID_PARAMETER_VALUE;
        }
        
        if(prevRow >= countLedgerRows)                                          // in mempool
        {
            pldRow.Zero();
            memcpy((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,m_MemPool->GetRow(prevRow-countLedgerRows),m_Ledger->m_TotalSize);
        }
        else
        {
            if(m_Ledger->GetRow(prevRow,&pldRow))
            {
                m_Ledger->Close();
                LogString("Error: FillPermissionDetails: couldn't get row");
                return MC_ERR_DBOPEN_ERROR;
            }            
            if(memcmp(&pldRow,plsRow,m_Database->m_ValueOffset) != 0)           // m_Database->m_ValueOffset are common for all structures
            {
                m_Ledger->Close();
                LogString("Error: FillPermissionDetails: row key mismatch");
                return MC_ERR_DBOPEN_ERROR;                        
            }
                    
            if(prevRow != pldRow.m_ThisRow)
            {
                m_Ledger->Close();
                LogString("Error: FillPermissionDetails: row id mismatch");
                return MC_ERR_DBOPEN_ERROR;                                                
            }
        }                
        
        path=0;
        
        if(phase == 0)
        {
            path+=1;
            if(pldRow.m_Consensus > 0)                                          // The last row is in consensus
            {
                memcpy(plsRow->m_LastAdmin,pldRow.m_Admin,MC_PLS_SIZE_ADDRESS);
                if(required <= 1)                                               // No pending records, this is the only admin, no details required
                {
                    return MC_ERR_NOERROR;
                }
                phase=2;                                                        // We are in consensus, but want to find all admins
            }
            else
            {
                plsRow->m_Flags |= MC_PFL_HAVE_PENDING;
                phase=1;                                                        // There are pending records
            }
            plsRow->m_BlockReceived=pldRow.m_BlockReceived;
            if(plsDetailsBuffer == NULL)                                              // We have details, but they are not required
            {
                return MC_ERR_NOERROR;
            }
            else
            {
                m_TmpPool->Clear();                
            }
        }
        else
        {
            if(pldRow.m_Consensus > 0)                                          // Consensus record
            {
                path+=128;
                if(phase == 1)                                                  // We were waiting for consensus record, now we are just looking to fill list of admins
                {
                    path+=256;
                    phase=2;
                }
                else                                                            // This is previous consensus, we should not go beyond this point
                {
                    path+=512;
                    exit_now=1;
                }
            }                                
        }
        
        if(exit_now == 0)
        {
            path+=2;
            in_consensus=0;                                                     // This row match consensus
            if(pldRow.m_GrantFrom == plsRow->m_BlockFrom)
            {
                if(pldRow.m_GrantTo == plsRow->m_BlockTo)
                {
                    if(phase == 2)
                    {
                        in_consensus=1;
                    }
                }            
            }                
        
            ptr=NULL;
            if((phase == 1) || (in_consensus>0))                                // If we just fill consensus admin list (phase 2) we ignore non-consensus records 
            {                    
                path+=4;
                memcpy(&pldAdmin,&pldRow,sizeof(mc_PermissionLedgerRow));
                memcpy(pldAdmin.m_Address,pldRow.m_Admin,MC_PLS_SIZE_ADDRESS);
                pldAdmin.m_Type=in_consensus;
                
                ptr=&pldAdmin;                                                  // Row with address replaced by admin

                found=m_TmpPool->Seek((unsigned char*)ptr);                                           
                if(found >= 0)
                {
                    path+=8;
                    ptr=(mc_PermissionLedgerRow *)m_TmpPool->GetRow(found);
                    if(ptr->m_Timestamp < pldAdmin.m_Timestamp)
                    {
                        if(ptr->m_GrantFrom == plsRow->m_BlockFrom)
                        {
                            if(ptr->m_GrantTo == plsRow->m_BlockTo)
                            {
                                required++;                                             // Duplicate record, compensate for decrementing below 
                            }            
                        }                
                        m_TmpPool->PutRow(found,&pldAdmin,(unsigned char*)&pldAdmin+m_Database->m_ValueOffset);
                        ptr=&pldAdmin;
                    }
                    else
                    {
                        ptr=NULL;                                                       // Ignore this record, we already have newer
                    }
                }
                else
                {
                    m_TmpPool->Add(&pldAdmin,(unsigned char*)&pldAdmin+m_Database->m_ValueOffset);            
                }        
            }    

            if(ptr)
            {
                path+=16;
                if(in_consensus)
                {
                    path+=32;
                    required--;    
                    if(required <= 0)
                    {
                        exit_now=1;
                    }
                }
            }
        }
        
        if(required > 0)
        {
            prevRow=pldRow.m_PrevRow;

            if(prevRow == 0)
            {
                exit_now=1;
            }                
        }
        else
        {
            exit_now=1;
        }        
    }
    
    for(i=0;i<m_TmpPool->m_Count;i++)
    {
        ptr=(mc_PermissionLedgerRow*)m_TmpPool->GetRow(i);
        
        memcpy(plsDetails.m_Entity,plsRow->m_Entity,MC_PLS_SIZE_ENTITY);
        memcpy(plsDetails.m_Address,plsRow->m_Address,MC_PLS_SIZE_ADDRESS);
        plsDetails.m_Type=plsRow->m_Type;
        plsDetails.m_BlockFrom=ptr->m_GrantFrom;
        plsDetails.m_BlockTo=ptr->m_GrantTo;
        plsDetails.m_Flags=ptr->m_Flags;
        plsDetails.m_LastRow=ptr->m_ThisRow;
        memcpy(plsDetails.m_LastAdmin,ptr->m_Address,MC_PLS_SIZE_ADDRESS);
        plsDetails.m_RequiredAdmins=ptr->m_Type;
        
        plsDetailsBuffer->Add(&plsDetails,(unsigned char*)&plsDetails+m_Ledger->m_ValueOffset);
    }
    
    return MC_ERR_NOERROR;
}

/** Returns number of admins required for consensus */

int mc_Permissions::RequiredForConsensus(const void* lpEntity,const void* lpAddress,uint32_t type,uint32_t from,uint32_t to)
{
    mc_PermissionLedgerRow pldRow;
    mc_PermissionLedgerRow pldLast;
    int required;
    
    if(mc_IsNullEntity(lpEntity) == 0)
    {
        return 1;
    }
    
    GetPermission(lpEntity,lpAddress,type,&pldLast,1);
    
    switch(type)
    {
        case MC_PTP_ADMIN:
        case MC_PTP_MINE:
        case MC_PTP_CONNECT:
        case MC_PTP_RECEIVE:
        case MC_PTP_SEND:
        case MC_PTP_ISSUE:
        case MC_PTP_CREATE:
        case MC_PTP_ACTIVATE:
        case MC_PTP_WRITE:
        case MC_PTP_READ:
            if(pldLast.m_ThisRow)
            {
                if(pldLast.m_BlockFrom == from)
                {
                    if(pldLast.m_BlockTo == to)
                    {
                        return 0;
                    }                    
                }
                                
                pldRow.Zero();
                
                if(lpEntity)
                {
                    memcpy(pldRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
                }
                memcpy(pldRow.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
                pldRow.m_Type=type;
                pldRow.m_PrevRow=pldLast.m_ThisRow;
                pldRow.m_BlockFrom=from;
                pldRow.m_BlockTo=to;
                pldRow.m_BlockReceived=0;
                pldRow.m_GrantFrom=from;
                pldRow.m_GrantTo=to;
                pldRow.m_ThisRow=m_Row;
                pldRow.m_Timestamp=mc_TimeNowAsUInt();
                pldRow.m_Flags=0;
//                pldRow.m_FoundInDB=pldLast.m_FoundInDB;
                
                if(VerifyConsensus(&pldRow,&pldLast,&required))
                {
                    return -2;
                }
            }    
            else
            {
                required=AdminConsensus(lpEntity,type);
            }
            break;
        default:
            return -1;
    }
    
    return required;
}

/** Returns permission details for specific record, external, locks */

mc_Buffer *mc_Permissions::GetPermissionDetails(mc_PermissionDetails *plsRow)
{
    mc_Buffer *result;
    
    Lock(0);
    
    result=new mc_Buffer;

    result->Initialize(m_Ledger->m_ValueOffset,sizeof(mc_PermissionDetails),MC_BUF_MODE_DEFAULT);

    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: FillPermissionDetails: couldn't open ledger");
        goto exitlbl;
    }
        
    if(FillPermissionDetails(plsRow,result))
    {
        delete result;
        result=NULL;
    }

    m_Ledger->Close();

exitlbl:    
    UnLock();

    return result;
}

/** Returns list of upgrade approval */

mc_Buffer *mc_Permissions::GetUpgradeList(const void* lpUpgrade,mc_Buffer *old_buffer)
{
    if(lpUpgrade)
    {    
        unsigned char address[MC_PLS_SIZE_ADDRESS];

        memset(address,0,MC_PLS_SIZE_ADDRESS);
        memcpy(address,lpUpgrade,MC_PLS_SIZE_UPGRADE);

        return GetPermissionList(upgrade_entity,address,MC_PTP_CONNECT | MC_PTP_UPGRADE,old_buffer);
    }
    
    return GetPermissionList(upgrade_entity,NULL,MC_PTP_CONNECT | MC_PTP_UPGRADE,old_buffer);    
}

/** Returns list of permission states */

mc_Buffer *mc_Permissions::GetPermissionList(const void* lpEntity,const void* lpAddress,uint32_t type,mc_Buffer *old_buffer)
{    
    mc_PermissionDBRow pdbRow;
    mc_PermissionLedgerRow pldRow;
    mc_PermissionDetails plsRow;
    unsigned char *ptr;
    mc_PermissionDetails *ptrFound;
    
    int dbvalue_len,err,i,cur_type,found,first,take_it;
    unsigned char null_entity[32];
    memset(null_entity,0,MC_PLS_SIZE_ENTITY);

    Lock(0);
    
    mc_Buffer *result;
    if(old_buffer == NULL)
    {
        result=new mc_Buffer;
                                                                                // m_Database->m_ValueOffset bytes are common for all structures
        result->Initialize(m_Database->m_ValueOffset,sizeof(mc_PermissionDetails),MC_BUF_MODE_MAP);        
    }
    else
    {
        result=old_buffer;
    }
    
    if(lpAddress)
    {
        cur_type=1;
        plsRow.Zero();
        
        if(lpEntity)
        {
            memcpy(plsRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
        }
        memcpy(plsRow.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
        for(i=0;i<32;i++)
        {
            if(cur_type & type)
            {
                GetPermission(lpEntity,lpAddress,cur_type,&pldRow,1);
                if(pldRow.m_ThisRow)
                {
                    plsRow.m_Type=pldRow.m_Type;
                    plsRow.m_BlockFrom=pldRow.m_BlockFrom;
                    plsRow.m_BlockTo=pldRow.m_BlockTo;
                    plsRow.m_Flags=pldRow.m_Flags;
                    plsRow.m_LastRow=pldRow.m_ThisRow;
                    plsRow.m_BlockReceived=pldRow.m_BlockReceived;
                    result->Add(&plsRow,(unsigned char*)&plsRow+m_Database->m_ValueOffset);
                }
            }
            cur_type=cur_type<<1;                    
        }
    }
    else
    {    
        if(mc_IsUpgradeEntity(lpEntity))
        {
            m_Ledger->Open();
        }
    
        pdbRow.Zero();

        if(lpEntity)
        {
            memcpy(pdbRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
            pdbRow.m_Type=MC_PTP_CONNECT;
        }
        
        first=1;
        ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&dbvalue_len,MC_OPT_DB_DATABASE_SEEK_ON_READ,&err);
        if(err)
        {
            LogString("Error: GetPermissionList: db read");
            delete result;
            result=NULL;
            goto exitlbl;
        }

        if(ptr)
        {
            memcpy((char*)&pdbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
            while(ptr)
            {
                if(first == 0)
                {
                    if(type & pdbRow.m_Type)
                    {
                        plsRow.Zero();
                        memcpy(plsRow.m_Entity,pdbRow.m_Entity,MC_PLS_SIZE_ENTITY);
                        memcpy(plsRow.m_Address,pdbRow.m_Address,MC_PLS_SIZE_ADDRESS);
                        plsRow.m_Type=pdbRow.m_Type;
                        plsRow.m_BlockFrom=pdbRow.m_BlockFrom;
                        plsRow.m_BlockTo=pdbRow.m_BlockTo;
                        plsRow.m_Flags=pdbRow.m_Flags;
                        plsRow.m_LastRow=pdbRow.m_LedgerRow;
                        if(mc_IsUpgradeEntity(lpEntity))
                        {
                            m_Ledger->GetRow(pdbRow.m_LedgerRow,&pldRow);                            
                        }
                        plsRow.m_BlockReceived=pldRow.m_BlockReceived;
                        result->Add(&plsRow,(unsigned char*)&plsRow+m_Database->m_ValueOffset);
                    }
                }
                first=0;
                ptr=(unsigned char*)m_Database->m_DB->MoveNext(&err);
                if(ptr)
                {
                    memcpy((char*)&pdbRow+m_Database->m_KeyOffset,ptr,m_Database->m_TotalSize);            
                    if(mc_IsNullEntity(lpEntity))
                    {
                        if(memcmp(pdbRow.m_Entity,null_entity,MC_PLS_SIZE_ENTITY))
                        {
                            ptr=NULL;
                        }
                    }
                    else
                    {
                        if(memcmp(pdbRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY))
                        {
                            ptr=NULL;
                        }                        
                    }
                }
            }
        }

        for(i=0;i<m_MemPool->GetCount();i++)
        {
            ptr=m_MemPool->GetRow(i);
            pldRow.Zero();
            memcpy((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,ptr,m_Ledger->m_TotalSize);            
            take_it=0;
            if(mc_IsNullEntity(lpEntity))
            {
                if(memcmp(pldRow.m_Entity,null_entity,MC_PLS_SIZE_ENTITY) == 0)
                {
                    take_it=1;
                }
            }
            else
            {                    
                if(memcmp(pldRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY) == 0)
                {
                    take_it=1;
                }
            }
            if(take_it)
            {
                if(type & pldRow.m_Type)
                {
                    found=result->Seek(&pldRow);
                    if(found >= 0)
                    {
                        ptrFound=(mc_PermissionDetails*)(result->GetRow(found));
                        ptrFound->m_BlockFrom=pldRow.m_BlockFrom;
                        ptrFound->m_BlockTo=pldRow.m_BlockTo;
                        ptrFound->m_Flags=pldRow.m_Flags;
                        ptrFound->m_LastRow=pldRow.m_ThisRow;
                    }
                    else
                    {
                        if(first == 0)
                        {
                            plsRow.Zero();
                            memcpy(plsRow.m_Entity,pldRow.m_Entity,MC_PLS_SIZE_ENTITY);
                            memcpy(plsRow.m_Address,pldRow.m_Address,MC_PLS_SIZE_ADDRESS);
                            plsRow.m_Type=pldRow.m_Type;
                            plsRow.m_BlockFrom=pldRow.m_BlockFrom;
                            plsRow.m_BlockTo=pldRow.m_BlockTo;
                            plsRow.m_Flags=pldRow.m_Flags;
                            plsRow.m_LastRow=pldRow.m_ThisRow;
                            plsRow.m_BlockReceived=pldRow.m_BlockReceived;
                            result->Add(&plsRow,(unsigned char*)&plsRow+m_Database->m_ValueOffset);                    
                        }
                        first=0;
                    }
                }
                else
                {
                    first=0;                    
                }
            }
        }
    }
     
    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: GetPermissionList: couldn't open ledger");
        goto exitlbl;
    }
        
    for(i=0;i<result->m_Count;i++)
    {
        FillPermissionDetails((mc_PermissionDetails*)result->GetRow(i),NULL);
    }

    m_Ledger->Close();
    
exitlbl:
    UnLock();
    
    return result;
}

/** Frees buffer returned by GetPermissionList */

void mc_Permissions::FreePermissionList(mc_Buffer *permissions)
{    
    if(permissions)
    {
        delete permissions;
    }
}

/** Returns 1 if Activate permission is ebough for granting/revoking permission of specified type */


int mc_Permissions::IsActivateEnough(uint32_t type)
{
    if(type & ( MC_PTP_ADMIN | MC_PTP_ISSUE | MC_PTP_MINE | MC_PTP_ACTIVATE | MC_PTP_CREATE | MC_PTP_FILTER))
    {
        return 0;
    }    
    if(type & GetCustomHighPermissionTypes())
    {
        return 0;        
    }
    return 1;
}

/** Sets permission record, external, locks */

int mc_Permissions::SetPermission(const void* lpEntity,const void* lpAddress,uint32_t type,const void* lpAdmin,
                                  uint32_t from,uint32_t to,uint32_t timestamp,uint32_t flags,int update_mempool,int offset)
{
    int result;
    
    if(mc_IsNullEntity(lpEntity) || ((flags & MC_PFL_ENTITY_GENESIS) == 0))
    {
        if(!CanAdmin(lpEntity,lpAdmin))
        {
            if(IsActivateEnough(type))
            {
                if(!CanActivate(lpEntity,lpAdmin))
                {
                    return MC_ERR_NOT_ALLOWED;                
                }
            }
            else
            {
                return MC_ERR_NOT_ALLOWED;
            }
        }    
    }
    
    Lock(1);
    result=SetPermissionInternal(lpEntity,lpAddress,type,lpAdmin,from,to,timestamp,flags,update_mempool,offset);
    UnLock();
    return result;
}

/** Sets approval record, external, locks */

int mc_Permissions::SetApproval(const void* lpUpgrade,uint32_t approval,const void* lpAdmin,uint32_t from,uint32_t timestamp,uint32_t flags,int update_mempool,int offset)
{
    int result=MC_ERR_NOERROR;
    mc_PermissionLedgerRow row;
    unsigned char address[MC_PLS_SIZE_ADDRESS];
    memset(address,0,MC_PLS_SIZE_ADDRESS);
    Lock(1);
    
    if(GetPermission(upgrade_entity,address,MC_PTP_CONNECT,&row,1) == 0)
    {
        result=SetPermissionInternal(upgrade_entity,address,MC_PTP_CONNECT,address,0,(uint32_t)(-1),timestamp, MC_PFL_ENTITY_GENESIS ,update_mempool,offset);        
    }
    
    
    if(result == MC_ERR_NOERROR)
    {
        memcpy(address,lpUpgrade,MC_PLS_SIZE_UPGRADE);
        if(lpAdmin == NULL)
        {
            result=SetPermissionInternal(upgrade_entity,address,MC_PTP_CONNECT,address,from,approval ? (uint32_t)(-1) : 0,timestamp, flags,update_mempool,offset);                
        }
        else
        {
            if(IsApprovedInternal(lpUpgrade,0) == 0)
            {
                result=SetPermissionInternal(upgrade_entity,address,MC_PTP_UPGRADE,lpAdmin,from,approval ? (uint32_t)(-1) : 0,timestamp, flags,update_mempool,offset);                            
            }
        }
    }

    UnLock();    
    return result;
}

/** Sets permission record, internal */

int mc_Permissions::SetPermissionInternal(const void* lpEntity,const void* lpAddress,uint32_t type,const void* lpAdmin,uint32_t from,uint32_t to,uint32_t timestamp,uint32_t flags,int update_mempool,int offset)
{
    mc_PermissionLedgerRow pldRow;
    mc_PermissionLedgerRow pldLast;
    int err,i,num_types,thisBlock,lastAllowed,thisAllowed;        
    char msg[256];
    uint32_t types[32];
    uint32_t pr_entity,pr_address,pr_admin;
    num_types=0;
    types[num_types]=MC_PTP_CONNECT;num_types++;
    types[num_types]=MC_PTP_SEND;num_types++;
    types[num_types]=MC_PTP_RECEIVE;num_types++;
    types[num_types]=MC_PTP_WRITE;num_types++;        
    types[num_types]=MC_PTP_READ;num_types++;        
    types[num_types]=MC_PTP_CREATE;num_types++;        
    types[num_types]=MC_PTP_ISSUE;num_types++;
    types[num_types]=MC_PTP_MINE;num_types++;
    types[num_types]=MC_PTP_ACTIVATE;num_types++;        
    types[num_types]=MC_PTP_ADMIN;num_types++;        
    types[num_types]=MC_PTP_UPGRADE;num_types++;                        
    types[num_types]=MC_PTP_FILTER;num_types++;                        
    types[num_types]=MC_PTP_CUSTOM1;num_types++;                        
    types[num_types]=MC_PTP_CUSTOM2;num_types++;                        
    types[num_types]=MC_PTP_CUSTOM3;num_types++;                        
    types[num_types]=MC_PTP_CUSTOM4;num_types++;                        
    types[num_types]=MC_PTP_CUSTOM5;num_types++;                        
    types[num_types]=MC_PTP_CUSTOM6;num_types++;                        
    
    err=MC_ERR_NOERROR;


    for(i=0;i<num_types;i++)
    {
        if(types[i] & type)
        {
            if(types[i] == type)
            {                
                lastAllowed=GetPermission(lpEntity,lpAddress,type,&pldLast,1);
                                
                thisBlock=m_Block+1;

                pldRow.Zero();
                if(lpEntity)
                {
                    memcpy(pldRow.m_Entity,lpEntity,MC_PLS_SIZE_ENTITY);
                }
                memcpy(pldRow.m_Address,lpAddress,MC_PLS_SIZE_ADDRESS);
                pldRow.m_Type=type;
                pldRow.m_PrevRow=pldLast.m_ThisRow;
                pldRow.m_BlockFrom=from;
                pldRow.m_BlockTo=to;
                if(m_AdminCount)
                {
                    memcpy(pldRow.m_Admin,lpAdmin,MC_PLS_SIZE_ADDRESS);
                }
                else
                {
                    memcpy(pldRow.m_Admin,lpAddress,MC_PLS_SIZE_ADDRESS);
                }
                pldRow.m_BlockReceived=thisBlock;
                pldRow.m_GrantFrom=from;
                pldRow.m_GrantTo=to;
                pldRow.m_ThisRow=m_Row;
                pldRow.m_Timestamp=timestamp;
                pldRow.m_Flags=flags;
                pldRow.m_Offset=offset;
//                pldRow.m_FoundInDB=pldLast.m_FoundInDB;
                
                err=VerifyConsensus(&pldRow,&pldLast,NULL);
                
                if(err)
                {
                    return err;
                }
                
                if(update_mempool)
                {
                    if(m_MemPool->GetCount() == 0)
                    {
                        if(m_CopiedRow == 0)
                        {
                            m_ClearedAdminCount=m_AdminCount;
                            m_ClearedMinerCount=m_MinerCount;
                            m_ClearedMinerCountForMinerVerification=m_MinerCount;
                        }
                    }
                    m_MemPool->Add((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,(unsigned char*)&pldRow+m_Ledger->m_ValueOffset);
                    m_Row++;                

                    if( (type == MC_PTP_ADMIN) || (type == MC_PTP_MINE))
                    {
                        if(mc_IsNullEntity(lpEntity))
                        {
                            thisAllowed=GetPermission(lpEntity,lpAddress,type);
                            if(lastAllowed != thisAllowed)
                            {
                                if(lastAllowed)
                                {
                                    thisAllowed=-1;
                                }
                                else
                                {
                                    thisAllowed=1;
                                }
                                if(type == MC_PTP_ADMIN)
                                {
                                    m_AdminCount+=thisAllowed;
                                }
                                if(type == MC_PTP_MINE)
                                {
                                    m_MinerCount+=thisAllowed;
                                }                        
                            }
                        }
                        
/*                        
                        if(type == MC_PTP_ADMIN)
                        {
                            sprintf(msg,"Admin grant: (%08x-%08x-%08x) (%ld-%ld, %ld), In consensus: %d, Admin count: %d, Miner count: %d",
                                    *(uint32_t*)((void*)pldRow.m_Entity),*(uint32_t*)((void*)pldRow.m_Address),*(uint32_t*)((void*)pldRow.m_Admin),
                                    (int64_t)from,(int64_t)to,(int64_t)timestamp,pldRow.m_Consensus,m_AdminCount,m_MinerCount);
                        }
                        if(type == MC_PTP_MINE)
                        {
                            sprintf(msg,"Miner grant: (%08x-%08x-%08x) (%ld-%ld, %ld), In consensus: %d, Admin count: %d, Miner count: %d",
                                    *(uint32_t*)((void*)pldRow.m_Entity),*(uint32_t*)((void*)pldRow.m_Address),*(uint32_t*)((void*)pldRow.m_Admin),
                                    (int64_t)from,(int64_t)to,(int64_t)timestamp,pldRow.m_Consensus,m_AdminCount,m_MinerCount);
                        }
 */ 
                    }                
                    memcpy(&pr_entity,pldRow.m_Entity,sizeof(uint32_t));
                    memcpy(&pr_address,pldRow.m_Address,sizeof(uint32_t));
                    memcpy(&pr_admin,pldRow.m_Admin,sizeof(uint32_t));
                    sprintf(msg,"Grant: (%08x-%08x-%08x-%08x) (%ld-%ld, %ld), In consensus: %d, Admin count: %d, Miner count: %d",
                            pr_entity,pr_address,pldRow.m_Type,pr_admin,
                            (int64_t)from,(int64_t)to,(int64_t)timestamp,pldRow.m_Consensus,m_AdminCount,m_MinerCount);
                    LogString(msg);                        
                }
            }
            else
            {
                err=SetPermissionInternal(lpEntity,lpAddress,types[i],lpAdmin,from,to,timestamp,flags,update_mempool,offset);
                if(err)
                {
                    return err;
                }
            }
        }
    }
    return err;
}

/** Sets mempool checkpoint */


int mc_Permissions::SetCheckPoint()
{
    Lock(1);
    
    m_CheckPointRow=m_Row;
    m_CheckPointAdminCount=m_AdminCount;
    m_CheckPointMinerCount=m_MinerCount;
    m_CheckPointMemPoolSize=m_MemPool->GetCount();
    
    UnLock();
    
    return MC_ERR_NOERROR;
}

/** Rolls back to mempool checkpoint */

int mc_Permissions::RollBackToCheckPoint()
{
    Lock(1);
    
    m_Row=m_CheckPointRow;
    m_AdminCount=m_CheckPointAdminCount;
    m_MinerCount=m_CheckPointMinerCount;
    m_MemPool->SetCount(m_CheckPointMemPoolSize);

    UnLock();
    
    return MC_ERR_NOERROR;
}


/** Returns address of the specific block miner by height*/

int mc_Permissions::GetBlockMiner(uint32_t block,unsigned char* lpMiner)
{
    int err;
    int64_t last_block_row;
    mc_BlockLedgerRow pldRow;
    mc_BlockLedgerRow pldLast;    
    uint32_t type;
            
    Lock(0);
    type=MC_PTP_BLOCK_INDEX;        

    pldLast.Zero();
    pldRow.Zero();
    
    sprintf((char*)pldRow.m_Address,"Block %08X row",block);
    
    GetPermission(pldRow.m_Address,type,(mc_PermissionLedgerRow*)&pldLast);
            
    last_block_row=pldLast.m_ThisRow;
    
    if(last_block_row <= 1)
    {
        LogString("Error: GetBlockMiner: block row not found");
        err=MC_ERR_NOT_FOUND;
        goto exitlbl;
    }
    
    last_block_row--;
    err=MC_ERR_NOERROR;
    
    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: GetBlockMiner: couldn't open ledger");
        err=MC_ERR_FILE_READ_ERROR;
        goto exitlbl;
    }

    m_Ledger->GetRow(last_block_row,(mc_PermissionLedgerRow*)&pldRow);

    if(pldRow.m_Type == MC_PTP_BLOCK_MINER)
    {
        memcpy(lpMiner,pldRow.m_Address,MC_PLS_SIZE_ADDRESS);
    }
    else
    {
        LogString("Error: GetBlockMiner: row type mismatch");
        err=MC_ERR_CORRUPTED;
    }
    
    m_Ledger->Close();

exitlbl:
    UnLock();

    return err;    
}


/** Verifies if block of specific height has specified hash */

int mc_Permissions::VerifyBlockHash(int32_t block,const void* lpHash)
{
    int type;
    int64_t last_block_row;
    int result;
    
    mc_BlockLedgerRow pldRow;
    mc_BlockLedgerRow pldLast;
    
    type=MC_PTP_BLOCK_INDEX;        

    pldRow.Zero();
    pldLast.Zero();
    sprintf((char*)(pldRow.m_Address),"Block %08X row",block);
    
    GetPermission(pldRow.m_Address,type,(mc_PermissionLedgerRow*)&pldLast);
            
    last_block_row=pldLast.m_ThisRow;
    
    if(last_block_row == 0)
    {
        return 0;
    }
        
    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: VerifyBlockHash: couldn't open ledger");
        return 0;
    }

    m_Ledger->GetRow(last_block_row,(mc_PermissionLedgerRow*)&pldRow);

    result=1;
    if(memcmp(pldRow.m_CommitHash,lpHash,MC_PLS_SIZE_HASH))
    {
        result=0;
    }
        
    m_Ledger->Close();

    return result;
}

/** Commits block, external, locks */

int mc_Permissions::Commit(const void* lpMiner,const void* lpHash)
{
    int result;
    Lock(1);
    result=CommitInternal(lpMiner,lpHash);
    UnLock();
    return result;
}

/** Finds blocks in range with governance model change */

uint32_t mc_Permissions::FindGovernanceModelChange(uint32_t from,uint32_t to)
{
    int64_t last_block_row;
    mc_BlockLedgerRow pldRow;
    mc_BlockLedgerRow pldLast;    
    uint32_t type;
    uint32_t block,start;
    uint32_t found=0;
            
    Lock(0);
    
    type=MC_PTP_BLOCK_INDEX;        

    start=from;
    if(start == 0)
    {
        start=1;
    }
    block=to;
    if((int)block > m_Block)
    {
        block=m_Block;
    }
    
    if(block < from)
    {
        goto exitlbl;
    }
    
    pldLast.Zero();
    pldRow.Zero();
    
    sprintf((char*)pldRow.m_Address,"Block %08X row",block);

    GetPermission(pldRow.m_Address,type,(mc_PermissionLedgerRow*)&pldLast);
         
    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: FindGovernanceModelChange: couldn't open ledger");
        goto exitlbl;
    }
    
    last_block_row=pldLast.m_ThisRow;
    while( (block >= start) && (found == 0) )
    {
        m_Ledger->GetRow(last_block_row,(mc_PermissionLedgerRow*)&pldRow);
        if(pldRow.m_BlockFlags & MC_PFB_GOVERNANCE_CHANGE)
        {
            found=block;
            goto exitlbl;
        }
        block--;
        last_block_row=pldRow.m_PrevRow;
    }
    
    m_Ledger->Close();

exitlbl:
    UnLock();

    return found;
}

/** Calculate block flags before commit */

uint32_t mc_Permissions::CalculateBlockFlags()
{
    int i,mprow;
    uint32_t flags=MC_PFB_NONE;
    mc_PermissionLedgerRow pldRow;
    mc_PermissionLedgerRow pldLast;
    mc_PermissionLedgerRow pldNext;
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        if( (flags & MC_PFB_GOVERNANCE_CHANGE) == 0)
        {
            memcpy((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,m_MemPool->GetRow(i),m_Ledger->m_TotalSize);

            if( (pldRow.m_Type & (MC_PTP_ADMIN | MC_PTP_MINE)) &&
                (mc_IsNullEntity(pldRow.m_Entity)) &&
                (pldRow.m_Consensus > 0) )
            {                    
                memcpy((unsigned char*)&pldNext,(unsigned char*)&pldRow,m_Ledger->m_TotalSize);
                pldNext.m_PrevRow=pldRow.m_ThisRow;
                mprow=0;
                while(mprow>=0)
                {
                    mprow=m_MemPool->Seek((unsigned char*)&pldNext+m_Ledger->m_KeyOffset);
                    if(mprow>=0)
                    {
                        memcpy((unsigned char*)&pldNext+m_Ledger->m_KeyOffset,m_MemPool->GetRow(mprow),m_Ledger->m_TotalSize);
                        if(pldNext.m_Consensus)
                        {
                            mprow=-1;
                        }                            
                        pldNext.m_PrevRow=pldNext.m_ThisRow;
                    }
                }
                if( (pldNext.m_Consensus == 0) || (pldNext.m_ThisRow == pldRow.m_ThisRow) )
                {
                    pldLast.Zero();
                    GetPermission(pldRow.m_Entity,pldRow.m_Address,pldRow.m_Type,&pldLast,0);            
                    if(pldLast.m_ThisRow)
                    {
                        if(pldRow.m_BlockFrom != pldLast.m_BlockFrom)
                        {
                            if( ((uint32_t)(m_Block+1) < pldRow.m_BlockFrom) || ((uint32_t)(m_Block+1) < pldLast.m_BlockFrom) )
                            {
                                flags |= MC_PFB_GOVERNANCE_CHANGE;
                            }
                        }
                        if(pldRow.m_BlockTo != pldLast.m_BlockTo)
                        {
                            if( ((uint32_t)(m_Block+1) < pldRow.m_BlockTo) || ((uint32_t)(m_Block+1) < pldLast.m_BlockTo) )
                            {
                                flags |= MC_PFB_GOVERNANCE_CHANGE;
                            }
                        }
                    }
                    else
                    {
                        if(pldRow.m_BlockFrom < pldRow.m_BlockTo)
                        {
                            if( (uint32_t)(m_Block+1) < pldRow.m_BlockTo ) 
                            {
                                flags |= MC_PFB_GOVERNANCE_CHANGE;
                            }
                        }
                    }
                }
            }
        }
    }    
    
    
    return flags;
}

/** Commits block, external, no lock */

int mc_Permissions::CommitInternal(const void* lpMiner,const void* lpHash)
{
    int i,err,thisBlock,value_len,pld_items;
    uint32_t block_flags;
    char msg[256];
    
    mc_PermissionDBRow pdbRow;
    mc_PermissionDBRow pdbAdminMinerRow;
    mc_PermissionLedgerRow pldRow;
    mc_BlockLedgerRow pldBlockRow;
    mc_BlockLedgerRow pldBlockLast;
    
    unsigned char *ptr;
    
    err=MC_ERR_NOERROR;
    
    pdbAdminMinerRow.Zero();
    memcpy(pdbAdminMinerRow.m_Entity,adminminerlist_entity,MC_PLS_SIZE_ENTITY);
    
    block_flags=CalculateBlockFlags();
    pld_items=m_MemPool->GetCount();
    
    if(lpMiner)
    {
        thisBlock=m_Block+1;
        
        GetPermission(lpMiner,MC_PTP_BLOCK_MINER,(mc_PermissionLedgerRow*)&pldBlockLast);
        
        pldBlockRow.Zero();
        memcpy(pldBlockRow.m_Address,lpMiner,MC_PLS_SIZE_ADDRESS);
        pldBlockRow.m_Type=MC_PTP_BLOCK_MINER;
        pldBlockRow.m_PrevRow=pldBlockLast.m_ThisRow;
        pldBlockRow.m_BlockFrom=thisBlock;
        pldBlockRow.m_BlockTo=thisBlock;
        pldBlockRow.m_BlockReceived=thisBlock;
        memcpy(pldBlockRow.m_CommitHash,lpHash,MC_PLS_SIZE_HASH);
        pldBlockRow.m_ThisRow=m_Row;
        pldBlockRow.m_BlockFlags=pldBlockLast.m_BlockFlags & MC_PFB_MINED_BEFORE;
        pldBlockRow.m_BlockFlags |= block_flags;
        m_MemPool->Add((unsigned char*)&pldBlockRow+m_Ledger->m_KeyOffset,(unsigned char*)&pldBlockRow+m_Ledger->m_ValueOffset);
        m_Row++;                
        
        pldBlockRow.Zero();
        sprintf((char*)(pldBlockRow.m_Address),"Block %08X row",thisBlock);
        pldBlockRow.m_Type=MC_PTP_BLOCK_INDEX;
        pldBlockRow.m_PrevRow=m_Row-m_MemPool->GetCount()-1;
        pldBlockRow.m_BlockFrom=thisBlock;
        pldBlockRow.m_BlockTo=thisBlock;
        pldBlockRow.m_BlockReceived=thisBlock;
        memcpy(pldBlockRow.m_CommitHash,lpHash,MC_PLS_SIZE_HASH);
        pldBlockRow.m_ThisRow=m_Row;
        pldBlockRow.m_BlockFlags=pldBlockLast.m_BlockFlags & MC_PFB_MINED_BEFORE;
        pldBlockRow.m_BlockFlags |= block_flags;
        m_MemPool->Add((unsigned char*)&pldBlockRow+m_Ledger->m_KeyOffset,(unsigned char*)&pldBlockRow+m_Ledger->m_ValueOffset);
        m_Row++;                                
    }
    
    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: Commit: couldn't open ledger");
        return MC_ERR_DBOPEN_ERROR;
    }

    thisBlock=m_Block+1;
    if(m_MemPool->GetCount())
    {
        for(i=0;i<pld_items;i++)
        {
            memcpy((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,m_MemPool->GetRow(i),m_Ledger->m_TotalSize);
            if(i)
            {
                m_Ledger->WriteRow(&pldRow);
            }
            else                
            {
                m_Ledger->SetRow(m_Row-m_MemPool->GetCount(),&pldRow);
            }
        }
        
        if(err == MC_ERR_NOERROR)
        {
            for(i=0;i<m_MemPool->GetCount();i++)
            {
                if(err == MC_ERR_NOERROR)
                {
                    memcpy((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,m_MemPool->GetRow(i),m_Ledger->m_TotalSize);

                    pdbRow.Zero();
                    memcpy(pdbRow.m_Entity,pldRow.m_Entity,MC_PLS_SIZE_ENTITY);
                    memcpy(pdbRow.m_Address,pldRow.m_Address,MC_PLS_SIZE_ADDRESS);
                    pdbRow.m_Type=pldRow.m_Type;
                    pdbRow.m_BlockFrom=pldRow.m_BlockFrom;
                    pdbRow.m_BlockTo=pldRow.m_BlockTo;
                    pdbRow.m_LedgerRow=pldRow.m_ThisRow;
                    pdbRow.m_Flags=pldRow.m_Flags;

/*                    
                    if(pldRow.m_FoundInDB == 0 )
                    {
                        m_DBRowCount++;
                    }
 */ 
                    err=m_Database->m_DB->Write((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                (char*)&pdbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                    if(err)
                    {
                        LogString("Error: Commit: DB write error");                        
                    }
                    
                    if( (pdbRow.m_Type == MC_PTP_ADMIN) || (pdbRow.m_Type == MC_PTP_MINE))
                    {
                        if(memcmp(pdbRow.m_Entity,null_entity,MC_PLS_SIZE_ENTITY) == 0)
                        {                    
                            pdbAdminMinerRow.m_Type=pdbRow.m_Type;
                            memcpy(pdbAdminMinerRow.m_Address,pdbRow.m_Address,MC_PLS_SIZE_ADDRESS);
                            err=m_Database->m_DB->Write((char*)&pdbAdminMinerRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                        (char*)&pdbAdminMinerRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                        }
                    }
                }                
            }
        }
    }
    
    if(err == MC_ERR_NOERROR)
    {
        pdbRow.Zero();
        
        ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
        if(ptr)
        {
            memcpy((char*)&pdbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        }
/*        
        else
        {
            m_DBRowCount++;            
        }
*/
        pdbRow.m_BlockTo=thisBlock;
        pdbRow.m_LedgerRow=m_Row;
        err=m_Database->m_DB->Write((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                    (char*)&pdbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            LogString("Error: Commit: DB write error (0)");                                    
        }
    }        
    
    if(err == MC_ERR_NOERROR)
    {
        for(i=pld_items;i<m_MemPool->GetCount();i++)
        {
            memcpy((unsigned char*)&pldBlockRow+m_Ledger->m_KeyOffset,m_MemPool->GetRow(i),m_Ledger->m_TotalSize);
            if(i)
            {
                m_Ledger->WriteRow((mc_PermissionLedgerRow*)&pldBlockRow);
            }
            else                
            {
                m_Ledger->SetRow(m_Row-m_MemPool->GetCount(),(mc_PermissionLedgerRow*)&pldBlockRow);
            }
        }        
    }    
    
    if(err == MC_ERR_NOERROR)
    {
        m_Ledger->GetRow(0,&pldRow);

        thisBlock=m_Block+1;
        pldRow.m_BlockTo=thisBlock;
        pldRow.m_PrevRow=m_Row;
        m_Ledger->SetRow(0,&pldRow);
    }
    
    m_Ledger->Flush();
    m_Ledger->Close();
    
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            LogString("Error: Commit: DB commit error");                                    
        }
    }    

    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: Commit: couldn't open ledger");
        return MC_ERR_DBOPEN_ERROR;
    }

    if(err == MC_ERR_NOERROR)
    {
        m_Block++;
        UpdateCounts();        
        m_Block--;
        m_ClearedAdminCount=m_AdminCount;
        m_ClearedMinerCount=m_MinerCount;
        m_ClearedMinerCountForMinerVerification=m_MinerCount;
        for(i=pld_items;i<m_MemPool->GetCount();i++)
        {
            m_Ledger->GetRow(m_Row-m_MemPool->GetCount()+i,(mc_PermissionLedgerRow*)&pldBlockRow);
            pldBlockRow.m_AdminCount=m_AdminCount;
            pldBlockRow.m_MinerCount=m_MinerCount;
            m_Ledger->SetRow(m_Row-m_MemPool->GetCount()+i,(mc_PermissionLedgerRow*)&pldBlockRow);
        }        
    }    
    m_Ledger->Close();
    
    
    if(err)
    {
        RollBackInternal(m_Block);        
    }
    else
    {
        m_MemPool->Clear();
        StoreBlockInfoInternal(lpMiner,lpHash,0);
        m_Block++;
    }

    
    sprintf(msg,"Block commit: %9d (Hash: %08x, Miner: %08x), Admin count: %d, Miner count: %d, Ledger Rows: %ld",
            m_Block,*(uint32_t*)lpHash,*(uint32_t*)lpMiner,m_AdminCount,m_MinerCount,m_Row);
    LogString(msg);
    return err;
}

/** Returns address of the specific block by hash */

int mc_Permissions::GetBlockMiner(const void* lpHash, unsigned char* lpMiner,uint32_t *lpAdminMinerCount)
{
    int err,value_len;
    mc_BlockMinerDBRow pdbBlockMinerRow;
    
    unsigned char *ptr;
    
    Lock(0);
    
    err=MC_ERR_NOERROR;
    pdbBlockMinerRow.Zero();
    memcpy(pdbBlockMinerRow.m_BlockHash,lpHash,MC_PLS_SIZE_ENTITY);
    pdbBlockMinerRow.m_Type=MC_PTP_BLOCK_MINER;

    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbBlockMinerRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
    if(ptr)
    {
        memcpy((char*)&pdbBlockMinerRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);        
        memcpy(lpMiner,pdbBlockMinerRow.m_Address,MC_PLS_SIZE_ADDRESS);
        *lpAdminMinerCount=pdbBlockMinerRow.m_AdminMinerCount;
    }
    else
    {
        err=MC_ERR_NOT_FOUND;
    }
    
    UnLock();

    return err;
}

int mc_Permissions::GetBlockAdminMinerGrants(const void* lpHash, int record, int32_t* offsets)
{
    int err,value_len;
    mc_AdminMinerGrantDBRow pdbAdminMinerGrantRow;
    
    unsigned char *ptr;
    
    Lock(0);
    
    err=MC_ERR_NOERROR;
    
    pdbAdminMinerGrantRow.Zero();
    memcpy(pdbAdminMinerGrantRow.m_BlockHash,lpHash,MC_PLS_SIZE_ENTITY);
    pdbAdminMinerGrantRow.m_Type=MC_PTP_BLOCK_MINER;
    pdbAdminMinerGrantRow.m_RecordID=record;

    ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbAdminMinerGrantRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
    
    if(ptr)
    {
        memcpy((char*)&pdbAdminMinerGrantRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);        
        memcpy(offsets,pdbAdminMinerGrantRow.m_Offsets,MC_PLS_SIZE_OFFSETS_PER_ROW*sizeof(int32_t));
    }
    else
    {
        err=MC_ERR_NOT_FOUND;
    }
    
    UnLock();

    return err;    
}

int mc_Permissions::IncrementBlock(uint32_t admin_miner_count)
{
    Lock(1);
    m_Block++;
    if(admin_miner_count)
    {
        m_AdminCount=(admin_miner_count >> 16) & 0xffff;
        m_MinerCount=admin_miner_count & 0xffff;
    }
    else
    {
        UpdateCounts();        
    }
    if(m_CopiedRow > 0)
    {
        m_ClearedMinerCountForMinerVerification=m_MinerCount;
    }
/*    
    char msg[256];
    sprintf(msg,"Verifier Increment: %9d, Admin count: %d, Miner count: %d, Ledger Rows: %ld",
            m_Block,m_AdminCount,m_MinerCount,m_Row);
    LogString(msg);
*/    
    UnLock();
    return MC_ERR_NOERROR;
}


/** Stores info about block miner and admin/miner grant transactions */

int mc_Permissions::StoreBlockInfo(const void* lpMiner,const void* lpHash)
{    
    int result;
    Lock(1);
    result=StoreBlockInfoInternal(lpMiner,lpHash,1);
    m_Block++;
    UnLock();
    return result;
}

void mc_Permissions::SaveTmpCounts()
{
    Lock(1);
    m_TmpSavedAdminCount=m_AdminCount;
    m_TmpSavedMinerCount=m_MinerCount;
    UnLock();    
}

int mc_Permissions::StoreBlockInfoInternal(const void* lpMiner,const void* lpHash,int update_counts)
{    
    int i,err,amg_items,last_offset;
    
    mc_PermissionDBRow pdbAdminMinerRow;
    mc_PermissionLedgerRow pldRow;
    mc_BlockMinerDBRow pdbBlockMinerRow;
    mc_AdminMinerGrantDBRow pdbAdminMinerGrantRow;
        
    if(mc_gState->m_NetworkParams->GetInt64Param("supportminerprecheck") == 0)                                
    {
        return MC_ERR_NOERROR;        
    }    
    
    if(MCP_ANYONE_CAN_MINE)                                
    {
        return MC_ERR_NOERROR;        
    }    
    
    err=MC_ERR_NOERROR;
    
    amg_items=0;
    last_offset=0;
    pdbAdminMinerRow.Zero();
    memcpy(pdbAdminMinerRow.m_Entity,adminminerlist_entity,MC_PLS_SIZE_ENTITY);
    pdbAdminMinerGrantRow.Zero();
    memcpy(pdbAdminMinerGrantRow.m_BlockHash,lpHash,MC_PLS_SIZE_ENTITY);
    pdbAdminMinerGrantRow.m_Type=MC_PTP_BLOCK_MINER;
    
    for(i=0;i<m_MemPool->GetCount();i++)
    {
        if(err == MC_ERR_NOERROR)
        {
            memcpy((unsigned char*)&pldRow+m_Ledger->m_KeyOffset,m_MemPool->GetRow(i),m_Ledger->m_TotalSize);

            if( (pldRow.m_Type == MC_PTP_ADMIN) || (pldRow.m_Type == MC_PTP_MINE))
            {
                if(memcmp(pldRow.m_Entity,null_entity,MC_PLS_SIZE_ENTITY) == 0)
                {                    
                    if( pldRow.m_BlockReceived == (uint32_t)(m_Block+1) )
                    {                       
                        pdbAdminMinerRow.m_Type=pldRow.m_Type;
                        memcpy(pdbAdminMinerRow.m_Address,pldRow.m_Address,MC_PLS_SIZE_ADDRESS);
                        err=m_Database->m_DB->Write((char*)&pdbAdminMinerRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                    (char*)&pdbAdminMinerRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                        if(err)
                        {
                            LogString("Error: StoreBlockInfoInternal: DB write error");                        
                        }

                        if(pldRow.m_Offset != last_offset)
                        {
                            if( (amg_items > 0) && ((amg_items % MC_PLS_SIZE_OFFSETS_PER_ROW) == 0) )
                            {
                                err=m_Database->m_DB->Write((char*)&pdbAdminMinerGrantRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                                            (char*)&pdbAdminMinerGrantRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                                if(err)
                                {
                                    LogString("Error: StoreBlockInfoInternal: DB write error");                        
                                }
                                memset(pdbAdminMinerGrantRow.m_Offsets,0,MC_PLS_SIZE_OFFSETS_PER_ROW*sizeof(int32_t));
                            }                    
                            pdbAdminMinerGrantRow.m_RecordID=(amg_items / MC_PLS_SIZE_OFFSETS_PER_ROW) + 1;
                            pdbAdminMinerGrantRow.m_Offsets[amg_items % MC_PLS_SIZE_OFFSETS_PER_ROW]=pldRow.m_Offset;
                            last_offset=pldRow.m_Offset;
                            amg_items++;
                        }
                    }
                }                
            }
        }                
    }
    
    if(err == MC_ERR_NOERROR)
    {
        if(amg_items > 0)
        {
            err=m_Database->m_DB->Write((char*)&pdbAdminMinerGrantRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                        (char*)&pdbAdminMinerGrantRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
            if(err)
            {
                LogString("Error: StoreBlockInfoInternal: DB write error");                        
            }
        }                            
    }
    
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
        if(err)
        {
            LogString("Error: StoreBlockInfoInternal: DB commit error");                                    
        }
    }
    
    if(update_counts)
    {
        m_Block++;
        UpdateCounts();
        m_ClearedMinerCountForMinerVerification=m_MinerCount;
        m_Block--;
    }
    
    pdbBlockMinerRow.Zero();
    memcpy(pdbBlockMinerRow.m_BlockHash,lpHash,MC_PLS_SIZE_ENTITY);
    pdbBlockMinerRow.m_Type=MC_PTP_BLOCK_MINER;
    pdbBlockMinerRow.m_AdminMinerCount=0;
    if( (m_AdminCount <= 0xffff) && (m_MinerCount <= 0xffff) )
    {
        pdbBlockMinerRow.m_AdminMinerCount = (m_AdminCount << 16) + m_MinerCount;
    }
    memcpy(pdbBlockMinerRow.m_Address,lpMiner,MC_PLS_SIZE_ADDRESS);
    err=m_Database->m_DB->Write((char*)&pdbBlockMinerRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                (char*)&pdbBlockMinerRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,0);
    if(err)
    {
        LogString("Error: StoreBlockInfoInternal: DB write error");                        
    }
    
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->m_DB->Commit(0);
        if(err)
        {
            LogString("Error: StoreBlockInfoInternal: DB commit error");                                    
        }
    }
    
/*    
    char msg[256];

    sprintf(msg,"Block store: %9d (Hash: %08x, Miner: %08x), Admin count: %d, Miner count: %d, Ledger Rows: %ld",
            m_Block+1,*(uint32_t*)lpHash,*(uint32_t*)lpMiner,m_AdminCount,m_MinerCount,m_Row);
    LogString(msg);
*/    
    return MC_ERR_NOERROR;
}

/** Rolls one block back */

int mc_Permissions::RollBack()
{
    return RollBack(m_Block-1);
}

/** Rolls back to specific height, external, locks */

int mc_Permissions::RollBack(int block)
{
    int result;
    Lock(1);
    result=RollBackInternal(block);
    UnLock();
    return result;    
}

/** Rolls back to specific height, internal, no lock */

int mc_Permissions::RollBackInternal(int block)
{
    int err;
    int take_it,value_len;
    uint64_t this_row,prev_row;
    mc_PermissionDBRow pdbRow;
    mc_PermissionLedgerRow pldRow;
    unsigned char *ptr;
    char msg[256];
    
    err=MC_ERR_NOERROR;
    
    if(m_Ledger->Open() <= 0)
    {
        LogString("Error: Rollback: couldn't open ledger");
        return MC_ERR_DBOPEN_ERROR;
    }

    ClearMemPoolInternal();
    
    this_row=m_Row-1;
    take_it=1;
    if(this_row == 0)
    {
        take_it=0;
    }
    
    while(take_it && (this_row>0))
    {
        m_Ledger->GetRow(this_row,&pldRow);
        
        if((int32_t)pldRow.m_BlockReceived > block)
        {
            pdbRow.Zero();
            memcpy(pdbRow.m_Entity,pldRow.m_Entity,MC_PLS_SIZE_ENTITY);
            memcpy(pdbRow.m_Address,pldRow.m_Address,MC_PLS_SIZE_ADDRESS);
            pdbRow.m_Type=pldRow.m_Type;
            prev_row=pldRow.m_PrevRow;
            if(prev_row)
            {
                m_Ledger->GetRow(prev_row,&pldRow);
                pdbRow.m_BlockFrom=pldRow.m_BlockFrom;
                pdbRow.m_BlockTo=pldRow.m_BlockTo;
                pdbRow.m_LedgerRow=prev_row;
                pdbRow.m_Flags=pldRow.m_Flags;
                err=m_Database->m_DB->Write((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                            (char*)&pdbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);                
                if(err)
                {
                    LogString("Error: Rollback: db write error");                    
                }
            }
            else
            {
//                m_DBRowCount--;
                err=m_Database->m_DB->Delete((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,MC_OPT_DB_DATABASE_TRANSACTIONAL);
                if(err)
                {
                    LogString("Error: Rollback: db delete error");                    
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
            this_row--;
        }
    }
        
    this_row++;

    if(err == MC_ERR_NOERROR)
    {
        pdbRow.Zero();
        
        ptr=(unsigned char*)m_Database->m_DB->Read((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,&value_len,0,&err);
        if(ptr)
        {
            memcpy((char*)&pdbRow+m_Database->m_ValueOffset,ptr,m_Database->m_ValueSize);
        }
/*        
        else
        {
            m_DBRowCount++;            
        }
*/
        pdbRow.m_BlockTo=block;
        pdbRow.m_LedgerRow=this_row;
        err=m_Database->m_DB->Write((char*)&pdbRow+m_Database->m_KeyOffset,m_Database->m_KeySize,
                                    (char*)&pdbRow+m_Database->m_ValueOffset,m_Database->m_ValueSize,MC_OPT_DB_DATABASE_TRANSACTIONAL);        
        if(err)
        {
            LogString("Error: Rollback: db write error (0)");                    
        }
    }        
    
    if(err == MC_ERR_NOERROR)
    {
        err=m_Database->m_DB->Commit(MC_OPT_DB_DATABASE_TRANSACTIONAL);
    }    
    
    if(err == MC_ERR_NOERROR)
    {
        m_Ledger->GetRow(0,&pldRow);

        pldRow.m_BlockTo=block;
        pldRow.m_PrevRow=this_row;
        m_Ledger->SetRow(0,&pldRow);        
    }
    
    m_Ledger->Close();  
    
    if(err == MC_ERR_NOERROR)
    {
        m_Block=block;
        m_Row=this_row;
        UpdateCounts();        
        m_ClearedAdminCount=m_AdminCount;
        m_ClearedMinerCount=m_MinerCount;
    }
    
    sprintf(msg,"Block rollback: %9d, Admin count: %d, Miner count: %d, Ledger Rows: %ld",m_Block,m_AdminCount,m_MinerCount,m_Row);
    LogString(msg);
    return err;   
}



