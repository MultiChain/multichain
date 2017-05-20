// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"
#include "crypto/sha256.h"
#include "structs/base58.h"

#ifndef WIN32

#include <sys/ioctl.h>

#else

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <io.h> /* for _commit */
#include <shlobj.h>

#endif


#include "chainparams/chainparams.h"
#include "utils/util.h"
#include "utils/utilstrencodings.h"
#include "structs/hash.h"
#include "core/main.h"
#include "net/net.h"

#define MC_DCT_SEED_NODE_MAX_SIZE 32

//#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
//#include <boost/algorithm/string/join.hpp>
//#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
//#include <boost/foreach.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>
//#include <boost/thread.hpp>
//#include <openssl/crypto.h>
//#include <openssl/rand.h>
/*
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/program_options/detail/config_file.hpp>
*/
using namespace std;

const boost::filesystem::path mc_GetDataDir(const char *network_name,int create);

void mc_Params::Parse(int argc, const char* const argv[])
{
    int i,length;
    const char* exe_name;
    ParseParameters(argc,argv);
    mc_ExpandDataDirParam();
    
    m_NumArguments=0;
    length=MC_DCT_SEED_NODE_MAX_SIZE+1;
    for (i = 1; i < argc; i++)
    {
        if(argv[i][0] != '-')
        {
            m_NumArguments++;
            length+=strlen(argv[i])+1;
        }
    }
    
    if(m_NumArguments)
    {
        m_Arguments=(char**)mc_New((m_NumArguments+1)*sizeof(char*));
        m_Arguments[0]=(char*)mc_New(length);
    }
    
    m_NumArguments=0;
    length=0;
    for (i = 1; i < argc; i++)
    {
        if(argv[i][0] != '-')
        {
            m_Arguments[m_NumArguments]=m_Arguments[0]+length;
            strcpy(m_Arguments[m_NumArguments],argv[i]);
            m_NumArguments++;            
            length+=strlen(argv[i])+1;
        }
    }

    
    
    if(m_NumArguments)
    {
        exe_name=argv[0];
        for(i=0;i<(int)strlen(argv[0]);i++)
        {
            if((argv[0][i]=='/') || (argv[0][i]=='\\'))
            {
                exe_name=argv[0]+i+1;
            }
        }
        
        if((strcmp(exe_name,"multichain-util") == 0) || (strcmp(exe_name,"multichain-util.exe") == 0))
        {
            m_FirstArgumentType=MC_FAT_COMMAND;
        }
        else
        {
            if((strcmp(exe_name,"multichain-cli") == 0) || 
               (strcmp(exe_name,"multichain-cli.exe") == 0) ||                     
               (strcmp(exe_name,"multichaind") == 0) ||                     
               (strcmp(exe_name,"multichaind.exe") == 0))
            {
                m_FirstArgumentType=MC_FAT_NETWORK;                
                for(i=0;i<(int)strlen(m_Arguments[0]);i++)
                {
                    if(m_FirstArgumentType == MC_FAT_NETWORK)
                    {
                        if(m_Arguments[0][i] == '@')
                        {
                            m_FirstArgumentType=MC_FAT_NETWORKSEED;
                            m_Arguments[0][i]=0x00;
                        }
                    }
                }
                if((m_FirstArgumentType == MC_FAT_NETWORK) && ((strcmp(exe_name,"multichaind") == 0) || (strcmp(exe_name,"multichaind.exe") == 0)))
                {
                    m_Arguments[m_NumArguments]=m_Arguments[0]+length;
                    m_Arguments[m_NumArguments][0]=0x00;
                    length++;

                    mc_MapStringString *mapConfig;
                    int err;
                    const char *seed_node;

                    mapConfig=new mc_MapStringString;

                    err=mc_ReadGeneralConfigFile(mapConfig,mc_gState->m_Params->NetworkName(),"seed",".dat");

                    if(err == MC_ERR_NOERROR)
                    {
                        seed_node=mapConfig->Get("seed");

                        if(seed_node)
                        {
                            if(strlen(seed_node) <= MC_DCT_SEED_NODE_MAX_SIZE)
                            {
                                strcpy(m_Arguments[m_NumArguments],seed_node);
                                length+=strlen(seed_node);
                            }
                        }
                    }

                    delete mapConfig;                            
                    m_NumArguments++;                                                                
                }                
            }            
        }
    }
        
}

