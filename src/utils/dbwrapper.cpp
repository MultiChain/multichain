// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

//#include "utils/declare.h"
//#include "utils/dbwrapper.h"

#include "leveldb/include/leveldb/c.h"
#include "multichain/multichain.h"

int cs_Database::Zero()
{
    
    m_DB=NULL;
    m_OpenOptions=NULL;
    m_ReadOptions=NULL;
    m_IterOptions=NULL;
    m_WriteOptions=NULL;
    m_SyncOptions=NULL;
    m_Cache=NULL;
    m_Iterator=NULL;
    m_WriteBatch=NULL;
    
    m_ReadBuffer=NULL;
    m_ReadBufferSize=0;
    
    m_ShMemKey=0;
    m_MaxClients=MC_DCT_DB_DEFAULT_MAX_CLIENTS;
    m_KeySize=MC_DCT_DB_DEFAULT_MAX_KEY_SIZE;
    m_ValueSize=MC_DCT_DB_DEFAULT_MAX_VALUE_SIZE;
    m_MaxRows=MC_DCT_DB_DEFAULT_MAX_ROWS;
    m_Semaphore=NULL;
    m_Signal=0;            
    m_LastError=0;
    
    m_hShMem=NULL;
    m_lpShMem=NULL;
    m_lpShMemSlot=NULL;
    
    m_Options=MC_OPT_DB_DATABASE_DEFAULT;
    m_Status=MC_STT_DB_DATABASE_CLOSED;  
    m_Log=NULL;

    m_WriteCount=0;
    m_DeleteCount=0;
    
    m_Name[0]=0;
    
    return MC_ERR_NOERROR;
}

int cs_Database::Destroy()
{
    if(m_lpShMem)
    {
//        __US_ShMemUnmap(m_lpShMem);
        m_lpShMem=NULL;
    }

    if(m_hShMem)
    {
//        __US_ShMemClose(m_hShMem);
        m_hShMem=NULL;
    }

    
    if(m_ReadBuffer)
    {
        mc_Delete(m_ReadBuffer);
        m_ReadBuffer=NULL;
    }
    
    Zero();
    
    return MC_ERR_NOERROR;
}

