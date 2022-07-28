// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "utils/util.h"

#include "chainparams/chainparamsbase.h"
#include "utils/random.h"
#include "utils/serialize.h"
#include "utils/sync.h"
#include "utils/utilstrencodings.h"
#include "utils/utiltime.h"

#include "multichain/multichain.h"                                              // MCHN

#include <stdarg.h>

#ifndef WIN32
// for posix_fallocate
#ifdef __linux__

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L

#endif // __linux__

#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

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

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>

// Work around clang compilation problem in Boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function 'to_internal' that is neither visible in the template definition nor found by argument-dependent lookup
// See also: http://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           http://clang.debian.net/status.php?version=3.0&key=CANNOT_FIND_FUNCTION
namespace boost {

    namespace program_options {
        std::string to_internal(const std::string&);
    }

} // namespace boost

using namespace std;

map<string, string> mapArgs;
map<string, vector<string> > mapMultiArgs;
bool fDebug = false;
bool fPrintToConsole = false;
bool fPrintToDebugLog = true;
bool fPauseLogPrint = false;
bool fDaemon = false;
bool fServer = false;
string strMiscWarning;
bool fLogTimestamps = false;
bool fLogIPs = false;
bool fLogTimeMillis = false;
bool fInRecovery = false;
volatile bool fReopenDebugLog = false;

/**
 * LogPrintf() has been broken a couple of times now
 * by well-meaning people adding mutexes in the most straightforward way.
 * It breaks because it may be called by global destructors during shutdown.
 * Since the order of destruction of static/global objects is undefined,
 * defining a mutex as a global object doesn't work (the mutex gets
 * destroyed, and then some later destructor calls OutputDebugStringF,
 * maybe indirectly, and you get a core dump at shutdown trying to lock
 * the mutex).
 */

static boost::once_flag debugPrintInitFlag = BOOST_ONCE_INIT;
/**
 * We use boost::call_once() to make sure these are initialized
 * in a thread-safe manner the first time called:
 */
static FILE* fileout = NULL;
static boost::mutex* mutexDebugLog = NULL;

static void DebugPrintInit()
{
    assert(fileout == NULL);
    assert(mutexDebugLog == NULL);

    boost::filesystem::path pathDebug = GetLogDir() / "debug.log";
    fileout = fopen(pathDebug.string().c_str(), "a");
    if (fileout) setbuf(fileout, NULL); // unbuffered

    mutexDebugLog = new boost::mutex();
}

/* MCHN START */

void DebugPrintClose()
{
    if(fileout)
    {
        fclose(fileout);
        fileout=NULL;
    }
}

/* MCHN END */

bool LogAcceptCategory(const char* category)
{
    if (category != NULL)
    {
        static boost::thread_specific_ptr<set<string> > ptrPerfCategory;
        if(!fDebug)
        {
            if(!mapMultiArgs.empty() && !mapMultiArgs["-debugperf"].empty())
            {
                if (ptrPerfCategory.get() == NULL)
                {
                    const vector<string>& categories = mapMultiArgs["-debugperf"];
                    ptrPerfCategory.reset(new set<string>(categories.begin(), categories.end()));
                    // thread_specific_ptr automatically deletes the set when the thread ends.
                }
                const set<string>& setPerfCategories = *ptrPerfCategory.get();
                if (setPerfCategories.count(string(category)))
                    return true;
            }
        }
        
        if (!fDebug)
            return false;

        // Give each thread quick access to -debug settings.
        // This helps prevent issues debugging global destructors,
        // where mapMultiArgs might be deleted before another
        // global destructor calls LogPrint()
        static boost::thread_specific_ptr<set<string> > ptrCategory;
        if (ptrCategory.get() == NULL)
        {
            const vector<string>& categories = mapMultiArgs["-debug"];
            ptrCategory.reset(new set<string>(categories.begin(), categories.end()));
            // thread_specific_ptr automatically deletes the set when the thread ends.
        }
        const set<string>& setCategories = *ptrCategory.get();

        // if not debugging everything and not debugging specific category, LogPrint does nothing.
        if (setCategories.count(string("")) == 0 &&
            setCategories.count(string(category)) == 0)
            return false;
    }
    return true;
}