const char *mc_Params::NetworkName()
{
    if((m_FirstArgumentType == MC_FAT_NETWORK) || (m_FirstArgumentType == MC_FAT_NETWORKSEED))
    {
        return m_Arguments[0];
    }
    return NULL;
}

const char *mc_Params::SeedNode()
{
    const char *seed_node;
    if(m_FirstArgumentType == MC_FAT_NETWORKSEED)
    {
        return m_Arguments[0]+strlen(m_Arguments[0])+1;
    }
    if(m_FirstArgumentType == MC_FAT_NETWORK)
    {
        seed_node=m_Arguments[m_NumArguments-1];
        if(*seed_node)
        {
            return seed_node;
        }
    }
    return NULL;
}

const char* mc_State::GetSeedNode()
{
    const char *seed_node;
    seed_node=mc_gState->m_Params->SeedNode();
    if(seed_node == NULL)
    {
/*        
        int seed_node_size;
        seed_node=(char*)mc_gState->m_NetworkParams->GetParam("seednode",&seed_node_size);
        if(seed_node_size <= 1)
        {
            seed_node=NULL;
        }
 */ 
    }
    
    return seed_node;
}


const char *mc_Params::Command()
{
    if(m_FirstArgumentType == MC_FAT_COMMAND)
    {
        return m_Arguments[0];
    }
    return NULL;
}

const char *mc_Params::DataDir()
{
    return  DataDir(1,1);
}

const char *mc_Params::DataDir(int network_specific,int create)
{
    const char *name=NULL;
    if(network_specific)
    {
        name=NetworkName();
    }
    
    boost::filesystem::path path=mc_GetDataDir(name,create);
    
    if(network_specific)
    {
        strcpy(m_DataDirNetSpecific,path.string().c_str());
        return m_DataDirNetSpecific;
    }
    
    strcpy(m_DataDir,path.string().c_str());
    return m_DataDir;
}

const char* mc_Params::GetOption(const char* strArg, const char* strDefault)
{
    return GetArg(string(strArg),string(strDefault)).c_str();
}

int64_t mc_Params::GetOption(const char* strArg, int64_t nDefault)
{
    return GetArg(string(strArg),nDefault);
}

int64_t mc_Params::HasOption(const char* strArg)
{
    return mapArgs.count(string(strArg));
}


boost::filesystem::path mc_GetDefaultDataDir()
{
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\MultiChain
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\MultiChain
    // Mac and Unix: ~/.multichain
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "MultiChain";
#else
    // Mac and Unix
    boost::filesystem::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = boost::filesystem::path("/");
    else
        pathRet = boost::filesystem::path(pszHome);
    return pathRet / ".multichain";
#endif
}

int mc_GetDataDirArg(char *buf)
{
    if (mapArgs.count("-datadir"))
    {
        strcpy(buf,mapArgs["-datadir"].c_str());
        return 1;
    }
    return 0;
}

void mc_UnsetDataDirArg()
{
    if (mapArgs.count("-datadir"))
    {
        mapArgs.erase("-datadir");
    }
}

void mc_SetDataDirArg(char *buf)
{
    mapArgs["-datadir"] = string(buf);
}


void mc_ExpandDataDirParam()
{
    if (mapArgs.count("-datadir"))
    {
        string original=mapArgs["-datadir"];
        if(original.size() > 1)
        {
            if( (*(original.c_str()) == '~') && (*(original.c_str() + 1) == '/') )
            {
                const char *homedir=__US_UserHomeDir();

                if(homedir)
                {
                    mapArgs["-datadir"]=strprintf("%s%s",homedir,original.c_str()+1);                    
                }
            }
        }
    }    
}

void mc_CheckDataDirInConfFile()
{
    if (mapArgs.count("-datadir"))
    {
        return;
    }    
    
    mc_MapStringString *mapConfig;
    
    mapConfig=new mc_MapStringString;
    if(mc_ReadGeneralConfigFile(mapConfig,NULL,"multichain",".conf") == 0)
    {
        if(mapConfig->Get("datadir") != NULL)
        {
            mapArgs["-datadir"]=strprintf("%s",mapConfig->Get("datadir"));            
            mc_ExpandDataDirParam();
        }
    }    
}