int cs_Database::Open(char *name,int Options)
{
    char *err = NULL;
//    uint32_t pid;
//    double start_time;


    if(m_ReadBuffer  == NULL)
    {
        m_ReadBuffer=(char*)mc_New(MC_DCT_DB_READ_BUFFER_SIZE);
        if(m_ReadBuffer  == NULL)
        {
            return MC_ERR_ALLOCATION;
        }
    }
    
    strcpy(m_Name,name);
    
    m_Options=Options;
    
    switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
    {
        case MC_OPT_DB_DATABASE_LEVELDB:    
//            return MC_ERR_NOT_SUPPORTED;

            if((m_Options & MC_OPT_DB_DATABASE_DELAYED_OPEN) == 0)
            {
                m_OpenOptions = (void*)leveldb_options_create();
                m_ReadOptions = (void*)leveldb_readoptions_create();
                m_IterOptions = (void*)leveldb_readoptions_create();
                m_WriteOptions = (void*)leveldb_writeoptions_create();
                m_SyncOptions = (void*)leveldb_writeoptions_create();
                m_Cache = (void*)leveldb_cache_create_lru(128 << 20);
                
                leveldb_readoptions_set_fill_cache((leveldb_readoptions_t*)m_ReadOptions,0);
                leveldb_readoptions_set_fill_cache((leveldb_readoptions_t*)m_IterOptions,0);
                leveldb_writeoptions_set_sync((leveldb_writeoptions_t*)m_SyncOptions,1);
                
                leveldb_options_set_cache((leveldb_options_t*)m_OpenOptions,(leveldb_cache_t*)m_Cache);
                leveldb_options_set_max_open_files((leveldb_options_t*)m_OpenOptions,128);
                if(Options & MC_OPT_DB_DATABASE_CREATE_IF_MISSING)
                {
                    leveldb_options_set_create_if_missing((leveldb_options_t*)m_OpenOptions, 1);
                }
                
                if(Options & MC_OPT_DB_DATABASE_TRANSACTIONAL)
                {
                    m_WriteBatch=(void*)leveldb_writebatch_create();
                    if(m_WriteBatch == NULL)
                    {
                        return MC_ERR_DBOPEN_ERROR;            
                    }
                    leveldb_writebatch_clear((leveldb_writebatch_t*)m_WriteBatch);
                }

                m_DB = leveldb_open((leveldb_options_t*)m_OpenOptions, name, &err);

                if (err != NULL) 
                {
                    Destroy();
                    printf("%s\n",err);
                    leveldb_free(err);

                    return MC_ERR_DBOPEN_ERROR;
                }
//                m_Iterator=(void*)leveldb_create_iterator((leveldb_t*)m_DB,(leveldb_readoptions_t*)m_IterOptions);
            }
            
            break;
            
        case MC_OPT_DB_DATABASE_REMOTE_SHMEM:    
            
            return MC_ERR_DBOPEN_ERROR;
/*            
            if(m_ShMemKey == 0)
            {
                ierr=MC_ERR_DB_SHMEM_KEY_NOT_SET;
                return ierr;
            }
    
            m_hShMem=NULL;            
            m_lpShMem=NULL;
    
            if(__US_ShMemOpen(&m_hShMem,m_ShMemKey))
            {
                ierr=MC_ERR_DB_SHMEM_CANNOT_OPEN;
                return ierr;
            }
    
            m_lpShMem=(uint64_t*)__US_ShMemMap(m_hShMem);
            if(m_lpShMem == NULL)
            {
                __US_ShMemClose(m_hShMem);
                m_hShMem=NULL;
                ierr=MC_ERR_DB_SHMEM_CANNOT_OPEN;
                return ierr;
            }
            
            pid=__US_GetProcessId();
            
            m_lpShMemSlot=NULL;
            start_time=cs_TimeNow();
            while((m_lpShMemSlot==NULL) && (cs_TimeNow()-start_time<MC_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
            {
                if(m_lpShMem[MC_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID]!=pid)
                {
                    m_lpShMem[MC_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID]=pid;
                }
                else
                {
                    m_lpShMemSlot=m_lpShMem+(m_lpShMem[MC_OFF_DB_SHMEM_HEAD_DATA_OFFSET]+
                            m_lpShMem[MC_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID]*m_lpShMem[MC_OFF_DB_SHMEM_HEAD_SLOT_SIZE])/sizeof(uint64_t);
                    m_lpShMem[MC_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID]=0;
                    m_lpShMem[MC_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID]=0;
                }                
            }
            
            if(m_lpShMemSlot==NULL)
            {
                __US_ShMemUnmap(m_lpShMem);
                m_lpShMem=NULL;                
                __US_ShMemClose(m_hShMem);
                m_hShMem=NULL;
                ierr=MC_ERR_DB_SHMEM_CANNOT_CONNECT;
                return ierr;
            }
  */          
            break;
        default:
            return MC_ERR_DBOPEN_ERROR;
            break;
    }
            
    m_Status=MC_STT_DB_DATABASE_OPENED;  
    
    return MC_ERR_NOERROR;    
}

