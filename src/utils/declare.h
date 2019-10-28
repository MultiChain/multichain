// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_DECLARE_H
#define	MULTICHAIN_DECLARE_H

#include "utils/define.h"

#define MC_DCT_TERM_BUFFER_SIZE         25165824

/* Classes */


#ifdef	__cplusplus
extern "C" {
#endif


typedef struct mc_MapStringIndex
{    
    void *mapObject;
    mc_MapStringIndex()
    {
        mapObject=NULL;
        Init();
    }    

    ~mc_MapStringIndex()
    {
        Destroy();
    }        
    void Init();
    void Add(const char* key,int value);
    void Add(const unsigned char* key,int size,int value);
    void Remove(const char* key,int size);
    int Get(const char* key);
    int Get(const unsigned char* key,int size);
    void Set(const unsigned char* key,int size,int value);
    void Destroy();
    void Clear();
} mc_MapStringIndex;

typedef struct mc_MapStringString
{    
    void *mapObject;
    mc_MapStringString()
    {
        Init();
    }    

    ~mc_MapStringString()
    {
        Destroy();
    }        
    void Init();
    void Add(const char* key,const char* value);
    const char * Get(const char* key);
    void Destroy();
    int GetCount();
} mc_MapStringString;

typedef struct mc_Buffer
{
    mc_Buffer()
    {
        Zero();
    }

    ~mc_Buffer()
    {
        Destroy();
    }

    mc_MapStringIndex      *m_lpIndex;
    unsigned char          *m_lpData;   
    int                     m_AllocSize;
    int                     m_Size;
    int                     m_KeySize;
    int                     m_RowSize;
    int                     m_Count;
    uint32_t                m_Mode;
    
    void Zero();
    int Destroy();
    int Initialize(int KeySize,int TotalSize,uint32_t Mode);
    
    int Clear();
    int Realloc(int Rows);
    int Add(const void *lpKey,const void *lpValue);
    int Add(const void *lpKeyValue);
    int Seek(void *lpKey);
    unsigned char *GetRow(int RowID);    
    int PutRow(int RowID,const void *lpKey,const void *lpValue);    
    int UpdateRow(int RowID,const void *lpKey,const void *lpValue);    
    int GetCount();    
    int SetCount(int count);
    
    int Sort();    
    
    void CopyFrom(mc_Buffer *source);
    
} mc_Buffer;

    
typedef struct mc_List
{
    mc_List()
    {
         Zero();
    }

    ~mc_List()
    {
         Destroy();
    }

    unsigned char          *m_lpData;   
    int                     m_AllocSize;
    int                     m_Size;
    int                     m_Pos;
    int                     m_ItemSize;
    
    void Zero();
    int Destroy();
    
    void Clear();
    int Put(unsigned char *ptr, int size);
    unsigned char *First();
    unsigned char *Next();
    
} mc_List;


typedef struct mc_SHA256
{
    void *m_HashObject;
    void Init();
    void Destroy();
    void Reset();
    void Write(const void *lpData,int size);
    void GetHash(unsigned char *hash);
    
    void DoubleHash(const void *lpData,int size,void *hash);
    void DoubleHash(const void *lpSalt,int salt_size,const void *lpData,int size,void *hash);
    
    mc_SHA256()
    {
        Init();
    }
    
    ~mc_SHA256()
    {
        Destroy();
    }
} mc_SHA256;


struct mc_TerminalInput
{
    char m_Prompt[64];
    char m_Data[MC_DCT_TERM_BUFFER_SIZE];
    char m_Line[MC_DCT_TERM_BUFFER_SIZE];
    char m_Cache[MC_DCT_TERM_BUFFER_SIZE];
    int m_Offsets[100];
    
    int m_HistoryLines;
    int m_BufferSize;
    int m_ThisLine;
    int m_FirstLine;
    int m_LoadedLine;
    int m_TerminalCols;
    int m_TerminalRows;
    
    char *GetLine();
    int SetPrompt(const char *prompt);
    int Prompt();
    void AddLine();
    int LoadLine(int line);
    void SaveLine();
    void MoveBack(int offset);
    int TerminalCols();
    int IsAlphaNumeric(char c);
    int LoadDataFromLog(const char *fileName);
    
    mc_TerminalInput()
    {
        int i;
        strcpy(m_Prompt,"");
        m_HistoryLines=100;
        m_BufferSize=MC_DCT_TERM_BUFFER_SIZE;
        m_ThisLine=0;
        m_FirstLine=0;
        m_LoadedLine=0;
        memset(m_Line,0,m_BufferSize);
        memset(m_Data,0,m_BufferSize);
        for(i=0;i<m_HistoryLines;i++)
        {
            m_Offsets[i]=-1;
        }
        m_TerminalCols=0;
    }
};


/* Functions */
    
int mc_AllocSize(int items,int chunk_size,int item_size);
void *mc_New(int Size);
void mc_Delete(void *ptr);
void mc_PutLE(void *dest,void *src,int dest_size);
int64_t mc_GetLE(void *src,int size);
uint32_t mc_SwapBytes32(uint32_t src);
int mc_BackupFile(const char *network_name,const char *filename, const char *extension,int options);
int mc_RecoverFile(const char *network_name,const char *filename, const char *extension,int options);
FILE *mc_OpenFile(const char *network_name,const char *filename, const char *extension,const char *mode, int options);        
void mc_CloseFile(FILE *fHan);
int mc_RemoveFile(const char *network_name,const char *filename, const char *extension,int options);
size_t mc_ReadFileToBuffer(FILE *fHan,char **lpptr);
void mc_print(const char *message);
int mc_ReadGeneralConfigFile(mc_MapStringString *mapConfig,const char *network_name,const char *file_name,const char *extension);
int mc_ReadParamArgs(mc_MapStringString *mapConfig,int argc, char* argv[],const char *prefix);
int mc_HexToBin(void *dest,const void *src,int len);
int mc_BinToHex(void *dest,const void *src,int len);
void mc_MemoryDump(const void *ptr,int from,int len);
void mc_MemoryDumpChar(const void *ptr,int from,int len);
void mc_Dump(const char * message,const void *ptr,int size);
void mc_MemoryDumpCharSize(const void *ptr,int from,int len,int row_size);
void mc_MemoryDumpCharSizeToFile(FILE *fHan,const void *ptr, int from, int len,int row_size);          
void mc_DumpSize(const char * message,const void *ptr,int size,int row_size);
void mc_RandomSeed(unsigned int seed);
double mc_RandomDouble();
unsigned int mc_RandomInRange(unsigned int min,unsigned int max);
unsigned int mc_TimeNowAsUInt();
double mc_TimeNowAsDouble();
int mc_GetFullFileName(const char *network_name,const char *filename, const char *extension,int options,char *buf);
int64_t mc_GetVarInt(const unsigned char *buf,int max_size,int64_t default_value,int* shift);
int mc_PutVarInt(unsigned char *buf,int max_size,int64_t value);
int mc_BuildDescription(int build, char *desc);
void mc_SwapBytes(void *vptr,uint32_t size);

void mc_GetCompoundHash160(void *result,const void  *hash1,const void  *hash2);
int mc_SetIPv4ServerAddress(const char* host);
int mc_FindIPv4ServerAddress(uint32_t *all_ips,int max_ips);
int mc_GenerateConfFiles(const char *network_name);
void mc_CreateDir(const char *dir_name);
void mc_RemoveDataDir(const char *network_name);
void mc_RemoveDir(const char *network_name,const char *dir_name);
int mc_GetDataDirArg(char *buf);
void mc_UnsetDataDirArg();
void mc_SetDataDirArg(char *buf);
void mc_ExpandDataDirParam();
void mc_CheckDataDirInConfFile();
void mc_AdjustStartAndCount(int *count,int *start,int size);
void* custom_get_blockchain_default(const char *param,int* size,void *param_in);


int mc_TestScenario(char* scenario_file);

int mc_StringToArg(char *src,char *dest);
int mc_SaveCliCommandToLog(const char *fileName, int argc, char* argv[]);

void mc_StringLowerCase(char *buf,uint32_t len);
int mc_StringCompareCaseInsensitive(const char *str1,const char *str2,int len);

uint32_t mc_GetParamFromDetailsScript(const unsigned char *ptr,uint32_t total,uint32_t offset,uint32_t* param_value_start,size_t *bytes);
uint32_t mc_GetParamFromDetailsScriptErr(const unsigned char *ptr,uint32_t total,uint32_t offset,uint32_t* param_value_start,size_t *bytes, int *err);
uint32_t mc_FindSpecialParamInDetailsScript(const unsigned char *ptr,uint32_t total,uint32_t param,size_t *bytes);
uint32_t mc_FindSpecialParamInDetailsScriptFull(const unsigned char *ptr,uint32_t total,uint32_t param,size_t *bytes,uint32_t *param_offset);
uint32_t mc_FindNamedParamInDetailsScript(const unsigned char *ptr,uint32_t total,const char *param,size_t *bytes);

const unsigned char *mc_ParseOpDropOpReturnScript(const unsigned char *src,int size,int *op_drop_offset,int *op_drop_size,int op_drop_count,int *op_return_offset,int *op_return_size);
const unsigned char *mc_ExtractAddressFromInputScript(const unsigned char *src,int size,int *op_addr_offset,int *op_addr_size,int* is_redeem_script,int* sighash_type,int check_last);

void mc_LogString(FILE *fHan, const char* message);

const char *mc_Version();
const char *mc_FullVersion();
void __US_Sleep (int dwMilliseconds);
int __US_Fork(char *lpCommandLine,int WinState,int *lpChildPID);
int __US_BecomeDaemon();
void __US_Dummy();
void* __US_SemCreate();
void __US_SemWait(void* sem);
void __US_SemPost(void* sem);
void __US_SemDestroy(void* sem);
uint64_t __US_ThreadID();
const char* __US_UserHomeDir();
char * __US_FullPath(const char* path, char *full_path, int len);
void __US_FlushFile(int FileHan);
void __US_FlushFileWithMode(int FileHan,uint32_t use_data_sync);
int __US_LockFile(int FileHan);
int __US_UnLockFile(int FileHan);
int __US_DeleteFile(const char *file_name);
int __US_GetPID();
int __US_FindMacServerAddress(unsigned char **lppAddr,unsigned char *lpAddrToValidate);
void sprintf_hex(char *hex,const unsigned char *bin,int size);



#ifdef	__cplusplus
}
#endif


#endif	/* MULTICHAIN_DECLARE_H */