const boost::filesystem::path mc_GetDataDir(const char *network_name,int create)
{
    boost::filesystem::path path;
    if (mapArgs.count("-datadir")) {
        path = boost::filesystem::system_complete(mapArgs["-datadir"]);
        if (!boost::filesystem::is_directory(path)) 
        {
            return path;
        }
    } 
    else 
    {
        path = mc_GetDefaultDataDir();
    }
    if(network_name)
    {
        path /= std::string(network_name);
    }
    if(create)
    {
        boost::filesystem::create_directories(path);
    }
    return path;
}

void mc_RemoveDataDir(const char *network_name)
{
    boost::filesystem::path path;
    if (mapArgs.count("-datadir")) {
        path = boost::filesystem::system_complete(mapArgs["-datadir"]);
    } 
    else 
    {
        path = mc_GetDefaultDataDir();
    }
    if(network_name)
    {
        path /= std::string(network_name);
    }
    
    boost::filesystem::remove_all(path);
}

void mc_RemoveDir(const char *network_name,const char *dir_name)
{
    boost::filesystem::path path;
    if (mapArgs.count("-datadir")) {
        path = boost::filesystem::system_complete(mapArgs["-datadir"]);
    } 
    else 
    {
        path = mc_GetDefaultDataDir();
    }
    if(network_name)
    {
        path /= std::string(network_name);
    }
    if(dir_name)
    {
        path /= std::string(dir_name);
    }
    
    boost::filesystem::remove_all(path);
}

string mc_GetFullFileName(const char *network_name,const char *filename, const char *extension,int options)
{
    int create;
    std::string fullName = filename;
    std::string backupName;
    fullName += extension;
    boost::filesystem::path pathFile;
    
    create=0;
    if(options & MC_FOM_CREATE_DIR)
    {
        create=1;
    }
    switch(options & MC_FOM_RELATIVE_MASK)
    {
        case MC_FOM_NONE:
            pathFile=fullName;
            break;
        case MC_FOM_RELATIVE_TO_DATADIR:
            pathFile = mc_GetDataDir(network_name,create) / fullName;
            break;
    }
            
    return pathFile.string();    
}

int mc_GetFullFileName(const char *network_name,const char *filename, const char *extension,int options,char *buf)
{
    strcpy(buf,mc_GetFullFileName(network_name,filename,extension,options).c_str());
    return MC_ERR_NOERROR;
}

int mc_BackupFile(const char *network_name,const char *filename, const char *extension,int options)
{
    std::string fullName = mc_GetFullFileName(network_name,filename,extension,options);
    std::string backupName;
    
    backupName=fullName + ".bak";
    
    if(rename(fullName.c_str(),backupName.c_str()))
    {
        return MC_ERR_FILE_WRITE_ERROR;
    }
    
    return MC_ERR_NOERROR;
}

int mc_RecoverFile(const char *network_name,const char *filename, const char *extension,int options)
{
    std::string fullName = mc_GetFullFileName(network_name,filename,extension,options);
    std::string backupName;
    
    backupName=fullName + ".bak";
    
    if(rename(backupName.c_str(),fullName.c_str()))
    {
        return MC_ERR_FILE_WRITE_ERROR;
    }
    
    return MC_ERR_NOERROR;
}

FILE *mc_OpenFile(const char *network_name,const char *filename, const char *extension,const char *mode, int options)        
{    
    return fopen(mc_GetFullFileName(network_name,filename,extension,options).c_str(), mode); 
}

int mc_RemoveFile(const char *network_name,const char *filename, const char *extension,int options)        
{    
    return unlink(mc_GetFullFileName(network_name,filename,extension,options).c_str()); 
}


void mc_CloseFile(FILE *fHan)
{
    if(fHan)
    {
        fclose(fHan);
    }
}