int cs_Database::Close()
{
//    uint64_t *rptr;
//    uint32_t pid; 
//    double start_time;
    
    switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
    {
        case MC_OPT_DB_DATABASE_LEVELDB:    

            if(m_Cache)
            {
                leveldb_cache_destroy((leveldb_cache_t*)m_Cache);
                m_Cache=NULL;
            }

            if(m_OpenOptions)
            {
                leveldb_options_destroy((leveldb_options_t*)m_OpenOptions);
                m_OpenOptions=NULL;
            }

            if(m_ReadOptions)
            {
                leveldb_readoptions_destroy((leveldb_readoptions_t*)m_ReadOptions);
                m_ReadOptions=NULL;
            }

            if(m_IterOptions)
            {
                leveldb_readoptions_destroy((leveldb_readoptions_t*)m_IterOptions);
                m_IterOptions=NULL;
            }

            if(m_WriteOptions)
            {
                leveldb_writeoptions_destroy((leveldb_writeoptions_t*)m_WriteOptions);
                m_WriteOptions=NULL;
            }

            if(m_SyncOptions)
            {
                leveldb_writeoptions_destroy((leveldb_writeoptions_t*)m_SyncOptions);
                m_SyncOptions=NULL;
            }

            if(m_WriteBatch)
            {
                leveldb_writebatch_destroy((leveldb_writebatch_t*)m_WriteBatch);
                m_WriteBatch=NULL;
            }

            if(m_Iterator)
            {
                leveldb_iter_destroy((leveldb_iterator_t*)m_Iterator);
                m_Iterator=NULL;
            }
            
            if(m_DB)
            {
                switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
                {
                    case MC_OPT_DB_DATABASE_LEVELDB:                          
                        leveldb_close((leveldb_t*)m_DB);
                        break;
                }
                m_DB=NULL;
            }
            break;
        case MC_OPT_DB_DATABASE_REMOTE_SHMEM:    
/*            
            if(m_lpShMemSlot)
            {
                pid=__US_GetProcessId();
                if(m_lpShMemSlot[MC_OFF_DB_SHMEM_SLOT_PROCESS_ID] == pid)
                {
                    m_lpShMemSlot[MC_OFF_DB_SHMEM_SLOT_ROW_COUNT]=0;
                    m_lpShMemSlot[MC_OFF_DB_SHMEM_SLOT_RESULT]=MC_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED;
                    rptr=m_lpShMemSlot+m_lpShMem[MC_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET]/sizeof(uint64_t);
                    rptr[MC_OFF_DB_SHMEM_ROW_RESPONSE]=MC_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED;
                    rptr[MC_OFF_DB_SHMEM_ROW_REQUEST]=MC_PRT_DB_SHMEM_ROW_REQUEST_CLOSE;
                    m_lpShMemSlot[MC_OFF_DB_SHMEM_SLOT_ROW_COUNT]=1;
                    start_time=cs_TimeNow();
                    while((m_lpShMemSlot[MC_OFF_DB_SHMEM_SLOT_PROCESS_ID] == pid) && (cs_TimeNow()-start_time<MC_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT))
                    {
                        __US_Sleep(1);
                    }
                }
            }
 */ 
            break;
    }
    
    return MC_ERR_NOERROR;
}

