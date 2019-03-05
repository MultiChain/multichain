// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAINDB_H
#define	MULTICHAINDB_H

#define MC_DCT_DB_MAX_PATH                     1024

#define MC_DCT_DB_READ_BUFFER_SIZE                         4096
#define MC_DCT_DB_DEFAULT_MAX_CLIENTS                       256
#define MC_DCT_DB_DEFAULT_MAX_ROWS                          256
#define MC_DCT_DB_DEFAULT_MAX_KEY_SIZE                      256
#define MC_DCT_DB_DEFAULT_MAX_VALUE_SIZE                    256
#define MC_DCT_DB_DEFAULT_MAX_TIME_PER_SWEEP                1.f
#define MC_DCT_DB_DEFAULT_MAX_SHMEM_TIMEOUT                 3.f

#define MC_STT_DB_DATABASE_CLOSED                           0x00000000
#define MC_STT_DB_DATABASE_OPENED                           0x00000001
#define MC_STT_DB_DATABASE_SHMEM_OPENED                     0x00000002


#define MC_OPT_DB_DATABASE_DEFAULT                          0x00000000
#define MC_OPT_DB_DATABASE_CREATE_IF_MISSING                0x00000001
#define MC_OPT_DB_DATABASE_TRANSACTIONAL                    0x00000002
#define MC_OPT_DB_DATABASE_READONLY                         0x00000004
#define MC_OPT_DB_DATABASE_SEEK_ON_READ                     0x00000010
#define MC_OPT_DB_DATABASE_DELAYED_OPEN                     0x00000020
#define MC_OPT_DB_DATABASE_NEXT_ON_READ                     0x00000040
#define MC_OPT_DB_DATABASE_SYNC_ON_COMMIT                   0x00000080
#define MC_OPT_DB_DATABASE_OPTION_MASK                      0x000FFFFF
#define MC_OPT_DB_DATABASE_LEVELDB                          0x00100000
#define MC_OPT_DB_DATABASE_FSR_UTXOC_BLOCKS                 0x00200000
#define MC_OPT_DB_DATABASE_REMOTE_SHMEM                     0x01000000
#define MC_OPT_DB_DATABASE_TYPE_MASK                        0x0FF00000

#define MC_OFF_DB_SHMEM_HEAD_STATUS                              0
#define MC_OFF_DB_SHMEM_HEAD_SIGNAL                              1
#define MC_OFF_DB_SHMEM_HEAD_CLIENT_COUNT                        2
#define MC_OFF_DB_SHMEM_HEAD_MAX_CLIENT_COUNT                    3
#define MC_OFF_DB_SHMEM_HEAD_REQUEST_PROCESS_ID                  4
#define MC_OFF_DB_SHMEM_HEAD_RESPONSE_PROCESS_ID                 5
#define MC_OFF_DB_SHMEM_HEAD_RESPONSE_SLOT_ID                    6
#define MC_OFF_DB_SHMEM_HEAD_KEY_SIZE                            8
#define MC_OFF_DB_SHMEM_HEAD_VALUE_SIZE                          9
#define MC_OFF_DB_SHMEM_HEAD_ROW_SIZE                           10
#define MC_OFF_DB_SHMEM_HEAD_SLOT_SIZE                          11
#define MC_OFF_DB_SHMEM_HEAD_MAX_ROWS                           12
#define MC_OFF_DB_SHMEM_HEAD_DATA_OFFSET                        16
#define MC_OFF_DB_SHMEM_HEAD_SLOT_DATA_OFFSET                   17
#define MC_OFF_DB_SHMEM_HEAD_ROW_DATA_OFFSET                    18
#define MC_OFF_DB_SHMEM_HEAD_FIELD_COUNT                        32

#define MC_OFF_DB_SHMEM_SLOT_PROCESS_ID                          0
#define MC_OFF_DB_SHMEM_SLOT_ROW_COUNT                           1
#define MC_OFF_DB_SHMEM_SLOT_RESULT                              2
#define MC_OFF_DB_SHMEM_SLOT_TIMESTAMP                           3
#define MC_OFF_DB_SHMEM_SLOT_FIXEDKEYSIZE                        4
#define MC_OFF_DB_SHMEM_SLOT_DATA                                8

#define MC_OFF_DB_SHMEM_ROW_REQUEST                              0
#define MC_OFF_DB_SHMEM_ROW_RESPONSE                             1
#define MC_OFF_DB_SHMEM_ROW_KEY_SIZE                             2
#define MC_OFF_DB_SHMEM_ROW_VALUE_SIZE                           3
#define MC_OFF_DB_SHMEM_ROW_DATA                                 4