size_t mc_ReadFileToBuffer(FILE *fHan,char **lpptr)
{
    size_t size;
    *lpptr=NULL;
    
    if(fHan==NULL)
    {
        return -MC_ERR_INTERNAL_ERROR;
    }
    
    fseek(fHan, 0L, SEEK_END);
    size = ftell(fHan);
    fseek(fHan, 0L, SEEK_SET);
    
    if(size<=0)
    {
        return size;
    }
    
    *lpptr=(char*)mc_New(size);
    if(*lpptr)
    {
        if(fread(*lpptr,size,1,fHan) != size)
        {
            mc_Delete(*lpptr);
            *lpptr=NULL;
            return -MC_ERR_FILE_READ_ERROR;
        }        
    }    
    else
    {
        return -MC_ERR_ALLOCATION;
    }
    
    return size;
}

boost::filesystem::path mc_GetConfigFile(const char *network_name,const char *file_name,const char *extension)
{
    string fileName="multichain";
    if(file_name)
    {
        fileName = file_name;
    }
    if(extension)
    {
        fileName += extension;
    }
    
    boost::filesystem::path pathConfigFile(GetArg("-conf", fileName));
    if (!pathConfigFile.is_complete())
        pathConfigFile = mc_GetDataDir(network_name,0) / pathConfigFile;
    return pathConfigFile;
}

static void mc_InterpretNegativeSetting(string name, map<string, string>& mapSettingsRet)
{
    // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
    if (name.find("-no") == 0)
    {
        std::string positive("-");
        positive.append(name.begin()+3, name.end());
        if (mapSettingsRet.count(positive) == 0)
        {
            bool value = !GetBoolArg(name, false);
            mapSettingsRet[positive] = (value ? "1" : "0");
        }
    }
}

int mc_ReadParamArgs(mc_MapStringString *mapConfig,
                    int argc, char* argv[],
                    const char *prefix)
{
    int p;
    char *ptr;
    map<string, string>* mapSettingsRet=(std::map<string, string>*)mapConfig->mapObject;
    for(int argi=1;argi<argc;argi++)
    {
        // Don't overwrite existing settings so command line settings override bitcoin.conf

        if(argv[argi][0]== '-')
        {
            p=0;
            ptr=argv[argi]+1;
            
            while((p == 0) && *ptr)
            {
                if(*ptr == '=')
                {
                    p=ptr-argv[argi];
                }
                ptr++;
            }
            
            if(p > 1)
            {            
                string strKey = argv[argi]+1;
                strKey=strKey.substr(0,p-1);
                
                strKey.erase(std::remove(strKey.begin(), strKey.end(), '-'), strKey.end());
                strKey.erase(std::remove(strKey.begin(), strKey.end(), '_'), strKey.end());

                strKey = string(prefix) + strKey;
                if ((*mapSettingsRet).count(strKey) == 0)
                {
                    (*mapSettingsRet)[strKey] = argv[argi]+p+1;
                    // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set)
                    mc_InterpretNegativeSetting(strKey, *mapSettingsRet);
                }
            }
        }
    }
    
    return MC_ERR_NOERROR;
}


int mc_ReadConfigFile(
                    boost::filesystem::path fileConfig,
                    map<string, string>* mapSettingsRet,
                    map<string, vector<string> >* mapMultiSettingsRet,
                    const char *prefix)
{
    try
    {
        boost::filesystem::ifstream streamConfig(fileConfig);
        if (!streamConfig.good())
            return MC_ERR_NOERROR; // No bitcoin.conf file is OK

        set<string> setOptions;
        setOptions.insert("*");

//        boost::program_options::detail::config_file_iterator it(streamConfig, setOptions), end;
//        while(it != end)
        for (boost::program_options::detail::config_file_iterator it(streamConfig, setOptions), end; it != end; ++it)
        {
            // Don't overwrite existing settings so command line settings override bitcoin.conf
            
            string strKey = it->string_key;
            strKey.erase(std::remove(strKey.begin(), strKey.end(), '-'), strKey.end());
            strKey.erase(std::remove(strKey.begin(), strKey.end(), '_'), strKey.end());
            
            strKey = string(prefix) + strKey;
            if ((*mapSettingsRet).count(strKey) == 0)
            {
                (*mapSettingsRet)[strKey] = it->value[0];
                // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set)
                mc_InterpretNegativeSetting(strKey, *mapSettingsRet);
            }
            if(mapMultiSettingsRet)
            {
                (*mapMultiSettingsRet)[strKey].push_back(it->value[0]);
            }
/*            
            try
            {
                ++it;
            }
            catch(std::exception &e1) {
                ++it;
            }
 */ 
        }
    } catch(std::exception &e) {
        fprintf(stderr,"ERROR: reading configuration file: %s\n", e.what());
        return MC_ERR_FILE_READ_ERROR;
    }
    
    return MC_ERR_NOERROR;
}


