// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"

#ifndef WIN32

#ifndef _AIX
#include <pthread.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <semaphore.h>

#include <sys/shm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <syslog.h>

#include <glob.h>
#include <pwd.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
//#include "us_define.h"
#include <string.h>
#include <sys/stat.h>


#include <dlfcn.h>  // We need if of dlopen
#include <unistd.h> // We need it for sleep

#include <pwd.h>

#include "utils/define.h"
#include "utils/declare.h"

#define SHM_FAILED        (void *)-1L
#define WAIT_FAILED ((cs_int32)0xFFFFFFFF)
#define WAIT_OBJECT_0       0


void __US_Dummy()
{
    
}

void __US_Sleep (int dwMilliseconds)
{
    timespec ts;

    ts.tv_sec = (time_t)(dwMilliseconds/1000);
    ts.tv_nsec = (int) ((dwMilliseconds % 1000) * 1000000);

    nanosleep(&ts, NULL);
}

int __US_Fork(char *lpCommandLine,int WinState,int *lpChildPID)
{
    int res;
    int statloc;

    statloc=0;
    res=fork();
    if(res==0)
    {
        res=fork();
        if(res)
        {
            *lpChildPID=res;
            exit(0); //Child exits
        }
    }
    else
    {
        if(res>0)
        {
            while(*lpChildPID==0)
            {
                 __US_Sleep(1);
            }
            waitpid(res, &statloc, 0);
            res=*lpChildPID;
        }
    }

    return res;//Parent exits with PID of grandchild , GrandChild exits with 0
}
    
int __US_BecomeDaemon()
{
    int ChildPID;
    int res;
    int i,fd;

    res=__US_Fork(NULL,0,&ChildPID);

    if(res)
         exit(0);

    for (i = getdtablesize()-1; i > 2; --i)
          close(i);

    fd = open("/dev/tty", O_RDWR);
    ioctl(fd, TIOCNOTTY, 0);
    close (fd);

    return res;
}