int LogPrintStr(const std::string &str)
{
    int ret = 0; // Returns total number of characters written
    
    if(fPauseLogPrint)
    {
        return ret;
    }
    
    if (fPrintToConsole)
    {
        // print to console
        ret = fwrite(str.data(), 1, str.size(), stdout);
        fflush(stdout);
    }
    else if (fPrintToDebugLog && AreBaseParamsConfigured())
    {
        static bool fStartedNewLine = true;
        boost::call_once(&DebugPrintInit, debugPrintInitFlag);

        if (fileout == NULL)
            return ret;

        boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);

        // reopen the log file, if requested
        if (fReopenDebugLog) {
            fReopenDebugLog = false;
            boost::filesystem::path pathDebug = GetLogDir() / "debug.log";
            if (freopen(pathDebug.string().c_str(),"a",fileout) != NULL)
                setbuf(fileout, NULL); // unbuffered
        }

        // Debug print useful for profiling
        if (fLogTimestamps && fStartedNewLine)
/* MCHN PRMF */            
            ret += fprintf(fileout, "%s", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str());
//            ret += fprintf(fileout, "%s ", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()).c_str());
        if (fLogTimestamps && fStartedNewLine)
        {
            if (fLogTimeMillis)
            {
                ret += fprintf(fileout, ".%03d ",(int)(GetTimeMillis()%1000));            
            }
            else
            {
                ret += fprintf(fileout, " ");            
            }
        }
/* MCHN END */            
        if (!str.empty() && str[str.size()-1] == '\n')
            fStartedNewLine = true;
        else
            fStartedNewLine = false;

        ret = fwrite(str.data(), 1, str.size(), fileout);
    }

    return ret;
}

static void InterpretNegativeSetting(string name, map<string, string>& mapSettingsRet)
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

void ParseParameters(int argc, const char* const argv[])
{
    mapArgs.clear();
    mapMultiArgs.clear();

    for (int i = 1; i < argc; i++)
    {
        std::string str(argv[i]);
        std::string strValue;
        size_t is_index = str.find('=');
        if (is_index != std::string::npos)
        {
            strValue = str.substr(is_index+1);
            str = str.substr(0, is_index);
        }
#ifdef WIN32
        boost::to_lower(str);
        if (boost::algorithm::starts_with(str, "/"))
            str = "-" + str.substr(1);
#endif

/* MCHN START */                            
// Ignoring arguments not starting with "-". original bitcoin code discard everything after that                

/*        
        if (str[0] != '-')
            break;
*/
        if (str[0] == '-')
        {
            // Interpret --foo as -foo.
            // If both --foo and -foo are set, the last takes effect.
            if (str.length() > 1 && str[1] == '-')
                str = str.substr(1);
            mapArgs[str] = strValue;
            mapMultiArgs[str].push_back(strValue);
        }
        
/* MCHN END */        
        
        
    }

    // New 0.6 features:
    BOOST_FOREACH(const PAIRTYPE(string,string)& entry, mapArgs)
    {
        // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
        InterpretNegativeSetting(entry.first, mapArgs);
    }
}

std::string GetArg(const std::string& strArg, const std::string& strDefault)
{
    if (mapArgs.count(strArg))
        return mapArgs[strArg];
    return strDefault;
}

int64_t GetArg(const std::string& strArg, int64_t nDefault)
{
    if (mapArgs.count(strArg))
        return atoi64(mapArgs[strArg]);
    return nDefault;
}

bool GetBoolArg(const std::string& strArg, bool fDefault)
{
    if (mapArgs.count(strArg))
    {
        if (mapArgs[strArg].empty())
            return true;
        return (atoi(mapArgs[strArg]) != 0);
    }
    return fDefault;
}

bool SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    if (mapArgs.count(strArg))
        return false;
    mapArgs[strArg] = strValue;
    return true;
}

bool SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

