// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "version/clientversion.h"
#include "rpc/rpcserver.h"
#include "core/init.h"
#include "core/main.h"
#include "ui/noui.h"
#include "ui/ui_interface.h"
#include "utils/util.h"


#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include "multichain/multichain.h"
#include "chainparams/globals.h"
static bool fDaemon;
mc_EnterpriseFeatures* pEF = NULL;

void DebugPrintClose();
std::string HelpMessage_Cold();
bool AppInit2_Cold(boost::thread_group& threadGroup,int OutputPipe=STDOUT_FILENO);
void Shutdown_Cold();

void DetectShutdownThread(boost::thread_group* threadGroup)
{
    bool fShutdown = ShutdownRequested();
    // Tell the main threads to shutdown.
    while (!fShutdown)
    {
        MilliSleep(200);
        fShutdown = ShutdownRequested();
    }
    if (threadGroup)
    {
        threadGroup->interrupt_all();
        threadGroup->join_all();
    }
}

bool mc_DoesParentDataDirExist()
{
    if (mapArgs.count("-datadir"))
    {
        boost::filesystem::path path=boost::filesystem::system_complete(mapArgs["-datadir"]);
        if (!boost::filesystem::is_directory(path)) 
        {
            return false;
        }    
    }
    return true;
}

bool mc_DoesParentLogDirExist()
{
    if (mapArgs.count("-logdir"))
    {
        boost::filesystem::path path=boost::filesystem::system_complete(mapArgs["-logdir"]);
        if (!boost::filesystem::is_directory(path)) 
        {
            return false;
        }    
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
bool AppInit(int argc, char* argv[])
{
    boost::thread_group threadGroup;
    boost::thread* detectShutdownThread = NULL;

    bool fRet = false;

    int size;
    int err = MC_ERR_NOERROR;
    int pipes[2];
    int bytes_read;
    int bytes_written;
    char bufOutput[4096];
    bool is_daemon;
    //
    // Parameters
    //
    // If Qt is used, parameters/bitcoin.conf are parsed in qt/bitcoin.cpp's main()

#ifndef WIN32
    umask(077);        
#endif
        
    mc_gState=new mc_State;
    
    mc_gState->m_SessionFlags |= MC_SSF_COLD;
    
    mc_gState->m_Params->Parse(argc, argv, MC_ETP_DAEMON);
    mc_CheckDataDirInConfFile();
    
    if(mc_gState->m_Params->NetworkName())
    {
        if(strlen(mc_gState->m_Params->NetworkName()) > MC_PRM_NETWORK_NAME_MAX_SIZE)
        {
            fprintf(stderr, "Error: invalid chain name: %s\n",mc_gState->m_Params->NetworkName());
            return false;
        }
    }
    
    
    
    if(!mc_DoesParentDataDirExist())
    {
        fprintf(stderr,"\nError: Data directory %s needs to exist before calling multichaind-cold. Exiting...\n\n",mapArgs["-datadir"].c_str());
        return false;        
    }
        
    if(!mc_DoesParentLogDirExist())
    {
        fprintf(stderr,"\nError: Log directory %s needs to exist before calling multichaind-cold. Exiting...\n\n",mapArgs["-logdir"].c_str());
        return false;        
    }
        
    
    mc_gState->m_Params->HasOption("-?");
            
    // Process help and version before taking care about datadir
    if (mc_gState->m_Params->HasOption("-?") || 
        mc_gState->m_Params->HasOption("-help") || 
        mc_gState->m_Params->HasOption("-version") || 
        (mc_gState->m_Params->NetworkName() == NULL))
    {
        fprintf(stdout,"\nMultiChain %s Offline Daemon (protocol %s)\n\n",mc_BuildDescription(mc_gState->GetNumericVersion()).c_str(),mc_SupportedProtocols().c_str());
        std::string strUsage = "";
        if (mc_gState->m_Params->HasOption("-version"))
        {
            strUsage += LicenseInfo();
        }
        else
        {
            strUsage += "\n" + _("Usage:") + "\n" +
                  "  multichaind-cold <blockchain-name> [options]                     " + _("Start MultiChain Offline Daemon") + "\n";

            strUsage += "\n" + HelpMessage_Cold();  
        }

        fprintf(stdout, "%s", strUsage.c_str());

        delete mc_gState;                
        return true;
    }

    if(!GetBoolArg("-shortoutput", false))
    {
        fprintf(stdout,"\nMultiChain %s Offline Daemon (latest protocol %d)\n\n",mc_BuildDescription(mc_gState->GetNumericVersion()).c_str(),mc_gState->GetProtocolVersion());
    }
    
    pipes[1]=STDOUT_FILENO;
    is_daemon=false;
#ifndef WIN32
        fDaemon = GetBoolArg("-daemon", false);
        
        if (fDaemon)
        {
            delete mc_gState;                
            
            if(!GetBoolArg("-shortoutput", false))
            {
                fprintf(stdout, "Starting up node...\n");
            }
            
            if (pipe(pipes)) 
            {
                fprintf(stderr, "Error: couldn't create pipe between parent and child processes\n");
                return false;
            }
            
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            
            if(pid == 0)
            {
                is_daemon=true;
                close(pipes[0]);
            }
            
            if (pid > 0)                                                        // Parent process, pid is child process id
            {
                close(pipes[1]);            
                bytes_read=1;                
                while(bytes_read>0)
                {
                    bytes_read=read(pipes[0],bufOutput,4096);
                    if(bytes_read <= 0)
                    {
                        return true;                        
                    }
                    bytes_written=write(STDOUT_FILENO,bufOutput,bytes_read);
                    if(bytes_written != bytes_read)
                    {
                        return true;                                                
                    }
                }
                return true;
            }
            
            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "ERROR: setsid() returned %d errno %d\n", sid, errno);
            
            mc_gState=new mc_State;
            mc_gState->m_SessionFlags |= MC_SSF_COLD;

            mc_gState->m_Params->Parse(argc, argv, MC_ETP_DAEMON);
            mc_CheckDataDirInConfFile();
        }
#endif
        
    mc_GenerateConfFiles(mc_gState->m_Params->NetworkName());                
    
    err=mc_gState->m_Params->ReadConfig(mc_gState->m_Params->NetworkName());
    
    if(err)
    {
        fprintf(stderr,"ERROR: Couldn't read parameter file for blockchain %s. Exiting...\n",mc_gState->m_Params->NetworkName());
        delete mc_gState;                
        return false;
    }

    err=mc_gState->m_NetworkParams->Read(mc_gState->m_Params->NetworkName());
    if(err)
    {
        fprintf(stderr,"ERROR: Couldn't read configuration file for blockchain %s. Please try upgrading MultiChain. Exiting...\n",mc_gState->m_Params->NetworkName());
        delete mc_gState;                
        return false;
    }
    
    err=mc_gState->m_NetworkParams->Validate();
    if(err)
    {
        fprintf(stderr,"ERROR: Couldn't validate parameter set for blockchain %s. Exiting...\n",mc_gState->m_Params->NetworkName());
        delete mc_gState;                
        return false;
    }
    
    SoftSetBoolArg("-offline",true);

    if(mc_gState->m_NetworkParams->m_Status != MC_PRM_STATUS_VALID)
    {
        char fileName[MC_DCT_DB_MAX_PATH];
        mc_GetFullFileName(mc_gState->m_Params->NetworkName(),"params", ".dat",MC_FOM_RELATIVE_TO_DATADIR,fileName);
        fprintf(stderr,"ERROR: Parameter set for blockchain %s is not valid.\n\nThe file %s must be copied manually from an existing node into empty blockchain directory.\n\n",
                mc_gState->m_Params->NetworkName(),fileName);                        
        delete mc_gState;                
        return false;        
    }
    
    if(GetBoolArg("-reindex", false))
    {
        mc_RemoveDir(mc_gState->m_Params->NetworkName(),"permissions.db");
        mc_RemoveFile(mc_gState->m_Params->NetworkName(),"permissions",".dat",MC_FOM_RELATIVE_TO_DATADIR);
        mc_RemoveFile(mc_gState->m_Params->NetworkName(),"permissions",".log",MC_FOM_RELATIVE_TO_DATADIR);
    }
    
    mc_gState->m_Permissions= new mc_Permissions;
    err=mc_gState->m_Permissions->Initialize(mc_gState->m_Params->NetworkName(),0);                                
    if(err)
    {
        if(err == MC_ERR_CORRUPTED)
        {
            fprintf(stderr,"\nERROR: Couldn't initialize permission database for blockchain %s. Please restart multichaind with reindex=1.\n",mc_gState->m_Params->NetworkName());                        
        }
        else
        {
            fprintf(stderr,"\nERROR: Couldn't initialize permission database for blockchain %s. Probably multichaind for this blockchain is already running. Exiting...\n",mc_gState->m_Params->NetworkName());
        }
        delete mc_gState;                
        return false;
    }

    if(mc_gState->m_NetworkParams->GetParam("protocolversion",&size) != NULL)
    {
        int protocol_version=(int)mc_gState->m_NetworkParams->GetInt64Param("protocolversion");

        if(mc_gState->IsSupported(protocol_version) == 0) 
        {
            if(mc_gState->IsDeprecated(protocol_version))
            {
                fprintf(stderr,"ERROR: The protocol version (%d) for blockchain %s has been deprecated and was last supported in MultiChain %s\n\n",
                        protocol_version,mc_gState->m_Params->NetworkName(),
                        mc_BuildDescription(-mc_gState->VersionInfo(protocol_version)).c_str());                        
                delete mc_gState;                
                return false;
            }
            else
            {
                fprintf(stderr,"ERROR: Parameter set for blockchain %s was generated by MultiChain running newer protocol version (%d)\n\n",
                        mc_gState->m_Params->NetworkName(),protocol_version);                        
                fprintf(stderr,"Please upgrade MultiChain\n\n");
                delete mc_gState;                
                return false;
            }
        }
    }
                        
    SelectMultiChainParams(mc_gState->m_Params->NetworkName());

    try
    {
/*        
#ifndef WIN32
        fDaemon = GetBoolArg("-daemon", false);
        
        if (fDaemon)
        {
            fprintf(stdout, "MultiChain server starting\n");

            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                delete mc_gState;
                return false;
            }
            if (pid > 0) // Parent process, pid is child process id
            {
                delete mc_gState;
                return true;
            }
            
            pid_t sid = setsid();
            if (sid < 0)
                fprintf(stderr, "ERROR: setsid() returned %d errno %d\n", sid, errno);
            
        }
#endif
*/
        SoftSetBoolArg("-server", true);
        detectShutdownThread = new boost::thread(boost::bind(&DetectShutdownThread, &threadGroup));
        fRet = AppInit2_Cold(threadGroup,pipes[1]);
        if(is_daemon)
        {
            close(pipes[1]);            
        }
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "AppInit()");
    } catch (...) {
        PrintExceptionContinue(NULL, "AppInit()");
    }

    if (!fRet)
    {
        if (detectShutdownThread)
            detectShutdownThread->interrupt();

        threadGroup.interrupt_all();
        // threadGroup.join_all(); was left out intentionally here, because we didn't re-test all of
        // the startup-failure cases to make sure they don't result in a hang due to some
        // thread-blocking-waiting-for-another-thread-during-startup case
    }
    if (detectShutdownThread)
    {
        detectShutdownThread->join();
        delete detectShutdownThread;
        detectShutdownThread = NULL;
    }

    Shutdown();    
    DebugPrintClose();
    
    delete mc_gState;
    
    return fRet;
}


int main(int argc, char* argv[])
{
    SetupEnvironment();



    // Connect bitcoind signal handlers
    noui_connect();

    return (AppInit(argc, argv) ? 0 : 1);

}

