// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "community/community.h"

using namespace std;

void mc_EnterpriseFeatures::Zero()
{
    m_Impl=NULL;
}

void mc_EnterpriseFeatures::Destroy()
{
    Zero();
}

int mc_EnterpriseFeatures::Prepare()
{
    return MC_ERR_NOERROR;
}

int mc_EnterpriseFeatures::Initialize(const char *name,uint32_t mode)
{
    
    return MC_ERR_NOERROR;
}

int mc_EnterpriseFeatures::STR_CreateSubscription(mc_TxEntity *entity,const std::string parameters)
{
    return MC_ERR_FOUND;        
}

uint32_t mc_EnterpriseFeatures::STR_CheckAutoSubscription(const std::string parameters,bool check_license)
{
    return 0;
}

int mc_EnterpriseFeatures::STR_CreateAutoSubscription(mc_TxEntity *entity)
{
    return MC_ERR_NOERROR;        
}

int mc_EnterpriseFeatures::STR_TrimSubscription(mc_TxEntity *entity,const std::string parameters)
{
    return MC_ERR_NOERROR;        
}

int mc_EnterpriseFeatures::STR_IsIndexSkipped(mc_TxImport *import,mc_TxEntity *parent_entity,mc_TxEntity *entity)
{
    return 0;
}

int mc_EnterpriseFeatures::STR_NoRetrieve(mc_TxEntity *entity)
{
    return 0;
}

int mc_EnterpriseFeatures::STR_IsOutOfSync(mc_TxEntity *entity)
{
    return 0;
}

int mc_EnterpriseFeatures::STR_SetSyncFlag(mc_TxEntity *entity,bool confirm)
{
    return MC_ERR_NOERROR;
}

int mc_EnterpriseFeatures::STR_GetSubscriptions(mc_Buffer *subscriptions)
{
    return MC_ERR_NOERROR;        
}

int mc_EnterpriseFeatures::STR_PutSubscriptions(mc_Buffer *subscriptions)
{
    return MC_ERR_NOERROR;        
}