static std::string FormatException(std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(NULL, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "bitcoin";
#endif
    if (pex)
        return strprintf(
            "EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        return strprintf(
            "UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
}

void PrintExceptionContinue(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s\n", message);
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
}

/* MCHN START */
static boost::filesystem::path pathCachedMultiChain;
static boost::filesystem::path pathCachedMultiChainLog;
static CCriticalSection csPathCached;
/* MCHN END */

boost::filesystem::path GetDefaultDataDir()
{
    namespace fs = boost::filesystem;
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\Bitcoin
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\Bitcoin
    // Mac: ~/Library/Application Support/Bitcoin
    // Unix: ~/.bitcoin
/* MCHN START */
    LOCK(csPathCached);
    
    pathCachedMultiChain=fs::path(string(mc_gState->m_Params->DataDir()));
    fs::path &path =pathCachedMultiChain;
    return path;
//    fs::path &path = fNetSpecific ? pathCachedNetSpecific : pathCached;
/* MCHN END */
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "MultiChain";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    TryCreateDirectory(pathRet);
    return pathRet / "MultiChain";
#else
    // Unix
    return pathRet / ".multichain";
#endif
#endif
}

static boost::filesystem::path pathCached;
static boost::filesystem::path pathCachedNetSpecific;
//static CCriticalSection csPathCached;

const boost::filesystem::path &GetLogDir(bool fNetSpecific)
{
    namespace fs = boost::filesystem;
    
    LOCK(csPathCached);

/* MCHN START */
    fs::path &path =pathCachedMultiChainLog;
    if (!path.empty())
        return path;
    path=fs::path(string(mc_gState->m_Params->DataDir(2,1)));
    return pathCachedMultiChainLog;
}

const boost::filesystem::path &GetDataDir(bool fNetSpecific)
{
    namespace fs = boost::filesystem;
    
    LOCK(csPathCached);

/* MCHN START */
    fs::path &path =pathCachedMultiChain;
    if (!path.empty())
        return path;
    path=fs::path(string(mc_gState->m_Params->DataDir()));
    return pathCachedMultiChain;
//    pathCachedMultiChain=fs::path(string(mc_gState->m_Params->DataDir()));
//    fs::path &path =pathCachedMultiChain;
//    return path;
//    fs::path &path = fNetSpecific ? pathCachedNetSpecific : pathCached;
/* MCHN END */
    // This can be called during exceptions by LogPrintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    if (mapArgs.count("-datadir")) {
        path = fs::system_complete(mapArgs["-datadir"]);
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDefaultDataDir();
    }
    if (fNetSpecific)
        path /= BaseParams().DataDir();

    fs::create_directories(path);

    return path;
}

void ClearDatadirCache()
{
    pathCached = boost::filesystem::path();
    pathCachedNetSpecific = boost::filesystem::path();
}

boost::filesystem::path GetConfigFile()
{
    boost::filesystem::path pathConfigFile(GetArg("-conf", string(mc_gState->m_Params->NetworkName()) + ".conf"));    // MCHN
    if (!pathConfigFile.is_complete())
        pathConfigFile = GetDataDir(false) / pathConfigFile;

    return pathConfigFile;
}

bool SetupNetworking()
{
#ifdef WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion ) != 2 || HIBYTE(wsadata.wVersion) != 2)
        return false;
#endif
    return true;
}


void ReadConfigFile(map<string, string>& mapSettingsRet,
                    map<string, vector<string> >& mapMultiSettingsRet)
{
    boost::filesystem::ifstream streamConfig(GetConfigFile());
    if (!streamConfig.good())
        return; // No bitcoin.conf file is OK

    set<string> setOptions;
    setOptions.insert("*");

    for (boost::program_options::detail::config_file_iterator it(streamConfig, setOptions), end; it != end; ++it)
    {
        // Don't overwrite existing settings so command line settings override bitcoin.conf
        string strKey = string("-") + it->string_key;
        if (mapSettingsRet.count(strKey) == 0)
        {
            mapSettingsRet[strKey] = it->value[0];
            // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set)
            InterpretNegativeSetting(strKey, mapSettingsRet);
        }
        mapMultiSettingsRet[strKey].push_back(it->value[0]);
    }
    // If datadir is changed in .conf file:
    ClearDatadirCache();
}

//#ifndef WIN32
boost::filesystem::path GetPidFile()
{
    boost::filesystem::path pathPidFile(GetArg("-pid", "multichain.pid"));
    if (!pathPidFile.is_complete()) pathPidFile = GetDataDir() / pathPidFile;
    return pathPidFile;
}

bool CreatePidFile(const boost::filesystem::path &path, int pid)
{
    bool file_already_exist=boost::filesystem::exists(path);
    FILE* file = fopen(path.string().c_str(), "w");
    if (file)
    {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
    return file_already_exist;
}
//#endif

bool RenameOver(boost::filesystem::path src, boost::filesystem::path dest)
{
#ifdef WIN32
    return MoveFileExA(src.string().c_str(), dest.string().c_str(),
                       MOVEFILE_REPLACE_EXISTING) != 0;
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

/**
 * Ignores exceptions thrown by Boost's create_directory if the requested directory exists.
 * Specifically handles case where path p exists, but it wasn't possible for the user to
 * write to the parent directory.
 */
bool TryCreateDirectory(const boost::filesystem::path& p)
{
    try
    {
        return boost::filesystem::create_directory(p);
    } catch (boost::filesystem::filesystem_error) {
        if (!boost::filesystem::exists(p) || !boost::filesystem::is_directory(p))
            throw;
    }

    // create_directory didn't create the directory, it had to have existed already
    return false;
}

void FileCommit(FILE *fileout)
{
    fflush(fileout); // harmless if redundantly called
#ifdef WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fileout));
    FlushFileBuffers(hFile);
#else
    #if defined(__linux__) || defined(__NetBSD__)
    fdatasync(fileno(fileout));
    #elif defined(__APPLE__) && defined(F_FULLFSYNC)
    fcntl(fileno(fileout), F_FULLFSYNC, 0);
    #else
    fsync(fileno(fileout));
    #endif
#endif
}

bool TruncateFile(FILE *file, unsigned int length) {
#if defined(WIN32)
    return _chsize(_fileno(file), length) == 0;
#else
    return ftruncate(fileno(file), length) == 0;
#endif
}

/**
 * this function tries to raise the file descriptor limit to the requested number.
 * It returns the actual file descriptor limit (which may be more or less than nMinFD)
 */
int RaiseFileDescriptorLimit(int nMinFD) {
#if defined(WIN32)
    return 2048;
#else
    struct rlimit limitFD;
    if (getrlimit(RLIMIT_NOFILE, &limitFD) != -1) {
        if (limitFD.rlim_cur < (rlim_t)nMinFD) {
            limitFD.rlim_cur = nMinFD;
            if (limitFD.rlim_cur > limitFD.rlim_max)
                limitFD.rlim_cur = limitFD.rlim_max;
            setrlimit(RLIMIT_NOFILE, &limitFD);
            getrlimit(RLIMIT_NOFILE, &limitFD);
        }
        return limitFD.rlim_cur;
    }
    return nMinFD; // getrlimit failed, assume it's fine
#endif
}

/**
 * this function tries to make a particular range of a file allocated (corresponding to disk space)
 * it is advisory, and the range specified in the arguments will never contain live data
 */
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length) {
#if defined(WIN32)
    // Windows-specific version
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    LARGE_INTEGER nFileSize;
    int64_t nEndPos = (int64_t)offset + length;
    nFileSize.u.LowPart = nEndPos & 0xFFFFFFFF;
    nFileSize.u.HighPart = nEndPos >> 32;
    SetFilePointerEx(hFile, nFileSize, 0, FILE_BEGIN);
    SetEndOfFile(hFile);
#elif defined(MAC_OSX)
    // OSX specific version
    fstore_t fst;
    fst.fst_flags = F_ALLOCATECONTIG;
    fst.fst_posmode = F_PEOFPOSMODE;
    fst.fst_offset = 0;
    fst.fst_length = (off_t)offset + length;
    fst.fst_bytesalloc = 0;
    if (fcntl(fileno(file), F_PREALLOCATE, &fst) == -1) {
        fst.fst_flags = F_ALLOCATEALL;
        fcntl(fileno(file), F_PREALLOCATE, &fst);
    }
    ftruncate(fileno(file), fst.fst_length);
#elif defined(__linux__)
    // Version using posix_fallocate
    off_t nEndPos = (off_t)offset + length;
    posix_fallocate(fileno(file), 0, nEndPos);
#else
    // Fallback version
    // TODO: just write one byte per block
    static const char buf[65536] = {};
    fseek(file, offset, SEEK_SET);
    while (length > 0) {
        unsigned int now = 65536;
        if (length < now)
            now = length;
        fwrite(buf, 1, now, file); // allowed to fail; this function is advisory anyway
        length -= now;
    }
#endif
}

void ShrinkDebugFile(const char* FileName)
{
    // Scroll debug.log if it's getting too big
    boost::filesystem::path pathLog = GetLogDir() / string(FileName);
    FILE* file = fopen(pathLog.string().c_str(), "r");
/* MCHN START */    
    size_t bytes_written;
    uint64_t shrink_size=GetArg("-shrinkdebugfilesize",200000);
    if(shrink_size > 67108864)
    {
        shrink_size = 67108864;
    }
/* MCHN END */        
    if (file && boost::filesystem::file_size(pathLog) > 5 * shrink_size)// 10 * 1000000)
    {
        // Restart the file with some of the end
        std::vector <char> vch(shrink_size,0);//200000,0);
        fseek(file, -((long)vch.size()), SEEK_END);
        int nBytes = fread(begin_ptr(vch), 1, vch.size(), file);
        fclose(file);

        file = fopen(pathLog.string().c_str(), "w");
        if (file)
        {
            bytes_written=fwrite(begin_ptr(vch), 1, nBytes, file);
            if((int)bytes_written != nBytes)
            {
                fclose(file);
            }
            else
            {
                fclose(file);
            }
        }
    }
    else if (file != NULL)
        fclose(file);
}

void ShrinkDebugFile()
{
    if(!mapMultiArgs["-debugperf"].empty())
    {
       return; 
    }
    
    ShrinkDebugFile("debug.log");
    ShrinkDebugFile("permissions.log");
    ShrinkDebugFile("wallet/txs.log");
    // Scroll debug.log if it's getting too big
/*    
    boost::filesystem::path pathLog = GetLogDir() / "debug.log";
    FILE* file = fopen(pathLog.string().c_str(), "r");
    int64_t shrink_size=GetArg("-shrinkdebugfilesize",200000);
    if(shrink_size > 67108864)
    {
        shrink_size = 67108864;
    }
    if (file && boost::filesystem::file_size(pathLog) > 5 * shrink_size)// 10 * 1000000)
    {
        // Restart the file with some of the end
        std::vector <char> vch(shrink_size,0);//200000,0);
        fseek(file, -((long)vch.size()), SEEK_END);
        int nBytes = fread(begin_ptr(vch), 1, vch.size(), file);
        fclose(file);

        file = fopen(pathLog.string().c_str(), "w");
        if (file)
        {
            fwrite(begin_ptr(vch), 1, nBytes, file);
            fclose(file);
        }
    }
    else if (file != NULL)
        fclose(file);
 */ 
}

#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    namespace fs = boost::filesystem;

    char pszPath[MAX_PATH] = "";

    if(SHGetSpecialFolderPathA(NULL, pszPath, nFolder, fCreate))
    {
        return fs::path(pszPath);
    }

    LogPrintf("SHGetSpecialFolderPathA() failed, could not obtain requested path.\n");
    return fs::path("");
}
#endif

boost::filesystem::path GetTempPath() {
#if BOOST_FILESYSTEM_VERSION == 3
    return boost::filesystem::temp_directory_path();
#else
    // TODO: remove when we don't support filesystem v2 anymore
    boost::filesystem::path path;
#ifdef WIN32
    char pszPath[MAX_PATH] = "";

    if (GetTempPathA(MAX_PATH, pszPath))
        path = boost::filesystem::path(pszPath);
#else
    path = boost::filesystem::path("/tmp");
#endif
    if (path.empty() || !boost::filesystem::is_directory(path)) {
        LogPrintf("GetTempPath(): failed to find temp path\n");
        return boost::filesystem::path("");
    }
    return path;
#endif
}

void runCommand(std::string strCommand)
{
    int nErr = ::system(strCommand.c_str());
    if (nErr)
        LogPrintf("runCommand error: system(%s) returned %d\n", strCommand, nErr);
}

void RenameThread(const char* name)
{
#if defined(PR_SET_NAME)
    // Only the first 15 characters are used (16 - NUL terminator)
    ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif 0 && (defined(__FreeBSD__) || defined(__OpenBSD__))
    // TODO: This is currently disabled because it needs to be verified to work
    //       on FreeBSD or OpenBSD first. When verified the '0 &&' part can be
    //       removed.
    pthread_set_name_np(pthread_self(), name);

#elif defined(MAC_OSX) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED)

// pthread_setname_np is XCode 10.6-and-later
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
    pthread_setname_np(name);
#endif

#else
    // Prevent warnings for unused parameters...
    (void)name;
#endif
}

void SetupEnvironment()
{
#ifndef WIN32
    try
    {
#if BOOST_FILESYSTEM_VERSION == 3
            boost::filesystem::path::codecvt(); // Raises runtime error if current locale is invalid
#else // boost filesystem v2
            std::locale();                      // Raises runtime error if current locale is invalid
#endif
    } catch(std::runtime_error &e)
    {
        setenv("LC_ALL", "C", 1); // Force C locale
    }
#endif
}

void SetThreadPriority(int nPriority)
{
#ifdef WIN32
    SetThreadPriority(GetCurrentThread(), nPriority);
#else // WIN32
#ifdef PRIO_THREAD
    setpriority(PRIO_THREAD, 0, nPriority);
#else // PRIO_THREAD
    setpriority(PRIO_PROCESS, 0, nPriority);
#endif // PRIO_THREAD
#endif // WIN32
}


std::string mc_SupportedProtocols()
{
    std::string protocol_list;
    int protocol_min,protocol_max,protocol_next,this_build,next_build,out_it;
    
    protocol_list="";
    this_build=mc_gState->GetNumericVersion();
    protocol_next=mc_gState->MinProtocolVersion();
    protocol_min=0;
    protocol_max=-1;
    next_build=-mc_gState->VersionInfo(protocol_next);
    
    while(next_build <= this_build)
    {
        out_it=0;
        if(next_build > 0)
        {
            if(next_build == this_build)
            {
                if(protocol_min == 0)
                {
                    protocol_min=protocol_next;                    
                }
                protocol_max=protocol_next;
            }
            else
            {
                out_it=1;
            }
            protocol_next++;
        }
        else
        {
            out_it=1;   
            protocol_next=-next_build;
        }
        next_build=-mc_gState->VersionInfo(protocol_next);
        if(next_build > this_build)
        {
            out_it=1;
        }
        if(out_it)
        {
            if(protocol_list.size())
            {
                protocol_list += ", ";
            }
            if(protocol_max > protocol_min)
            {
                protocol_list += strprintf("%d-%d",protocol_min,protocol_max);
            }
            else
            {
                protocol_list += strprintf("%d",protocol_min);                
            }
            protocol_min=0;
            protocol_max=-1;
        }
    }
    
    
    return protocol_list;    
}

std::string mc_BuildDescription(int build)
{
    char build_desc[32];
    mc_BuildDescription(build,build_desc);
    return std::string(build_desc);
}

bool mc_CopyFile(boost::filesystem::path& pathDBOld,boost::filesystem::path& pathDBNew)
{
#ifndef WIN32
    
    try {
#if BOOST_VERSION >= 104000
                    boost::filesystem::copy_file(pathDBOld, pathDBNew, boost::filesystem::copy_option::overwrite_if_exists);
#else
                    filesystem::copy_file(pathSrc, pathDest);
#endif
    } catch(const boost::filesystem::filesystem_error &e) {
        LogPrintf("error copying %s to %s - %s\n", pathDBOld.string(), pathDBNew.string(), e.what());
        return false;
    }
    return true;
#else
    return CopyFile(pathDBOld.string().c_str(),pathDBNew.string().c_str(),false);
#endif
}