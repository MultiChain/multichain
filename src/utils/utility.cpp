// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#ifndef WIN32
#include <termios.h>
#include <sys/ioctl.h>
#include <ctype.h>
#endif

#define MC_DCT_BUF_ALLOC_ITEMS          256
#define MC_DCT_LIST_ALLOC_MIN_SIZE      32768
#define MC_DCT_LIST_ALLOC_MAX_SIZE      268435456


int c_IsHexNumeric[256]={
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 1,2,3,4,5,6,7,8,9,10,0,0,0,0,0,0,
 0,11,12,13,14,15,16,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,11,12,13,14,15,16,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int mc_AllocSize(int items,int chunk_size,int item_size)
{
    if(items<=0)
    {
        return 0;
    }
    return ((items-1)/chunk_size+1)*chunk_size*item_size;
}

void *mc_New(int Size)
{   
    int TSize;
    int64_t *ptr;
    
    TSize=(Size-1)/sizeof(int64_t)+1;
    ptr=new int64_t[(size_t)TSize];
    
    if(ptr)
    {
        memset(ptr,0,(size_t)Size);
    }

    return ptr;
}


void mc_Delete(void *ptr)
{
    delete [] (int64_t*)ptr;
}

void mc_PutLE(void *dest,void *src,int dest_size)
{
    memcpy(dest,src,dest_size);                                                 // Assuming all systems are little endian
}

int64_t mc_GetLE(void *src,int size)
{
    int64_t result;
    unsigned char *ptr;
    unsigned char *ptrEnd;
    int shift;
    
    ptr=(unsigned char*)src;
    ptrEnd=ptr+size;
    
    result=0;
    shift=0;
    while(ptr<ptrEnd)
    {
        result|=((int64_t)(*ptr))<<shift;
        shift+=8;
        ptr++;
    }
    
    return result;                                                              // Assuming all systems are little endian
}

uint32_t mc_SwapBytes32(uint32_t src)
{
    uint32_t res=0;
    unsigned char *pr;
    unsigned char *ps;
    pr=(unsigned char*)&res;
    ps=(unsigned char*)&src;
    for(int i=0;i<4;i++)
    {
        pr[3-i]=ps[i];        
    }
    return res;
}

void mc_print(const char *message)
{
    printf("%s\n",message);
}

int mc_HexToBin(void *dest,const void *src,int len)
{
    int i,s;
    unsigned char c;
    unsigned char *ptrIn;
    unsigned char *ptrOut;
    
    ptrIn=(unsigned char*)src;
    ptrOut=(unsigned char*)dest;
    
    for(i=0;i<len;i++)
    {
        s=c_IsHexNumeric[*ptrIn++];
        if(s==0)return i;
        c=(unsigned char)(s-1)*16;
        s=c_IsHexNumeric[*ptrIn++];
        if(s==0)return i;
        c+=(unsigned char)(s-1);
        *ptrOut++=c;
    }
    
    return len;
}

int mc_BinToHex(void *dest,const void *src,int len)
{
    int i;
    unsigned char *ptrIn;
    unsigned char *ptrOut;

    ptrIn=(unsigned char*)src;
    ptrOut=(unsigned char*)dest;
    
    for(i=0;i<len;i++)
    {
        sprintf((char*)ptrOut,"%02x",*ptrIn++);
        ptrOut+=2;
    }    
    return len;
}


void mc_MemoryDump(void *ptr,                                                   // Pointer to the memory to dump
                int from,                                                       // Position to dump from
                int len)                                                        // Size
{
    int *dptr;
    int i,j,n,k;
    
    dptr=(int *)ptr+from;
    
    k=from;
    n=(len-1)/4+1;
    
    for(i=0;i<n;i++)
    {
#ifdef MAC_OSX
        printf("%4d %08X: ",k,(unsigned int)(size_t)dptr);
#else        
        printf("%4d %08X: ",k,(unsigned int)(unsigned int64_t)dptr);
#endif        
        for(j=0;j<4;j++)
        {
            if(k<len)
            {
                printf("%08X  ",*dptr);            
            }
            dptr++;
            k++;
        }        
        printf("\n");            
    }        
}

void mc_MemoryDumpCharSizeToFile(
                FILE *fHan,
                const void *ptr, 
                int from,         
                int len,
                int row_size)          
{
    unsigned char *dptr;
    int i,j,n,k;
    
    dptr=(unsigned char *)ptr+from;
    
    k=from;
    n=(len-1)/row_size+1;
    
    for(i=0;i<n;i++)
    {
        fprintf(fHan,"%4x:",k);
        for(j=0;j<row_size;j++)
        {
            if((j%4) == 0)
            {
                fprintf(fHan," ");
            }
            if(k<len)
            {
                fprintf(fHan,"%02x",*dptr);            
            }
            dptr++;
            k++;
        }       
        fprintf(fHan,"\n");            
    }        
}


void mc_MemoryDumpCharSize(const void *ptr, 
                int from,         
                int len,
                int row_size)          
{
    unsigned char *dptr;
    int i,j,n,k;
    
    dptr=(unsigned char *)ptr+from;
    
    k=from;
    n=(len-1)/row_size+1;
    
    for(i=0;i<n;i++)
    {
        printf("%4x:",k);
        for(j=0;j<row_size;j++)
        {
            if((j%4) == 0)
            {
                printf(" ");
            }
            if(k<len)
            {
                printf("%02x",*dptr);            
            }
            dptr++;
            k++;
        }        
        printf("  ");
        dptr-=row_size;
        k-=row_size;
        for(j=0;j<row_size;j++)
        {
            if(k<len)
            {
                if((*dptr>=0x20) && (*dptr<=0x7F))
                {
                    printf("%c",*dptr);                                
                }
                else
                {
                    printf(".");                                                    
                }
            }
            dptr++;
            k++;
        }        
        printf("\n");            
    }        
}

void mc_MemoryDumpChar(const void *ptr, 
                int from,         
                int len)
{
    mc_MemoryDumpCharSize(ptr,from,len,16);
}


void mc_DumpSize(const char * message,const void *ptr,int size,int row_size)
{
    printf("%s\n",message);
    mc_MemoryDumpCharSize(ptr,0,size,row_size);
    printf("\n");
}

void mc_Dump(const char * message,const void *ptr,int size)
{
    mc_DumpSize(message,ptr,size,16);
}

void mc_RandomSeed(unsigned int seed)
{
    srand(seed);
}

double mc_RandomDouble()
{
    return (double)rand()/RAND_MAX;
}

unsigned int mc_RandomInRange(unsigned int min,unsigned int max)
{
    double scaled = (double)rand()/RAND_MAX;
    unsigned int result;
    
    result=(unsigned int)((max-min+1)*scaled) + min;
    if(result>max)
    {
        result=max;
    }
    return result;
}

unsigned int mc_TimeNowAsUInt()
{
    struct timeval time_now;
    
    gettimeofday(&time_now,NULL);
    
    return time_now.tv_sec;
}

double mc_TimeNowAsDouble()
{
    struct timeval time_now;
    
    gettimeofday(&time_now,NULL);
    
    return (double)(time_now.tv_sec)+0.000001f*((double)(time_now.tv_usec));
}


void mc_Buffer::Zero()
{
    m_lpData=NULL;   
    m_lpIndex=NULL;
    m_AllocSize=0;
    m_Size=0;
    m_KeySize=0;
    m_RowSize=0;
    m_Count=0;
    m_Mode=0;    
}

int mc_Buffer::Destroy()
{
    if(m_lpIndex)
    {
        delete m_lpIndex;
    }
    if(m_lpData)
    {
        mc_Delete(m_lpData);
    }
    
    Zero();
    
    return MC_ERR_NOERROR;
}

int mc_Buffer::Initialize(int KeySize,int RowSize,uint32_t Mode)
{
    int err;
    
    err=MC_ERR_NOERROR;
    
    Destroy();
    
    m_Mode=Mode;
    m_KeySize=KeySize;
    m_RowSize=RowSize;
    
    if(m_Mode & MC_BUF_MODE_MAP)
    {
        m_lpIndex=new mc_MapStringIndex;
    }
        
    
    m_AllocSize=mc_AllocSize(1,MC_DCT_BUF_ALLOC_ITEMS,m_RowSize);
    
    m_lpData=(unsigned char*)mc_New(m_AllocSize);
    if(m_lpData==NULL)
    {
        Zero();
        err=MC_ERR_ALLOCATION;
        return err;
    }
    
    return err;
}
    
int mc_Buffer::Clear()
{
    m_Size=0;
    m_Count=0;
    
    if(m_lpIndex)
    {
        m_lpIndex->Clear();
    }
    
    
    return MC_ERR_NOERROR;
}

int mc_Buffer::Realloc(int Rows)
{
    unsigned char *lpNewBuffer;
    int NewSize;
    int err;
    
    err=MC_ERR_NOERROR;
    
    if(m_Size+m_RowSize*Rows>m_AllocSize)
    {
        NewSize=mc_AllocSize(m_Count+Rows,MC_DCT_BUF_ALLOC_ITEMS,m_RowSize);
        lpNewBuffer=(unsigned char*)mc_New(NewSize);
        
        if(lpNewBuffer==NULL)
        {
            err=MC_ERR_ALLOCATION;
            return err;
        }

        memcpy(lpNewBuffer,m_lpData,m_AllocSize);
        mc_Delete(m_lpData);

        m_AllocSize=NewSize;
        m_lpData=lpNewBuffer;                
    }
    
    return err;
}

int mc_Buffer::Add(const void *lpKey,const void *lpValue)
{
    int err;
    
    err=Realloc(1);
    if(err)
    {
        return err;
    }   
    
    if(m_KeySize)
    {
        memcpy(m_lpData+m_Size,lpKey,m_KeySize);
    }
    
    if(m_KeySize<m_RowSize)
    {
        if(lpValue)
        {
            if( (m_lpData+m_Size+m_KeySize) != lpValue)
            {
                memcpy(m_lpData+m_Size+m_KeySize,lpValue,m_RowSize-m_KeySize);
            }
        }
        else
        {
            memset(m_lpData+m_Size+m_KeySize,0,m_RowSize-m_KeySize);
        }
    }
        
    m_Size+=m_RowSize;
    
    if(m_lpIndex)
    {
        m_lpIndex->Add((unsigned char*)lpKey,m_KeySize,m_Count);
    }
    
    m_Count++;
    
    return err;
}

int mc_Buffer::Add(const void *lpKeyValue)
{
    return Add(lpKeyValue,(unsigned char*)lpKeyValue+m_KeySize);
}

int mc_Buffer::UpdateRow(int RowID,const void *lpKey,const void *lpValue)
{
    if(RowID>=m_Count)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    if(m_lpIndex)
    {
        m_lpIndex->Remove((char*)GetRow(RowID),m_RowSize-m_KeySize);
    }
    
    return PutRow(RowID,lpKey,lpValue);
}

int mc_Buffer::PutRow(int RowID,const void *lpKey,const void *lpValue)
{
    unsigned char *ptr;
    
    if(RowID>=m_Count)
    {
        return MC_ERR_INTERNAL_ERROR;
    }
    
    ptr=m_lpData+m_RowSize*RowID;
    
    if(m_KeySize)
    {
        memcpy(ptr,lpKey,m_KeySize);
    }
    
    if(m_KeySize<m_RowSize)
    {
        memcpy(ptr+m_KeySize,lpValue,m_RowSize-m_KeySize);
    }
    
    if(m_lpIndex)
    {
        if(m_lpIndex->Get((unsigned char*)lpKey,m_KeySize) >= 0)
        {
            m_lpIndex->Set((unsigned char*)lpKey,m_KeySize,RowID);            
        }
        else
        {
            m_lpIndex->Add((unsigned char*)lpKey,m_KeySize,RowID);
        }
    }
    
    return MC_ERR_NOERROR;
}


int mc_Buffer::Seek(void *lpKey)
{
    unsigned char *ptr;
    int row;
    
    if(m_lpIndex)
    {
        row=m_lpIndex->Get((unsigned char*)lpKey,m_KeySize);
        if(row >= 0)
        {
            ptr=GetRow(row);
            if(memcmp(ptr,lpKey,m_KeySize)==0)
            {
                return row;
            }
        }
        return -1;
    }
    
    ptr=m_lpData;
    row=0;
    
    while(row<m_Count)
    {
        if(memcmp(ptr,lpKey,m_KeySize)==0)
        {
            return row;
        }
        ptr+=m_RowSize;
        row++;
    }
    
    return -1;
}


unsigned char *mc_Buffer::GetRow(int RowID)
{
    return m_lpData+m_RowSize*RowID;
}

int mc_Buffer::GetCount()
{
    return m_Count;
}

int mc_Buffer::SetCount(int count)
{    
    int i;
    int err=MC_ERR_NOERROR;
    
    if(m_Count>count)
    {
        if(m_lpIndex)
        {
            m_lpIndex->Clear();
            m_Count=0;
            m_Size=0;
            for(i=0;i<count;i++)
            {
                m_Size+=m_RowSize;
                m_lpIndex->Add((unsigned char*)GetRow(i),m_KeySize,m_Count);
                m_Count++;
//                Add(GetRow(i),GetRow(i)+m_KeySize);
            }
        }
        else
        {
            m_Count=count;    
        }
    }

    if(m_Count<count)
    {
        err=Realloc(count-m_Count);
    }
    
    m_Count=count;
    m_Size=count*m_RowSize;
    
    return err;
}

void mc_Buffer::CopyFrom(mc_Buffer *source)
{
    Clear();
    int i;
    unsigned char *ptr;
    
    for(i=0;i<source->GetCount();i++)    
    {
        ptr=source->GetRow(i);
        Add(ptr,ptr+m_KeySize);
    }
}

int mc_Buffer::Sort()                                                           
{
    if(m_lpIndex)
    {
        return MC_ERR_NOT_SUPPORTED;
    }

    if(m_Count <= 1)
    {
        return MC_ERR_NOERROR;
    }
    
    int i,j,t; 
    int err;

    err=Realloc(1);
    if(err)
    {
        return err;
    }   
    
    t=m_AllocSize/m_RowSize-1;
    
    for(i=0;i<m_Count-1;i++)                                                    // This is never used, so we can consider entire function as stub
    {
        for(j=i;j>=0;j--)
        {
            if(memcmp(GetRow(j),GetRow(j+1),m_RowSize) > 0)
            {
                memcpy(GetRow(t),GetRow(j+1),m_RowSize);
                memcpy(GetRow(j+1),GetRow(j),m_RowSize);
                memcpy(GetRow(j),GetRow(t),m_RowSize);
            }
        }
    }
    
    return MC_ERR_NOERROR;
}




void mc_List::Zero()
{
    m_lpData=NULL;   
    m_AllocSize=0;
    m_Size=0;
    m_Pos=0;    
    m_ItemSize=0;
}


int mc_List::Destroy()
{
    if(m_lpData)
    {
        mc_Delete(m_lpData);
    }
    
    Zero();
    
    return MC_ERR_NOERROR;
}

void mc_List::Clear()
{
    m_Size=0;
    m_Pos=0;
    m_ItemSize=0;
}

int mc_List::Put(unsigned char *ptr, int size)
{
    unsigned char *NewBuffer;
    int NewSize;
    int true_size;
    
    true_size=size;
    if(size<0)        
    {
        true_size=0;
    }
    if(ptr == NULL)
    {
        true_size=0;
    }
    
    while(m_Size+true_size+(int)sizeof(int)>m_AllocSize)
    {
        if(m_AllocSize>0)
        {
            NewSize=m_AllocSize*2;    
            if(NewSize> MC_DCT_LIST_ALLOC_MAX_SIZE)
            {
                return MC_ERR_ALLOCATION;
            }
        }
        else
        {
            NewSize=MC_DCT_LIST_ALLOC_MIN_SIZE;
        }
                
        NewBuffer=(unsigned char*)mc_New(NewSize);
        if(NewBuffer == NULL)
        {
            return MC_ERR_ALLOCATION;
        }
        else
        {
            if(m_lpData)
            {
                if(m_Size)
                {
                    memcpy(NewBuffer,m_lpData,m_Size);
                }
                mc_Delete(m_lpData);
            }
            m_lpData=NULL;                    
        }
        
        m_AllocSize=NewSize;
        m_lpData=NewBuffer;        
    }

    *(int *)(m_lpData+m_Size)=true_size;
    m_Size+=sizeof(int);
    if(true_size)
    {
        memcpy(m_lpData+m_Size,ptr,true_size);
    }
    m_Size+=true_size;
    
    return MC_ERR_NOERROR;    
}

unsigned char *mc_List::First()
{
    m_Pos=0;
    return Next();
}

unsigned char *mc_List::Next()
{
    unsigned char *ptr;
    if(m_Pos>=m_Size)
    {
        m_ItemSize=0;
        return NULL;
    }
    
    m_ItemSize=*(int *)(m_lpData+m_Pos);
    m_Pos+=sizeof(int);
    ptr=m_lpData+m_Pos;
    
    m_Pos+=m_ItemSize;
    return ptr;
}



int mc_VarIntSize(unsigned char byte)
{
    if(byte<0xfd)return 0;
    if(byte==0xfd)return 2;
    if(byte==0xfe)return 4;
    return  8;
}

int64_t mc_GetVarInt(const unsigned char *buf,int max_size,int64_t default_value,int* shift)
{
    int size;
    if(max_size<=0)
    {
        return default_value;
    }
    
    size=mc_VarIntSize(buf[0]);
    
    if(max_size < size+1)
    {
        return default_value;
    }
    
    if(shift)
    {
        *shift=size+1;
    }
    
    if(size == 0)
    {
        return buf[0]; 
    }
    
    return mc_GetLE((void*)(buf+1),size);
}

int mc_PutVarInt(unsigned char *buf,int max_size,int64_t value)
{
    int varint_size,shift;
    
    if(max_size<=0)
    {
        return -1;
    }
    
    varint_size=1;
    shift=0;
    if(value>=0xfd)
    {
        shift=1;
        if(value>=0xffff)
        {
            if(value>=0xffffffff)
            {
                buf[0]=0xff;
                varint_size=8;
            }        
            else
            {
                buf[0]=0xfe;
                varint_size=4;
            }
        }        
        else
        {
            buf[0]=0xfd;
            varint_size=2;            
        }
    }

    if(max_size < shift+varint_size)
    {
        return -1;
    }
    
    mc_PutLE(buf+shift,&value,varint_size);    
    return shift+varint_size;
}

#ifndef WIN32

static struct termios oldtc , newtc;

/* Initialize new terminal i/o settings */
void initTermios(int echo)
{
  tcgetattr(0, &oldtc); /* grab old terminal i/o settings */
  
  newtc = oldtc; /* make new settings same as old settings */
  
  newtc.c_lflag &= ~ICANON; /* disable buffered i/o */
  newtc.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
  
  tcsetattr(0, TCSANOW, &newtc); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void)
{
  tcsetattr(0, TCSANOW, &oldtc);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo)
{
  char ch;
//  initTermios(echo);
  ch = getchar();
//  resetTermios();
  return ch;
}

/* Read 1 character without echo */
char getch(void)
{
  return getch_(0);
}

int mc_TerminalInput::LoadDataFromLog(const char* fileName)
{
    int fHan;
    char *raw;
    
    int64_t offset,size;    
    int err;
    int start,pos;
    
    strcpy(m_Line,"");
    fHan=open(fileName,_O_BINARY | O_RDONLY);
    if(fHan<0)
    {
        return MC_ERR_FILE_READ_ERROR;
    }
    raw=(char*)mc_New(MC_DCT_TERM_BUFFER_SIZE);
    err=MC_ERR_NOERROR;
    
    size=lseek64(fHan,0,SEEK_END);
    offset=size-m_BufferSize;
    if(offset<0)
    {
        offset=0;
    }
    
    if(lseek64(fHan,offset,SEEK_SET) != offset)
    {
        err=MC_ERR_FILE_READ_ERROR;
        goto exitlbl;
    }
    
    if(size>m_BufferSize)
    {
        size=m_BufferSize;
    }
       
    if(read(fHan,raw,size) != size)
    {
        err=MC_ERR_FILE_READ_ERROR;
        goto exitlbl;        
    }
    
    pos=0;
    if(size==m_BufferSize)
    {
        while( (pos<size) && (raw[pos] !='\n'))
        {
            pos++;
        }
        pos++;
    }
    
    start=pos+24;
    while(start<size)
    {
        pos=start;
        while( (pos<size) && (raw[pos] !='\n'))
        {
            pos++;
        }
        if( (pos<size) && (pos != start) )
        {
            raw[pos]=0x00;
            if(strcmp(m_Line,raw+start))
            {
                strcpy(m_Line,raw+start);
                AddLine();
                m_ThisLine++;            
            }
        }
        start=pos+1+24;
    }
            
exitlbl:

    if(raw)
    {
        mc_Delete(raw);
    }
    close(fHan);
    return err;
}

int mc_TerminalInput::IsAlphaNumeric(char c)
{
    if( (c >= 0x30) && (c <= 0x39) )
    {
        return 1;
    }
    if( (c >= 0x41) && (c <= 0x5a) )
    {
        return 1;
    }
    if( (c >= 0x61) && (c <= 0x7a) )
    {
        return 1;
    }
    return 0;
}

int mc_TerminalInput::SetPrompt(const char* prompt)
{
    strcpy(m_Prompt,prompt);
    return MC_ERR_NOERROR;
}

int mc_TerminalInput::Prompt()
{
    printf("%s: ",m_Prompt);
    fflush(stdout);
    return MC_ERR_NOERROR;
}

int mc_TerminalInput::TerminalCols()
{
    struct winsize max;
    ioctl(0, TIOCGWINSZ , &max);
    m_TerminalCols=max.ws_col;
    m_TerminalRows=max.ws_row;
    return m_TerminalCols;
}

void mc_TerminalInput::MoveBack(int offset)
{
    int p,q,i;
    p=(offset+strlen(m_Prompt)+2)%m_TerminalCols;
    if(p==0)
    {
        q=offset-m_TerminalCols;
        printf("%c[A",0x1b);
        if(q<0)
        {
            while(q<0)
            {
                printf(" ");
                q++;
            }
            q+=strlen(m_Prompt)+2;
            for(i=0;i<q;i++)
            {
                printf("\b");
            }                    
            Prompt();
            q=0;
        }
        for(i=q;i<offset-1;i++)
        {
            printf("%c",m_Line[i]);
        }                    
    }
    else
    {
        printf("\b");
    }
}

void mc_TerminalInput::AddLine()
{
    int dataend,pos,len;
    
    len=strlen(m_Line)+1;
    dataend=0;
    if(m_ThisLine>m_FirstLine)
    {
        pos=(m_ThisLine-1)%m_HistoryLines;
        dataend=m_Offsets[pos]+strlen(m_Data+m_Offsets[pos])+1;
    }
    if(dataend+len>m_BufferSize)
    {
        dataend=0;
    }
    
    while( (m_FirstLine<m_ThisLine) &&
           ((m_FirstLine+m_HistoryLines<=m_ThisLine) || 
           ((m_Offsets[m_FirstLine%m_HistoryLines]>=dataend) && (m_Offsets[m_FirstLine%m_HistoryLines]<dataend+len)) ) )
    {
        m_FirstLine++;
    }
    
    pos=m_ThisLine%m_HistoryLines;
    strcpy(m_Data+dataend,m_Line);
    m_Offsets[pos]=dataend;
}

void mc_TerminalInput::SaveLine()
{
    if(m_LoadedLine==m_ThisLine)
    {
        strcpy(m_Cache,m_Line);        
    }    
}

int mc_TerminalInput::LoadLine(int line)
{
    int len;
    if(line<m_FirstLine)
    {
        return -1;
    }
    if(line>m_ThisLine)
    {
        return -1;
    }
    len=strlen(m_Line);
    if(line==m_ThisLine)
    {
        strcpy(m_Line,m_Cache);        
    }
    else
    {
        strcpy(m_Line,m_Data+m_Offsets[line%m_HistoryLines]);
    }
    m_LoadedLine=line;
    return len;
}


char *mc_TerminalInput::GetLine()
{
    char c,l;
    int offset,arrow_mode,arrow_prefix,len,i,cols,pos,oldcol,oldrow,totrow;
    initTermios(0);
    memset(m_Line,0,m_BufferSize);
    offset=0;
    len=0;
    arrow_mode=0;
    arrow_prefix=0;
    m_LoadedLine=m_ThisLine;
    cols=TerminalCols();
    
    c=getch();
    while(c != '\n')
    {        
        if(TerminalCols() != cols)
        {            
            oldcol=(offset+strlen(m_Prompt)+2-1) % cols + 1;
            oldrow=(offset+strlen(m_Prompt)+2-1) / cols;
            totrow=(len+strlen(m_Prompt)+2-1) / cols;
            for(i=0;i<oldcol;i++)
            {
                printf("\b");
            }
            for(i=0;i<oldrow;i++)
            {
                printf("%c[A",0x1b);
            }
            for(i=0;i<totrow*m_TerminalCols;i++)
            {
                printf(" ");
            }
            for(i=0;i<m_TerminalCols;i++)
            {
                printf(" ");
            }
            for(i=0;i<m_TerminalCols;i++)
            {
                printf("\b");
            }
            for(i=0;i<totrow;i++)
            {
                printf("%c[A",0x1b);
            }
            Prompt();
            for(i=0;i<len;i++)
            {
                printf("%c",m_Line[i]);
            }                                
            for(i=len;i>offset;i--)
            {
                MoveBack(i);
            }
            fflush(stdout);            
            cols=TerminalCols();
        }
        
        switch(arrow_mode)
        {
            case 0:
                if(((c >= 0x20) && (c<=0x7e)) || (c==0x09) )
                {
                    if(c == 0x09)
                    {
                        c=0x20;
                    }
                    if(offset<m_BufferSize-1)
                    {
                        for(i=offset;i<=len;i++)
                        {
                            l=m_Line[i];
                            m_Line[i]=c;
                            printf("%c",c);
                            c=l;
                        }
                        offset++;
                        len++;
                        for(i=len;i>offset;i--)
                        {
                            MoveBack(i);
                        }
                        fflush(stdout);
                        c=0;    
                    }                    
                }
                if(c == 0x7f)
                {
                    if(offset)
                    {
                        MoveBack(offset);
                        for(i=offset;i<len;i++)
                        {
                            m_Line[i-1]=m_Line[i];
                            printf("%c",m_Line[i]);
                        }
                        printf(" ");
                        for(i=len;i>offset;i--)
                        {
                            MoveBack(i);
                        }
                        MoveBack(offset);
                        offset--;
                        len--;
                        fflush(stdout);
                        c=0;
                    }
                }
                if(c == 0x01)
                {
                    if(offset>0)
                    {
                        for(i=offset;i>0;i--)
                        {
                            MoveBack(i);
                        }
                        offset=0;
                        c=0;
                    }
                }
                if(c == 0x05)
                {
                    if(offset < len)
                    {
                        for(i=offset;i<len;i++)
                        {
                            printf("%c",m_Line[i]);
                            c=l;
                        }
                        offset=len;
                        c=0;
                    }
                }
                if(c == 0x04)
                {
                    strcpy(m_Line,"bye");
                    resetTermios();
                    return m_Line;
                }
                
                if(c == 0x15)
                {
                    if(len<m_BufferSize)
                    {
                        m_Line[len]=0;
                    }
                    for(i=offset;i>0;i--)
                    {
                        MoveBack(i);
                    }
                    for(i=0;i<len;i++)
                    {
                        m_Line[i]=' ';
                        printf(" ");
                    }                                
                    for(i=len;i>0;i--)
                    {
                        MoveBack(i);
                    }
                    len=0;
                    offset=len;
                    c=0;
                }
                
                if(c == 0x1b)
                {
                    arrow_mode=1;
                    c=0;
                }
                
                
                break;
            case 1:
                arrow_mode=2;
                arrow_prefix=c;
                c=0;
                break;
            case 2:
                if( arrow_prefix == 0x4f )
                {
                    switch(c)
                    {
                        case 0x48:
                            if(offset > 0)
                            {
                                for(i=offset;i>0;i--)
                                {
                                    MoveBack(i);
                                }
                                offset=0;
                                c=0;
                            }
                            break;
                        case 0x46:
                            if(offset < len)
                            {
                                for(i=offset;i<len;i++)
                                {
                                    printf("%c",m_Line[i]);
                                }
                                offset=len;
                                c=0;
                            }
                            break;
                    }                    
                }
                if( arrow_prefix == 0x5b )
                {
                    switch(c)
                    {
                        case 0x31:                                              // Ctrl
                            arrow_mode=4;
                            arrow_prefix=0x31;
                            c=0;
                            break;
                        case 0x41:                                              // Arrow Up                            
                            if(len<m_BufferSize)
                            {
                                m_Line[len]=0;
                            }
                            if( (m_LoadedLine-1>=m_FirstLine) && (m_LoadedLine-1<=m_ThisLine) )
                            {
                                SaveLine();
                                for(i=offset;i>0;i--)
                                {
                                    MoveBack(i);
                                }
                                for(i=0;i<len;i++)
                                {
                                    m_Line[i]=' ';
                                    printf(" ");
                                }                                
                                for(i=len;i>0;i--)
                                {
                                    MoveBack(i);
                                }
                                LoadLine(m_LoadedLine-1); 
                                len=strlen(m_Line);
                                offset=len;
                                for(i=0;i<len;i++)
                                {
                                    printf("%c",m_Line[i]);
                                }                                             
                                fflush(stdout);
                                c=0;                                                    
                            }
                            arrow_mode=0;
                            break;
                        case 0x42:                                              // Arrow Down
                            if(len<m_BufferSize)
                            {
                                m_Line[len]=0;
                            }
                            if( (m_LoadedLine+1>=m_FirstLine) && (m_LoadedLine+1<=m_ThisLine) )
                            {
                                SaveLine();
                                for(i=offset;i>0;i--)
                                {
                                    MoveBack(i);
                                }
                                for(i=0;i<len;i++)
                                {
                                    m_Line[i]=' ';
                                    printf(" ");
                                }                                                                
                                for(i=len;i>0;i--)
                                {
                                    MoveBack(i);
                                }
                                LoadLine(m_LoadedLine+1); 
                                len=strlen(m_Line);
                                offset=len;
                                for(i=0;i<len;i++)
                                {
                                    printf("%c",m_Line[i]);
                                }                                             
                                fflush(stdout);
                                c=0;                                                    
                            }
                            arrow_mode=0;
                            break; 
                        case 0x43:                                              // Arrow Right
                            if(offset<len)
                            {
                                printf("%c",m_Line[offset]);
                                offset++;
                                c=0;                    
                            }
                            arrow_mode=0;
                            break;
                        case 0x44:                                              // Arrow Left
                            if(offset)
                            {
                                MoveBack(offset);
                                offset--;
//                                printf("\b");
                                c=0;                    
                            }
                            arrow_mode=0;
                            break;
                        case 0x32:                                              // Insert, Delete, PageUp, PageDown
                        case 0x33:
                        case 0x35:
                        case 0x36:
                            arrow_mode=3;
                            c=0;
                            break;                        
                        default:
                            arrow_mode=0;
                            break;
                    }
                }
                else
                {
                    arrow_mode=0;                    
                }
                arrow_prefix=0;
                break;
            case 3:
                arrow_mode=0;
                break;
            case 4:
                if(c == 0x3b)
                {
                    arrow_mode=5;
                    c=0;
                }
                else
                {
                    arrow_mode=0;                    
                }
                break;
            case 5:
                if(c == 0x35)
                {
                    arrow_mode=6;
                    c=0;
                }
                else
                {
                    arrow_mode=0;                    
                }
                break;
            case 6:
                if(c == 0x43)                                                   // Ctrl Arrow Right
                {
                    pos=offset;
                    while( (pos<len) && (IsAlphaNumeric(m_Line[pos]) == 0) )
                    {
                        pos++;
                    }
                    while( (pos<len) && (IsAlphaNumeric(m_Line[pos]) > 0) )
                    {
                        pos++;
                    }
                    if(pos != offset)
                    {
                        for(i=offset;i<pos;i++)
                        {
                            printf("%c",m_Line[i]);
                        }
                        fflush(stdout);
                        offset=pos;
                        c=0;                        
                    }
                }
                if(c == 0x44)                                                   // Ctrl Arrow Left
                {
                    pos=offset-1;
                    while( (pos>=0) && (IsAlphaNumeric(m_Line[pos]) == 0) )
                    {
                        pos--;
                    }
                    while( (pos>=0) && (IsAlphaNumeric(m_Line[pos]) > 0) )
                    {
                        pos--;
                    }
                    pos++;
                    if(pos <= (offset-1))
                    {
                        for(i=offset;i>pos;i--)
                        {
                            MoveBack(i);
                        }
                        fflush(stdout);
                        offset=pos;
                        c=0;                        
                    }
                }
                arrow_mode=0; 
                break;
        }
        if(c)
        {
            printf("%c",0x07);
            fflush(stdout);            
        }
        c=getch();
    }
    
    m_Line[len]=0;
    
    if(len)
    {
        if((m_FirstLine>=m_ThisLine) || (strcmp(m_Data+m_Offsets[(m_ThisLine-1)%m_HistoryLines],m_Line) != 0))
        {
            if((strcmp(m_Line,"exit")==0) || 
               (strcmp(m_Line,"quit")==0) || 
               (strcmp(m_Line,"bye")==0))
            {
                ;
            }
            else
            {
                AddLine();
                m_ThisLine++;
            }
        }
    }

    printf("%c[H",0x1b);
    for(i=0;i<m_TerminalRows;i++)
    {
        printf("%c[B",0x1b);
    }                         

    fflush(stdout);
    resetTermios();
    return m_Line;
}

char escape_convt(char c)
{
    char ch = c;

    switch (ch) 
    {
        case 'a': ch = '\a'; break;
        case 'b': ch = '\b'; break;
        case 't': ch = '\t'; break;
        case 'n': ch = '\n'; break;
        case 'f': ch = '\f'; break;
        case 'r': ch = '\r'; break;
    }

    return ch;
}

int mc_StringToArg(char *src,char *dest)
{
    int take_it;
    int mode=0;
    int quotes=0;
    char *str;
    char *d;
    
    str=src;
    d=dest;
    while(isspace((unsigned char)*str))
    {
        str++;
    }

    mode=0;
    take_it=1;
    while(take_it)
    {
        switch(*str)
        {
            case 0x00:
                str++;
                take_it=0;
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\f':
            case '\r':
            case '\b':                  
                if(mode==0)
                {
                    str++;
                    take_it=0;
                }
                else
                {
                    *d=*str;
                    d++;
                    str++;                    
                }
                break;
            case '\\':
                if(mode != 2)
                {
                    str++;
                    if(*str)
                    {
                        switch(mode)
                        {
                            case 0:
                                *d=*str;
                                d++;
                                str++;                    
                                break;                            
                            case 1:
                                *d=escape_convt(*str);
                                d++;
                                str++;                    
                                break;
                        }
                    }
                    else
                    {
                        *d='\\';
                        d++;
                        str++;
                        take_it=0;                    
                    }
                }
                else
                {
                    *d=*str;
                    d++;
                    str++;                                        
                }
                break;
            case '"':
                switch(mode)
                {
                    case 0:
                        quotes++;
                        mode=1;
                        str++;                    
                        break;                        
                    case 1:
                        quotes++;
                        mode=0;
                        str++;                    
                        break;
                    case 2:
                        *d=*str;
                        d++;
                        str++;                    
                        break;
                }
                break;
            case '\'':
                switch(mode)
                {
                    case 0:
                        quotes++;
                        mode=2;
                        str++;                    
                        break;                        
                    case 1:
                        *d=*str;
                        d++;
                        str++;                    
                        break;
                    case 2:
                        quotes++;
                        mode=0;
                        str++;                    
                        break;
                }
                break;
            default:
                *d=*str;
                d++;
                str++;
                break;
        }
    }
    
    if(mode)
    {
        return -1;
    }
    
    if(d == dest)
    {
        if(quotes == 0)
        {
            return 0;            
        }
    }
    
    *d=0x00;
    
    return str-src; 
}
#endif

int mc_SaveCliCommandToLog(const char *fileName, int argc, char* argv[])
{
    int a,c,i,p;
    
    p=0;    
    for(a=1;a<argc;a++)
    {
        if(argv[a][0] != '-')
        {
            if(p == 1)
            {
                if((strcmp(argv[a],"encryptwallet") == 0) ||
                   (strcmp(argv[a],"walletpassphrase") == 0) ||    
                   (strcmp(argv[a],"walletpassphrasechange") == 0) || 
                   (strcmp(argv[a],"signrawtransaction") == 0) || 
                   (strcmp(argv[a],"importprivkey") == 0))
                {
                    return 0;
                }
            }
            p++;
        }
    }
    
#ifndef WIN32    
    FILE *fHan;
    fHan=fopen(fileName,"a");

    struct timeval time_now;
    struct tm *bdt;
    
    gettimeofday(&time_now,NULL);    
    
    bdt=localtime(&(time_now.tv_sec));
    fprintf(fHan,"%04d-%02d-%02d %02d:%02d:%02d.%03d\t",1900+bdt->tm_year,
                                                        bdt->tm_mon+1,
                                                        bdt->tm_mday,
                                                        bdt->tm_hour,
                                                        bdt->tm_min,
                                                        bdt->tm_sec,
                                                        (int32_t)(time_now.tv_usec/1000));
    
    
    p=0;    
    for(a=1;a<argc;a++)
    {
        c=0;
        for(i=0;i<(int)strlen(argv[a]);i++)
        {
            switch(argv[a][i])
            {
                case '\'':
                case '\"':
                case '\\':
                case ' ':
                case '\t':
                    c++;
                    break;
                    
            }
        }
        if(argv[a][0] != '-')
        {
            if(p > 0)
            {
                if(c)
                {
                    fprintf(fHan,"%c",'\'');
                }
                for(i=0;i<(int)strlen(argv[a]);i++)
                {
                    if(argv[a][i] == '\'')
                    {
                        fprintf(fHan,"%c%c%c%c",'\'','\\','\'','\'');
                    }
                    else
                    {
                        fprintf(fHan,"%c",argv[a][i]);                
                    }
                }
                if(c)
                {
                    fprintf(fHan,"%c",'\'');
                }
                if(a<argc-1)
                {
                    fprintf(fHan," ");
                }
            }
            p++;
        }        
    }
    
    fprintf(fHan,"\n");
    fclose(fHan);
#endif    
    return 0;
}

void mc_StringLowerCase(char *buf,uint32_t len)
{
    unsigned char *ptr;
    unsigned char *ptrEnd;
    
    ptr=(unsigned char *)buf;
    ptrEnd=ptr+len;
    
    while(ptr<ptrEnd)
    {
        if(*ptr>0x40&&*ptr<0x5b)
        {
            *ptr=*ptr+0x20;
        }
        ptr++;
    }
}

int mc_StringCompareCaseInsensitive(const char *str1,const char *str2, int len) 
{
    int i,res;

    res=0;

    for(i=0;i<len;i++)
    if(res==0)
    {
        if(str1[i]!=str2[i])
        {
            res=1;
            if(str1[i]>0x60&&str1[i]<0x7b)
            if(str1[i]-0x20==str2[i])
            res=0;
            if(res)
            {
                if(str2[i]>0x60&&str2[i]<0x7b)
                if(str2[i]-0x20==str1[i])
                res=0;
            }
        }
    }

    return res;
}

void mc_LogString(FILE *fHan, const char* message)
{
    struct tm *bdt;
    
#ifndef WIN32
    struct timeval time_now;
    gettimeofday(&time_now,NULL);    
    bdt=localtime(&(time_now.tv_sec));
    fprintf(fHan,"%04d-%02d-%02d %02d:%02d:%02d.%03d\t%s\n",1900+bdt->tm_year,
                                                        bdt->tm_mon+1,
                                                        bdt->tm_mday,
                                                        bdt->tm_hour,
                                                        bdt->tm_min,
                                                        bdt->tm_sec,
                                                        (int32_t)(time_now.tv_usec/1000),
                                                        message);
#else
    time_t dt;
    struct tm dc;
    time(&dt);
    dc=*localtime(&dt);
    
    fprintf(fHan,"%04d-%02d-%02d %02d:%02d:%02d\t%s\n",1900+dc.tm_year,
                                                        dc.tm_mon+1,
                                                        dc.tm_mday,
                                                        dc.tm_hour,
                                                        dc.tm_min,
                                                        dc.tm_sec,
                                                        message);
#endif    
    
}

void mc_AdjustStartAndCount(int *count,int *start,int size)
{
    if(*count>size)
    {
        *count=size;
    }
    if(*start<0)
    {
        *start+=size;
        if(*start<0)
        {
            *start=0;
        }
    }

    if(*start > 0)
    {
        if(*start+*count>size)
        {
            *count=size-*start;
        }
    }    
}

void sprintf_hex(char *hex,const unsigned char *bin,int size)
{
    int i;
    for(i=0;i<size;i++)
    {
        sprintf(hex+(i*2),"%02x",bin[size-1-i]);
    }
    
    hex[size*2]=0;      
}

void mc_SwapBytes(void *vptr,uint32_t size)
{
    unsigned char *ptr=(unsigned char *)vptr;
    unsigned char t;
    for(uint32_t i=0;i<size/2;i++)
    {
        t=ptr[i];
        ptr[i]=ptr[size-i-1];
        ptr[size-i-1]=t;
    }
}