Value mc_EnterpriseFeatures::STR_RPCRetrieveStreamItems(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::STR_RPCPurgeStreamItems(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::STR_RPCPurgePublishedItems(const Array& params)
{
    return Value::null;
}

int mc_EnterpriseFeatures::STR_RemoveDataFromFile(const char *name)
{
    return MC_ERR_NOERROR;
}

Value mc_EnterpriseFeatures::FED_RPCCreateFeed(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::FED_RPCDeleteFeed(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::FED_RPCPauseFeed(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::FED_RPCResumeFeed(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::FED_RPCPurgeFeed(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::FED_RPCListFeeds(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::FED_RPCAddToFeed(const Array& params)
{
    return Value::null;   
}

Value mc_EnterpriseFeatures::FED_RPCUpdateFeed(const Array& params)
{
    return Value::null;    
}



    
Value mc_EnterpriseFeatures::DRF_RPCGetDataRefData(const Array& params)
{
    return Value::null;    
}

Value mc_EnterpriseFeatures::DRF_RPCDataRefToBinaryCache(const Array& params)
{
    return Value::null;
}

bool mc_EnterpriseFeatures::DRF_GetData(string dataref,CScript &script,const unsigned char **elem,int64_t *size,uint32_t *format,std::string &strError)
{
    return false;
}

int mc_EnterpriseFeatures::FED_EventTx(const CTransaction& tx,int block,CDiskTxPos* block_pos,uint32_t block_tx_index,uint256 block_hash,uint32_t block_timestamp)
{
    return MC_ERR_NOERROR;
}

int mc_EnterpriseFeatures::FED_EventChunksAvailable()
{
    return MC_ERR_NOERROR;    
}

int mc_EnterpriseFeatures::FED_EventPurgeChunk(mc_ChunkDBRow *chunk_def)
{
    return MC_ERR_NOERROR;    
}   

int mc_EnterpriseFeatures::FED_EventBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex,std::string type,bool after,bool error)
{
    return MC_ERR_NOERROR;        
}

int mc_EnterpriseFeatures::FED_EventInvalidateTx(const CTransaction& tx,int error_code,std::string error_message)
{
    return MC_ERR_NOERROR;            
}

bool mc_EnterpriseFeatures::OFF_ProcessChunkRequest(unsigned char *ptrStart,unsigned char *ptrEnd,vector<unsigned char>* payload_response,vector<unsigned char>* payload_relay,
        map<uint160,int>& mapReadPermissionedStreams,string& strError)
{
    return false;    
}

bool mc_EnterpriseFeatures::OFF_GetScriptsToVerify(map<uint160,int>& mapReadPermissionedStreams,vector<CScript>& vSigScriptsIn,vector<CScript>& vSigScriptsToVerify,string& strError)
{
    return true;
}

bool mc_EnterpriseFeatures::OFF_VerifySignatureScripts(uint32_t  msg_type,mc_OffchainMessageID& msg_id,mc_OffchainMessageID& msg_id_to_respond,uint32_t  flags,
            vector<unsigned char>& vPayload,vector<CScript>& vSigScriptsToVerify,string& strError,int& dos_score)
{
    return true;        
}

bool mc_EnterpriseFeatures::OFF_CreateSignatureScripts(uint32_t  msg_type,mc_OffchainMessageID& msg_id,mc_OffchainMessageID& msg_id_to_respond,uint32_t  flags,
            vector<unsigned char>& vPayload,set<CPubKey>& vAddresses,vector<CScript>& vSigScripts,string& strError)
{    
    return true;
}

bool mc_EnterpriseFeatures::OFF_GetPayloadForReadPermissioned(vector<unsigned char>* payload,int *ef_cache_id,string& strError)
{
    *ef_cache_id=-1;
    return true;
}

void mc_EnterpriseFeatures::OFF_FreeEFCache(int ef_cache_id)
{
    
}

bool mc_EnterpriseFeatures::OFF_ProcessChunkResponse(mc_RelayRequest *request,mc_RelayResponse *response,map <int,int>* request_pairs,mc_ChunkCollector* collector,string& strError)
{
    return true;
}

unsigned char* mc_EnterpriseFeatures::OFF_SupportedEnterpriseFeatures(unsigned char* min_ef,int min_ef_size,int *ef_size)
{
    if(ef_size)
    {
        *ef_size=0;
    }
    return NULL;
}


CPubKey mc_EnterpriseFeatures::WLT_FindReadPermissionedAddress(mc_EntityDetails* entity)
{
    CPubKey result;
    
    return result;
}



int mc_EnterpriseFeatures::WLT_CreateSubscription(mc_TxEntity *entity,uint32_t retrieve,uint32_t indexes,uint32_t *rescan_mode)
{
    *rescan_mode=0;
    return MC_ERR_NOERROR;    
}

int mc_EnterpriseFeatures::WLT_DeleteSubscription(mc_TxEntity *entity,uint32_t rescan_mode)
{
    return MC_ERR_NOERROR;        
}

int mc_EnterpriseFeatures::WLT_StartImport()
{
    return MC_ERR_NOERROR;            
}

int mc_EnterpriseFeatures::WLT_CompleteImport()
{
    return MC_ERR_NOERROR;            
}

int mc_EnterpriseFeatures::WLT_NoIndex(mc_TxEntity *entity)
{
    return 0;
}

int mc_EnterpriseFeatures::WLT_NoRetrieve(mc_TxEntity *entity)
{
    return 0;
}

bool mc_EnterpriseFeatures::NET_ProcessHandshakeData(CNode* pfrom, std::string sENTData,bool fIsVerackack)
{
    return true;
}

std::vector<unsigned char> mc_EnterpriseFeatures::NET_PushHandshakeData(CNode* pfrom, bool fIsVerackack)
{
    vector<unsigned char> result;
    return result;
}

bool mc_EnterpriseFeatures::NET_FinalizeHandshake(CNode* pfrom)
{
    return true;
}


void mc_EnterpriseFeatures::NET_FreeNodeData(void *pNodeData)
{
    
}

bool mc_EnterpriseFeatures::NET_IsEncrypted(CNode* pfrom)
{
    return false;
}

bool mc_EnterpriseFeatures::NET_IsFinalized(CNode* pfrom)
{
    return true;    
}


int mc_EnterpriseFeatures::NET_ReadHeader(void *pNodeData,CNetMessage& msg,const char *pch,unsigned int nBytes)
{
    return -1;
}

void mc_EnterpriseFeatures::NET_ProcessMsgData(void *pNodeData,CNetMessage& msg)
{
    
}

bool mc_EnterpriseFeatures::NET_PushMsg(void *pNodeData,CDataStream& ssSend)
{
    return true;
}

int mc_EnterpriseFeatures::NET_StoreInCache(void *pNodeData,CNetMessage& msg,const char *pch,unsigned int nBytes)
{
    return 0;
}

bool mc_EnterpriseFeatures::NET_RestoreFromCache(CNode* pfrom)
{
    return true;
}

void mc_EnterpriseFeatures::NET_CheckConnections()
{
    
}

uint64_t mc_EnterpriseFeatures::NET_Services()
{
    return 0;
}



string mc_EnterpriseFeatures::ENT_Edition() 
{
    return "Community";
}

int mc_EnterpriseFeatures::ENT_BuildVersion()
{
    return 0;
}

int mc_EnterpriseFeatures::ENT_EditionNumeric() 
{
    return 0;
}

int mc_EnterpriseFeatures::ENT_MinWalletDatVersion()
{
    return 2;
}

void mc_EnterpriseFeatures::ENT_RPCVerifyEdition(string message) 
{
	string error="This feature is only available in MultiChain Enterprise";
    if(message.size())
    {
        error += ": ";
        error += message;
    }
    throw JSONRPCError(RPC_NOT_SUPPORTED, error);        
}

std::string mc_EnterpriseFeatures::ENT_TextConstant(const char* name)
{
    return "";
}

void mc_EnterpriseFeatures::ENT_MaybeStop()
{
    
}

void mc_EnterpriseFeatures::ENT_InitRPCHelpMap()
{
    
}

void mc_EnterpriseFeatures::ENT_Debug(std::string action,std::string parameter)
{
    
}


void mc_EnterpriseFeatures::LIC_RPCVerifyFeature(uint64_t feature,string message)
{
	string error="This feature is only available in MultiChain Enterprise";
    if(message.size())
    {
        error += ": ";
        error += message;
    }
    throw JSONRPCError(RPC_NOT_SUPPORTED, error);        
}

bool mc_EnterpriseFeatures::LIC_VerifyFeature(uint64_t feature,std::string& reason)
{
    reason="Not available in Community edition";
    return false;
}

std::vector<std::string> mc_EnterpriseFeatures::LIC_LicensesWithStatus(std::string status)
{
    std::vector<std::string> result;
    return result;
}

int mc_EnterpriseFeatures::LIC_VerifyLicenses(int block)
{
    return MC_ERR_NOERROR;
}

int mc_EnterpriseFeatures::LIC_VerifyUpdateCoin(int block,mc_Coin *coin,bool is_new)
{
    return MC_ERR_NOERROR;    
}

Value mc_EnterpriseFeatures::LIC_RPCDecodeLicenseRequest(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCDecodeLicenseConfirmation(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCActivateLicense(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCTransferLicense(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCListLicenseRequests(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCGetLicenseConfirmation(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCTakeLicense(const Array& params)
{
    return Value::null;
}

Value mc_EnterpriseFeatures::LIC_RPCImportLicenseRequest(const Array& params)
{
    return Value::null;    
}