int mc_Params::ReadConfig(const char *network_name)
{
    mc_ReadConfigFile(mc_GetConfigFile(network_name,"multichain",".conf"),&mapArgs, &mapMultiArgs,"-");    
    return mc_ReadConfigFile(mc_GetConfigFile(NULL,NULL,".conf"),&mapArgs, &mapMultiArgs,"-");    
}

int mc_ReadGeneralConfigFile(mc_MapStringString *mapConfig,const char *network_name,const char *file_name,const char *extension)
{
    return mc_ReadConfigFile(mc_GetConfigFile(network_name,file_name,extension),(std::map<string, string>*)mapConfig->mapObject, NULL,"");
}


int mc_MultichainParams::SetGlobals()
{
    m_IsProtocolMultiChain=1;
    void *ptr=GetParam("chainprotocol",NULL);
    if(ptr)
    {
        if(strcmp((char*)ptr,"multichain"))
        {
            m_IsProtocolMultiChain=0;
        }
    }
    m_ProtocolVersion=ProtocolVersion();
    
    MIN_RELAY_TX_FEE=(unsigned int)GetInt64Param("minimumrelayfee");    
    MAX_OP_RETURN_RELAY=(unsigned int)GetInt64Param("maxstdopreturnsize");    
    MAX_OP_RETURN_RELAY=GetArg("-datacarriersize", MAX_OP_RETURN_RELAY);
    MAX_BLOCK_SIZE=(unsigned int)GetInt64Param("maximumblocksize");    
    DEFAULT_BLOCK_MAX_SIZE=MAX_BLOCK_SIZE;    
    while(MAX_BLOCK_SIZE>MAX_BLOCKFILE_SIZE)
    {
        MAX_BLOCKFILE_SIZE *= 2;
    }
    while(MAX_BLOCK_SIZE>MAX_SIZE)
    {
        MAX_SIZE *= 2;
    }
    MAX_STANDARD_TX_SIZE=(unsigned int)GetInt64Param("maxstdtxsize");    
    MAX_SCRIPT_ELEMENT_SIZE=(unsigned int)GetInt64Param("maxstdelementsize");
    COINBASE_MATURITY=(int)GetInt64Param("rewardspendabledelay");    
    COIN=GetInt64Param("nativecurrencymultiple");        
    CENT=COIN/100;    
    MAX_MONEY=GetInt64Param("maximumperoutput");    
    if((mc_gState->m_NetworkParams->GetInt64Param("initialblockreward") == 0) && (mc_gState->m_NetworkParams->GetInt64Param("firstblockreward") <= 0))
    {
        COIN=0;    
        CENT=0;
        MAX_MONEY=0;
    }
    
    if(mc_gState->m_Features->ShortTxIDAsAssetRef() == 0)
    {
        m_AssetRefSize=MC_AST_ASSET_REF_SIZE;
    }
    return MC_ERR_NOERROR;
}


void mc_SHA256::Init()
{
    m_HashObject=new CSHA256;    
    ((CSHA256*)m_HashObject)->Reset();
}

void mc_SHA256::Destroy()
{
    if(m_HashObject)
    {
        delete (CSHA256*)m_HashObject;
    }
}

void mc_SHA256::Reset()
{
    if(m_HashObject)
    {
        ((CSHA256*)m_HashObject)->Reset();
    }    
}

void mc_SHA256::Write(const void *lpData,int size)
{
    if(m_HashObject)
    {
        ((CSHA256*)m_HashObject)->Write((const unsigned char*)lpData,size);
    }    
}

void mc_SHA256::GetHash(unsigned char *hash)
{
    if(m_HashObject)
    {
        ((CSHA256*)m_HashObject)->Finalize(hash);
    }        
}

int mc_MultichainParams::Import(const char *name,const char *source_address)
{
    
    return MC_ERR_NOERROR;
}