void* __US_SemCreate()
{
    sem_t *lpsem;
    
    lpsem=NULL;
    
#ifdef MAC_OSX
    // Create a unique name for the named semaphore
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    int namelen = 20;
    char name[namelen + 1];
    for (int i = 0; i < namelen; ++i) {
        name[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    name[namelen] = 0;

    // Create named semaphore as unnamed semaphores are deprecated on OS X.
    lpsem = sem_open(name, O_CREAT | O_EXCL, 0666, 1);
    if (lpsem == SEM_FAILED) {
        return NULL;
    }
#else
    
    lpsem=new sem_t;
    if(sem_init(lpsem,0666,1))
    {
        delete lpsem;
        return NULL;
    }
#endif
    return (void*)lpsem;
}

void __US_SemWait(void* sem)
{
    if(sem)
    {
        sem_wait((sem_t *)sem);
    }
}

void __US_SemPost(void* sem)
{
    if(sem)
    {
        sem_post((sem_t*)sem);
    }
}

void __US_SemDestroy(void* sem)
{
#ifndef MAC_OSX
    sem_t *lpsem;
#endif    
    if(sem)
    {
        sem_close((sem_t*)sem);
#ifndef MAC_OSX
        lpsem=(sem_t*)sem;
        delete lpsem;
#endif    
    }
}

uint64_t __US_ThreadID()
{
    return (uint64_t)pthread_self();
}

const char* __US_UserHomeDir()
{
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) 
    {
        homedir = getpwuid(getuid())->pw_dir;
    }    
    
    return homedir;
}

char * __US_FullPath(const char* path, char *full_path, int len)
{
    full_path[0]=0x00;
    
    if(strlen(path) > 1)
    {
        if( (path[0] == '~') && (path[1] == '/') )
        {
            const char *homedir=__US_UserHomeDir();

            sprintf(full_path,"%s%s",homedir,path+1);
            
            return full_path;
        }
    }
    
    char *res= realpath(path, full_path); 
    if(res == NULL)
    {
        res=NULL;
    }
    return full_path;
}

void __US_FlushFile(int FileHan)
{
    fsync(FileHan);
}

void __US_FlushFileWithMode(int FileHan,uint32_t use_data_sync)
{
    if(use_data_sync)
    {
#ifdef MAC_OSX
        fcntl(FileHan, F_FULLFSYNC, 0);
#else
        fdatasync(FileHan);
#endif        
    }
    else
    {
        fsync(FileHan);
    }
}

int __US_LockFile(int FileHan)
{
    return flock(FileHan,LOCK_EX);
}

int __US_UnLockFile(int FileHan)
{
    return flock(FileHan,LOCK_UN);
}

int __US_DeleteFile(const char *file_name)
{
    return unlink(file_name);
}

int __US_GetPID()
{
    return getpid();
}

int __US_FindMacServerAddress(unsigned char **lppAddr,unsigned char *lpAddrToValidate)
{
#ifdef MAC_OSX
    return MC_ERR_NOT_SUPPORTED;
#else

    int nSD; // Socket descriptor
    struct ifreq sIfReq; // Interface request
    struct if_nameindex *pIfList; // Ptr to interface name index
    struct if_nameindex *pListSave; // Ptr to interface name index

    unsigned char *lpThisAddr;
    int AllocSize;
    
    int j,k,n,r,i,s;
    lpThisAddr=NULL;

    int AdapterCount;

    //
    // Initialize this function
    //
    pIfList = (struct if_nameindex *)NULL;
    pListSave = (struct if_nameindex *)NULL;
    #ifndef SIOCGIFADDR
    // The kernel does not support the required ioctls
    return MC_ERR_NOT_SUPPORTED;
    #endif
    //
    // Create a socket that we can use for all of our ioctls
    //
    nSD = socket( PF_INET, SOCK_STREAM, 0 );
    if ( nSD < 0 )
    {
    // Socket creation failed, this is a fatal error
        return MC_ERR_INTERNAL_ERROR;
    }
    //
    // Obtain a list of dynamically allocated structures
    //
    pIfList = pListSave = if_nameindex();
    //
    // Walk thru the array returned and query for each interface's
    // address
    //

    AdapterCount=0;
    for ( pIfList; *(char *)pIfList != 0; pIfList++ )
        AdapterCount++;
    
    if(AdapterCount>0)
    {
        AllocSize=6*AdapterCount+3;
        AllocSize=(((AllocSize-1)/16+1)*16)+16;
        
        lpThisAddr=new unsigned char[AllocSize];
        memset(lpThisAddr,1,AllocSize);
        
        *(lpThisAddr+0)=AdapterCount/256;
        *(lpThisAddr+1)=AdapterCount%256;
        
        if(lppAddr)
        {
            *lppAddr=lpThisAddr;
        }
        
        AdapterCount=0;
        pIfList = pListSave;
        for ( pIfList; *(char *)pIfList != 0; pIfList++ )
        {
            strncpy( sIfReq.ifr_name, pIfList->if_name, IF_NAMESIZE );
            if ( ioctl(nSD, SIOCGIFHWADDR, &sIfReq) != 0 )
            {
                // We failed to get the MAC address for the interface
                return MC_ERR_INTERNAL_ERROR;
         }
         memcpy(lpThisAddr+2+AdapterCount*6,(void *)&sIfReq.ifr_ifru.ifru_hwaddr.sa_data[0],6);
         lpThisAddr[2+AdapterCount*6+6]=0;
         AdapterCount++;
        }
    }
    //
    // Clean up things and return
    //
    if_freenameindex( pListSave );
    close( nSD );
    
    r=MC_ERR_NOERROR;
    if(lpAddrToValidate)
    {
        r=MC_ERR_NOT_FOUND;
        n=lpAddrToValidate[0]*256+lpAddrToValidate[1];
        for(k=0;k<AdapterCount;k++)
        if(r == MC_ERR_NOT_FOUND)
        {
            for(j=0;j<n;j++)
            {
                s=0;
                for(i=0;i<6;i++)
                {
                    s+=lpThisAddr[2+k*6+i];
                }
                if(s>0)
                {
                    if(memcmp(lpThisAddr+2+k*6,lpAddrToValidate+2+j*6,6)==0)
                    {
                         r=MC_ERR_NOERROR;     
                    }
                }
            }
        }
    }
    
    if(lppAddr==NULL)
    {
        delete [] lpThisAddr;
    }
    
    return r;
#endif        
 
}


#else

#include "windows.h"
#include "iphlpapi.h"

void __US_Dummy()
{
    
}

int __US_Fork(char *lpCommandLine,int WinState,int *lpChildPID)
{
    return 0;
}

int __US_BecomeDaemon()
{
    return 0;
}

void __US_Sleep (int dwMilliseconds)
{
    Sleep(dwMilliseconds);
}


void* __US_SemCreate()
{
    return (void*)CreateSemaphore(NULL,1,1,NULL);
}

void __US_SemWait(void* sem)
{
    if(sem)
    {
	WaitForSingleObject(sem, 0x7FFFFFFF);
    }
}

void __US_SemPost(void* sem)
{
    if(sem)
    {
        ReleaseSemaphore(sem,1,NULL);
    }
}

void __US_SemDestroy(void* sem)
{
    if(sem)
    {
	CloseHandle(sem);
    }
}

uint64_t __US_ThreadID()
{
    return (uint64_t)GetCurrentThreadId ();
}

const char* __US_UserHomeDir()
{
    return NULL;
}

char * __US_FullPath(const char* path, char *full_path, int len)
{
    return _fullpath(full_path,path,len);
}

void __US_FlushFile(int FileHan)
{
    HANDLE hFile = (HANDLE)_get_osfhandle(FileHan);
    FlushFileBuffers(hFile);
}

void __US_FlushFileWithMode(int FileHan,uint32_t use_data_sync)
{
    HANDLE hFile = (HANDLE)_get_osfhandle(FileHan);
    FlushFileBuffers(hFile);
}

int __US_LockFile(int FileHan)
{
    HANDLE hFile = (HANDLE)_get_osfhandle(FileHan);
    OVERLAPPED overlapvar = { 0 };

    if(LockFileEx(hFile, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                                0, MAXDWORD, MAXDWORD, &overlapvar))
    {
        return 0;
    }        
    return -1;
}

int __US_UnLockFile(int FileHan)
{
    HANDLE hFile = (HANDLE)_get_osfhandle(FileHan);
    OVERLAPPED overlapvar = { 0 };

    if(UnlockFileEx(hFile, 0, MAXDWORD, MAXDWORD, &overlapvar))
    {
        return 0;
    }        
    return -1;
}

int __US_DeleteFile(const char *file_name)
{
    return (int)DeleteFile(file_name);
}

int __US_GetPID()
{
    return (int)GetCurrentProcessId();
}

int __US_FindMacServerAddress(unsigned char **lppAddr,unsigned char *lpAddrToValidate)
{
    PIP_ADAPTER_INFO AdapterInfo;
    DWORD dwBufLen = sizeof(IP_ADAPTER_INFO);
    
    AdapterInfo = (IP_ADAPTER_INFO *) malloc(sizeof(IP_ADAPTER_INFO));
    if (AdapterInfo == NULL) 
    {
        return MC_ERR_INTERNAL_ERROR;
    }

    // Make an initial call to GetAdaptersInfo to get the necessary size into the dwBufLen variable
    if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) 
    {
        free(AdapterInfo);
        AdapterInfo = (IP_ADAPTER_INFO *) malloc(dwBufLen);
        if(AdapterInfo == NULL) 
        {
            return MC_ERR_INTERNAL_ERROR;
        }
    }    
    
    unsigned char *lpThisAddr;
    int AllocSize;
    
    int j,k,n,r,i,s;
    lpThisAddr=NULL;

    int AdapterCount=0;
    
    if (GetAdaptersInfo(AdapterInfo, &dwBufLen) == NO_ERROR) 
    {
        PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
        while(pAdapterInfo)
        {
            AdapterCount++;
            pAdapterInfo = pAdapterInfo->Next;
        }
    }
    
    if(AdapterCount>0)
    {
        AllocSize=6*AdapterCount+3;
        AllocSize=(((AllocSize-1)/16+1)*16)+16;
        
        lpThisAddr=new unsigned char[AllocSize];
        memset(lpThisAddr,1,AllocSize);
        
        *(lpThisAddr+0)=AdapterCount/256;
        *(lpThisAddr+1)=AdapterCount%256;
        
        if(lppAddr)
        {
            *lppAddr=lpThisAddr;
        }
        
        AdapterCount=0;
        
        
        PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;
        while(pAdapterInfo)
        {
            memcpy(lpThisAddr+2+AdapterCount*6,pAdapterInfo->Address,6);
            lpThisAddr[2+AdapterCount*6+6]=0;
            AdapterCount++;
            pAdapterInfo = pAdapterInfo->Next;
        }
    }
    
    free(AdapterInfo);
    
    r=MC_ERR_NOERROR;
    if(lpAddrToValidate)
    {
        r=MC_ERR_NOT_FOUND;
        n=lpAddrToValidate[0]*256+lpAddrToValidate[1];
        for(k=0;k<AdapterCount;k++)
        if(r == MC_ERR_NOT_FOUND)
        {
            for(j=0;j<n;j++)
            {
                s=0;
                for(i=0;i<6;i++)
                {
                    s+=lpThisAddr[2+k*6+i];
                }
                if(s>0)
                {
                    if(memcmp(lpThisAddr+2+k*6,lpAddrToValidate+2+j*6,6)==0)
                    {
                         r=MC_ERR_NOERROR;     
                    }
                }
            }
        }
    }
    
    if(lppAddr==NULL)
    {
        delete [] lpThisAddr;
    }
    
    return r;
}

#endif