int cs_Database::Write(char *key,int key_len,char *value,int value_len,int Options)
{
    char *err = NULL;    
    int klen=key_len;
    int vlen=value_len;
    
    
    m_WriteCount++;
    
    if(key == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    if(value == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    if(klen<0)klen=strlen(key);
    if(vlen<0)vlen=strlen(value);
    
    if(m_DB == NULL)
    {
        return MC_ERR_DBOPEN_ERROR;
    }
    switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
    {
        case MC_OPT_DB_DATABASE_LEVELDB:    
//            return MC_ERR_NOT_SUPPORTED;

            if(Options & MC_OPT_DB_DATABASE_TRANSACTIONAL)
            {
                leveldb_writebatch_put((leveldb_writebatch_t*)m_WriteBatch, key, klen, value, vlen);        
            }
            else
            {
                leveldb_put((leveldb_t*)m_DB, (leveldb_writeoptions_t*)m_SyncOptions, key, klen, value, vlen, &err);        
            }

            if (err != NULL) 
            {
                leveldb_free(err);

                return MC_ERR_INTERNAL_ERROR;
            }

            break;
    }
    
    return MC_ERR_NOERROR;
}

char *cs_Database::MoveNext(int *error)
{
    int NewSize;
    char *lpNewBuffer;
    size_t vallen;
    size_t keylen;
    const char *lpKey;
    const char *lpValue;
    
    leveldb_iter_next((leveldb_iterator_t*)m_Iterator);
    
    if(!leveldb_iter_valid((leveldb_iterator_t*)m_Iterator))
    {
        return NULL;
    }
    
    lpKey=leveldb_iter_key((leveldb_iterator_t*)m_Iterator, &keylen);
    lpValue=leveldb_iter_value((leveldb_iterator_t*)m_Iterator, &vallen);
   
    if((int)(keylen+vallen+1)>m_ReadBufferSize)
    {
        NewSize=((keylen+vallen)/MC_DCT_DB_READ_BUFFER_SIZE + 1) * MC_DCT_DB_READ_BUFFER_SIZE;
        lpNewBuffer=(char*)mc_New(NewSize);
        if(lpNewBuffer  == NULL)
        {
            *error=MC_ERR_ALLOCATION;
            return NULL;
        }

        mc_Delete(m_ReadBuffer);
        m_ReadBuffer=lpNewBuffer;
        m_ReadBufferSize=NewSize;
    }

    memcpy(m_ReadBuffer,lpKey,keylen);
    memcpy(m_ReadBuffer+keylen,lpValue,vallen);
    m_ReadBuffer[keylen+vallen]=0;

    return m_ReadBuffer;
}

char *cs_Database::MoveNextKeyLevels(int key_levels,int *error)
{
    *error=MC_ERR_OPERATION_NOT_SUPPORTED;
    return NULL;
}

char *cs_Database::Read(char *key,int key_len,int *value_len,int Options,int *error)
{
    char *err = NULL;
    const char *lpIterRead;
    const char *lpIterReadKey;
    char *lpRead;
    char *lpNewBuffer;
    int NewSize;
    size_t vallen;
    size_t kallen;

    int klen=key_len;
    
    *value_len=0;
    *error=MC_ERR_NOERROR;
    
    if(key == NULL)
    {
        *error=MC_ERR_INTERNAL_ERROR;
        return NULL;
    }
    if(klen<0)klen=strlen(key);
    
    if(m_DB == NULL)
    {
        *error=MC_ERR_DBOPEN_ERROR;
        return NULL;
    }

    lpRead=NULL;
    lpIterRead=NULL;
    lpIterReadKey=NULL;
    
    switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
    {
        case MC_OPT_DB_DATABASE_LEVELDB:    
            
//            *error=MC_ERR_NOT_SUPPORTED;
//            return NULL;

            if(Options & (MC_OPT_DB_DATABASE_SEEK_ON_READ | MC_OPT_DB_DATABASE_NEXT_ON_READ) )
            {
//                *error=MC_ERR_OPERATION_NOT_SUPPORTED;
//                return NULL;                
                lpIterRead=NULL;
                
                if(m_Iterator)
                {
                    leveldb_iter_destroy((leveldb_iterator_t*)m_Iterator);
                    m_Iterator=NULL;                   
                }
                m_Iterator=(void*)leveldb_create_iterator((leveldb_t*)m_DB,(leveldb_readoptions_t*)m_IterOptions);

                leveldb_iter_seek((leveldb_iterator_t*)m_Iterator, key, klen);
                if(leveldb_iter_valid((leveldb_iterator_t*)m_Iterator))
                {
                    lpIterReadKey=leveldb_iter_key((leveldb_iterator_t*)m_Iterator, &kallen);
                    if(lpIterReadKey)
                    {
                        if( ((int)kallen == klen) && ( ((Options & MC_OPT_DB_DATABASE_NEXT_ON_READ) != 0) || (memcmp(lpIterReadKey,key,klen) == 0) ) )
                        {
                            lpIterRead=leveldb_iter_value((leveldb_iterator_t*)m_Iterator, &vallen);                            
                        }
                        else
                        {
                            lpIterRead=NULL;
                        }
                    }                    
                }
            }
            else
            {
                lpRead = leveldb_get((leveldb_t*)m_DB, (leveldb_readoptions_t*)m_ReadOptions, key, klen, &vallen, &err);
            }

            if (err != NULL) 
            {
                leveldb_free(err);
                *error=MC_ERR_INTERNAL_ERROR;
                return NULL;
            }

            if((lpRead == NULL) && (lpIterRead == NULL))
            {
                return NULL;    
            }

            *value_len=vallen;

            break;
        default:
            *error=MC_ERR_DBOPEN_ERROR;
            return NULL;
            break;
    }
    
    if(*value_len+klen+1>m_ReadBufferSize)
    {
        NewSize=((*value_len+klen)/MC_DCT_DB_READ_BUFFER_SIZE + 1) * MC_DCT_DB_READ_BUFFER_SIZE;
        lpNewBuffer=(char*)mc_New(NewSize);
        if(lpNewBuffer  == NULL)
        {
            *value_len=0;
            *error=MC_ERR_ALLOCATION;
            return NULL;
        }

        mc_Delete(m_ReadBuffer);
        m_ReadBuffer=lpNewBuffer;
        m_ReadBufferSize=NewSize;
    }

    if(lpRead)
    {
        memcpy(m_ReadBuffer,lpRead,*value_len);
        m_ReadBuffer[*value_len]=0;
    }
    if(lpIterRead)
    {
        if(Options & MC_OPT_DB_DATABASE_NEXT_ON_READ)
        {
            memcpy(m_ReadBuffer,lpIterReadKey,klen);
            memcpy(m_ReadBuffer+klen,lpIterRead,*value_len);
            m_ReadBuffer[klen+*value_len]=0;
        }
        else
        {
            memcpy(m_ReadBuffer,lpIterRead,*value_len);
            m_ReadBuffer[*value_len]=0;
        }
    }
    
    switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
    {
        case MC_OPT_DB_DATABASE_LEVELDB:    

            if(lpRead)
            {
                leveldb_free(lpRead);
            }

            break;
    }
    
    return m_ReadBuffer;    
}

int cs_Database::Delete(char *key,int key_len,int Options)
{    
    char *err = NULL;
    int klen=key_len;

    m_DeleteCount++;
    
    if(key == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    if(klen<0)klen=strlen(key);

    if(m_DB == NULL)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
    {
        case MC_OPT_DB_DATABASE_LEVELDB:    
//            return MC_ERR_NOT_SUPPORTED;


            if(Options & MC_OPT_DB_DATABASE_TRANSACTIONAL)
            {
                leveldb_writebatch_delete((leveldb_writebatch_t*)m_WriteBatch, key, klen);        
            }
            else
            {
                leveldb_delete((leveldb_t*)m_DB, (leveldb_writeoptions_t*)m_SyncOptions, key, klen, &err);
            }

            if (err != NULL) 
            {
                leveldb_free(err);

                return MC_ERR_INTERNAL_ERROR;
            }

            break;
    }
    
    return MC_ERR_NOERROR;
    
}

int cs_Database::Commit(int Options)
{
    char *err = NULL;
    char msg[100];

    sprintf(msg,"Writes: %6d; Deletes: %6d;",m_WriteCount,m_DeleteCount);
//    cs_LogMessage(m_Log,MC_LOG_REPORT,"C-0083","Commit",msg);
    
    m_WriteCount=0;
    m_DeleteCount=0;
    
    if(m_DB == NULL)
    {
        return MC_ERR_DBOPEN_ERROR;
    }
    
    switch(m_Options & MC_OPT_DB_DATABASE_TYPE_MASK)
    {
        case MC_OPT_DB_DATABASE_LEVELDB:    
//            return MC_ERR_NOT_SUPPORTED;


            if(Options & MC_OPT_DB_DATABASE_TRANSACTIONAL)
            {
                if(Options & MC_OPT_DB_DATABASE_SYNC_ON_COMMIT)
                {
                    leveldb_write((leveldb_t*)m_DB, (leveldb_writeoptions_t*)m_SyncOptions, (leveldb_writebatch_t*)m_WriteBatch, &err);        
                }
                else
                {
                    leveldb_write((leveldb_t*)m_DB, (leveldb_writeoptions_t*)m_WriteOptions, (leveldb_writebatch_t*)m_WriteBatch, &err);                            
                }
                leveldb_writebatch_clear((leveldb_writebatch_t*)m_WriteBatch);
            }

            if (err != NULL) 
            {
                leveldb_free(err);

                return MC_ERR_INTERNAL_ERROR;
            }

            break;
    }
    return MC_ERR_NOERROR;    
}

int cs_Database::Synchronize()
{
    Close();
    
    return Open(m_Name,m_Options);
}


int cs_Database::SetOption(const char* option, int suboption, int value)
{
    if(strcmp(option,"ShMemKey") == 0)
    {
        m_ShMemKey=value;
    }
    
    if(strcmp(option,"KeySize") == 0)
    {
        m_KeySize=value;
    }

    if(strcmp(option,"ValueSize") == 0)
    {
        m_ValueSize=value;
    }
    
    if(strcmp(option,"MaxRows") == 0)
    {
        m_ValueSize=value;
    }
    
    return MC_ERR_NOERROR;
    
}