std::string MultichainServerAddress()
{
    string result=string(mc_gState->m_NetworkParams->Name());
    unsigned char *ptr;
    result+="@";
    if(mc_gState->m_IPv4Address)
    {
        ptr=(unsigned char *)(&(mc_gState->m_IPv4Address));
        result+=strprintf("%u.%u.%u.%u",ptr[3],ptr[2],ptr[1],ptr[0]);
    }
    else
    {
        result+="<server-ip-address>";
    }
    
    return result;
}

int mc_SetIPv4ServerAddress(const char* host)
{
    char host_copy[16];
    char *ptr;
    uint32_t result;
    int count,v,i;
    result=0;
    mc_gState->m_IPv4Address=0;
    
    if((strlen(host)<7) || (strlen(host)>15))
    {
        return 0;
    }
    memcpy(host_copy,host,strlen(host)+1);
    count=0;
    for(i=0;i<(int)strlen(host);i++)
    {
        if(host_copy[i] == '.')
        {
            host_copy[i]=0x00;
            count++;
        }
    }
    if(count != 3)
    {
        return 0;
    }
    ptr=host_copy;
    for(i=0;i<4;i++)
    {
        if((strlen(ptr)<1) || (strlen(ptr)>3))
        {
            return 0;
        }
        v=atoi(ptr);
        if((v<0) || (v>255))
        {
            return 0;
        }
        result=(result << 8) + v;
        ptr+=strlen(ptr)+1;
    }
    
    mc_gState->m_IPv4Address=result;
    return result;
}

int mc_FindIPv4ServerAddress(uint32_t *all_ips,int max_ips)
{
    int i, l, c;
    uint32_t ip;
    unsigned char *ptr;
    int result;

    mc_gState->m_IPv4Address=0;
    l=0;
    
    result=0;
    c=0;

#ifdef MAC_OSX
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == -1) {
	return c;
    }
    if (!ifaddr) {
        return c;
    }
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock > 0) {
        for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
	    if (ifa->ifa_addr == 0) {
                continue;
	    }
            int family = ifa->ifa_addr->sa_family;
            if (family != AF_INET) {
                continue;
            }
            uint32_t a = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
            ptr=(unsigned char*)&a;

#elif !defined WIN32
    
    int sock;
    struct ifreq ifreqs[20];
    struct ifconf ic;
    ic.ifc_len = sizeof ifreqs;
    ic.ifc_req = ifreqs;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if( (sock >= 0) && (ioctl(sock, SIOCGIFCONF, &ic) >= 0) ) 
    {
        for (i = 0; i < (int)(ic.ifc_len/sizeof(struct ifreq)); i++)
        {
            uint32_t a;
            a=((struct sockaddr_in*)&ifreqs[i].ifr_addr)->sin_addr.s_addr;
            ptr=(unsigned char*)&a;
#else
    
    struct hostent *phe = gethostbyname("");
    if (phe) 
    {
        for (int i = 0; phe->h_addr_list[i] != 0; ++i) 
        {            
            struct in_addr addr;
            memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
            ptr=(unsigned char*)&addr;
            
#endif    
        
            if ((ptr[0] != 127) && (ptr[0] != 0))
            {
                ip=((uint32_t)ptr[0]<<24)+((uint32_t)ptr[1]<<16)+((uint32_t)ptr[2]<<8)+(uint32_t)ptr[3];
                if( (ptr[0] == 10) || ((ptr[0] == 192) && (ptr[1] == 168)) || ((ptr[0] == 172) && ((ptr[1] >= 16) && (ptr[1] <= 31))) )
                {
                    if( (l == 0) && (result == 0) )
                    {
                        mc_gState->m_IPv4Address=ip;
                        l=1;
                        result=1;
                    }
                }           
                else
                {
                    mc_gState->m_IPv4Address=ip;
                    l=0;
                    result=2;
                }
                if(c<max_ips-1)
                {
                    all_ips[c]=ip;
                    c++;
                }
            }        
            
        }
    }   
#ifdef MAC_OSX
    if (sock > 0) {
	close(sock);
    }
    freeifaddrs(ifaddr);
#endif
    return c;
}

        