#define MC_PRT_DB_SHMEM_SLOT_RESULT_UNDEFINED                       0
#define MC_PRT_DB_SHMEM_SLOT_RESULT_SUCCESS                         1
#define MC_PRT_DB_SHMEM_SLOT_RESULT_ERROR                          16

#define MC_PRT_DB_SHMEM_ROW_REQUEST_READ                            1
#define MC_PRT_DB_SHMEM_ROW_REQUEST_WRITE                           2
#define MC_PRT_DB_SHMEM_ROW_REQUEST_DELETE                          3
#define MC_PRT_DB_SHMEM_ROW_REQUEST_COMMIT                          4
#define MC_PRT_DB_SHMEM_ROW_REQUEST_READALL                         5
#define MC_PRT_DB_SHMEM_ROW_REQUEST_CLOSE                          16

#define MC_PRT_DB_SHMEM_ROW_RESPONSE_UNDEFINED                      0
#define MC_PRT_DB_SHMEM_ROW_RESPONSE_SUCCESS                        1
#define MC_PRT_DB_SHMEM_ROW_RESPONSE_NULL                           1
#define MC_PRT_DB_SHMEM_ROW_RESPONSE_NOT_NULL                       2
#define MC_PRT_DB_SHMEM_ROW_RESPONSE_ERROR                         16



#ifdef	__cplusplus
extern "C" {
#endif

typedef struct mc_Database
{
    mc_Database()
    {
         Zero();
    }

    ~mc_Database()
    {
         Destroy();
    }
    
    void *                  m_DB;
    void *                  m_OpenOptions;
    void *                  m_ReadOptions;
    void *                  m_IterOptions;
    void *                  m_WriteOptions;
    void *                  m_SyncOptions;
    void *                  m_Cache;
    void *                  m_WriteBatch;
    void *                  m_Iterator;
    
    uint32_t                m_ShMemKey;
    int                     m_MaxClients;
    int                     m_KeySize;
    int                     m_ValueSize;
    int                     m_MaxRows;
    void *                  m_Semaphore;
    int                     m_Signal;
    int                     m_LastError;
    uint64_t               *m_lpHandlerShMem;    
    
    void *                  m_hShMem;
    uint64_t               *m_lpShMem;    
    uint64_t               *m_lpShMemSlot;
    
    char                   *m_ReadBuffer;    
    int                     m_ReadBufferSize;
    int                     m_Status;                                           /* Database status - MC_STT_DB_DATABASE constants*/
    int                     m_Options;
    void *                  m_Log;
    int                     m_WriteCount;
    int                     m_DeleteCount;

    char                    m_Name[MC_DCT_DB_MAX_PATH];
    
    char                    m_LogFile[MC_DCT_DB_MAX_PATH];
    
    char                   *m_LogBuffer;    
    int                     m_LogSize;    
    
    int LogWrite(int op,
                      char  *key,                                               /* key */
                      int key_len,                                              /* key length, -1 if strlen is should be used */
                      char  *value,                                             /* value */
                      int value_len                                             /* value length, -1 if strlen is used */
    );    
    int LogFlush();
    
    int Destroy();
    int Zero();
    
    int SetOption(const char *option,int suboption,int value);
    
    int Open(                                                                   /* Open database */
        char   *name,                                                           /* Database name, path in case of leveldb */
        int  Options                                                            /* Open options, MC_OPT_DB_DATABASE constants */
    );
    int Close();                                                                /* Close databse */
    int Write(                                                                  /* Writes (key,value) to database */
        char  *key,                                                             /* key */
        int key_len,                                                            /* key length, -1 if strlen is should be used */
        char  *value,                                                           /* value */
        int value_len,                                                          /* value length, -1 if strlen is used */
        int Options                                                             /* Options - not used */
    );
    char *Read(                                                                 /* Reads value for specified key, no freeing required, pointer is valid until next read */
        char  *key,                                                             /* key */
        int key_len,                                                            /* key length, -1 if strlen is should be used */
        int *value_len,                                                         /* value length */
        int Options,                                                            /* Options - not used */
        int *error                                                              /* Error */
    );
    int BatchRead(
        char *Data,
        int *results,
        int count,
        int Options
    );
    int Delete(                                                                 /* Delete key from database */
        char  *key,                                                             /* key */
        int key_len,                                                            /* key length, -1 if strlen is should be used */
        int Options                                                             /* Options - not used */
    );
    int Commit(
        int Options                                                             /* Options - not used */
    );
    
    void Lock(int write_mode);
    void UnLock();
    int Synchronize();
    
    char *MoveNext(
        int *error
    );
    
    char *MoveNextKeyLevels(
        int key_levels,
        int *error
    );
    
} cs_Database;




#ifdef	__cplusplus
}
#endif





#endif	/* MULTICHAINDB_H */

