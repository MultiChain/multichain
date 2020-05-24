// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef RPCMULTICHAINUTILS_H
#define	RPCMULTICHAINUTILS_H

#include "primitives/transaction.h"
#include "core/init.h"
#include "core/main.h"
#include "wallet/keystore.h"
#include "rpc/rpcserver.h"
//#include "script/script.h"
//#include "script/standard.h"
#include "structs/uint256.h"

#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"
#include "multichain/multichain.h"
#include "utils/utilparse.h"
#include "wallet/chunkdb.h"
#include "filters/multichainfilter.h"

using namespace std;
using namespace json_spirit;
#include <map>
#include <string>

#define MC_ASSET_KEY_UNCONFIRMED_GENESIS    1
#define MC_ASSET_KEY_VALID                  0
#define MC_ASSET_KEY_INVALID_TXID          -1
#define MC_ASSET_KEY_INVALID_REF           -2
#define MC_ASSET_KEY_INVALID_NAME          -3
#define MC_ASSET_KEY_INVALID_SIZE          -4
#define MC_ASSET_KEY_INVALID_EMPTY         -5

#define MC_DATA_API_PARAM_TYPE_NONE            0x00000000
#define MC_DATA_API_PARAM_TYPE_CREATE_STREAM   0x00000001
#define MC_DATA_API_PARAM_TYPE_PUBLISH         0x00000002
#define MC_DATA_API_PARAM_TYPE_ISSUE           0x00000004
#define MC_DATA_API_PARAM_TYPE_FOLLOWON        0x00000008
#define MC_DATA_API_PARAM_TYPE_RESERVED1       0x00000010
#define MC_DATA_API_PARAM_TYPE_APPROVAL        0x00000020
#define MC_DATA_API_PARAM_TYPE_CREATE_UPGRADE  0x00000040
#define MC_DATA_API_PARAM_TYPE_CREATE_FILTER   0x00000080
#define MC_DATA_API_PARAM_TYPE_EMPTY_RAW       0x00000100
#define MC_DATA_API_PARAM_TYPE_RAW             0x00000200
#define MC_DATA_API_PARAM_TYPE_FORMATTED       0x00000400
#define MC_DATA_API_PARAM_TYPE_CIS             0x00001000
#define MC_DATA_API_PARAM_TYPE_CREATE_VAR      0x00010000
#define MC_DATA_API_PARAM_TYPE_UPDATE_VAR      0x00020000

#define MC_DATA_API_PARAM_TYPE_SIMPLE          0x00000602
#define MC_DATA_API_PARAM_TYPE_ALL             0xFFFFFFFF

#define MC_VMM_MERGE_OBJECTS                   0x00000001
#define MC_VMM_RECURSIVE                       0x00000002
#define MC_VMM_IGNORE_OTHER                    0x00000004
#define MC_VMM_TAKE_FIRST                      0x00000008
#define MC_VMM_TAKE_FIRST_FOR_FIELD            0x00000010
#define MC_VMM_OMIT_NULL                       0x00000020
#define MC_VMM_IGNORE_MISSING                  0x00000040

#define MC_RFD_OPTION_NONE                     0x00000000
#define MC_RFD_OPTION_INLINE                   0x00000001
#define MC_RFD_OPTION_OFFCHAIN                 0x00000002
#define MC_RFD_OPTION_CACHE                    0x00000004
#define MC_RFD_OPTION_SALTED                   0x00000008

#define MC_OST_UNDEFINED                       0x00000000
#define MC_OST_UNKNOWN                         0x00000001
#define MC_OST_ON_CHAIN                        0x00000002
#define MC_OST_OFF_CHAIN                       0x00000003
#define MC_OST_STORAGE_MASK                    0x000000FF                  
#define MC_OST_RETRIEVED                       0x00000100
#define MC_OST_STATUS_MASK                     0x0000FF00                  
#define MC_OST_ERROR_SCRIPT                    0x00010000
#define MC_OST_ERROR_WRONG_SIZES               0x00020000
#define MC_OST_ERROR_CORRUPTED                 0x00030000
#define MC_OST_ERROR_NOT_SUPPORTED             0x00040000
#define MC_OST_ERROR_MASK                      0x00FF0000
#define MC_OST_CONTROL_NO_DATA                 0x01000000

#define MC_AST_MAX_NOT_EXTENDED_VARIABLE_SIZE   32767


// codes for allowed_objects fields    
// 0x0001 - create    
// 0x0002 - publish    
// 0x0004 - issue
// 0x0008 - follow-on
// 0x0010 - pure details
// 0x0020 - approval
// 0x0040 - create upgrade
// 0x0100 - encode empty hex
// 0x0200 - cache input script



//string HelpRequiringPassphrase();
string AllowedPermissions();
string AllowedPausedServices();
uint32_t GetPausedServices(const char *str);



CScript RemoveOpDropsIfNeeded(const CScript& scriptInput);
bool AssetRefDecode(unsigned char *bin, const char* string, const size_t stringLen);
int ParseAssetKey(const char* asset_key,unsigned char *txid,unsigned char *asset_ref,char *name,int *multiple,int *type,int entity_type);
int ParseAssetKeyToFullAssetRef(const char* asset_key,unsigned char *full_asset_ref,int *multiple,int *type,int entity_type);
Array AddressEntries(const CTxIn& txin,txnouttype& typeRet,mc_Script *lpScript);
Array AddressEntries(const CTxOut& txout,txnouttype& typeRet);
Value PermissionForFieldEntry(mc_EntityDetails *lpEntity);
Array PerOutputDataEntries(const CTxOut& txout,mc_Script *lpScript,uint256 txid,int vout);
Array PermissionEntries(const CTxOut& txout,mc_Script *lpScript,bool fLong);
Object StreamEntry(const unsigned char *txid,uint32_t output_level);
Object StreamEntry(const unsigned char *txid,uint32_t output_level,mc_EntityDetails *raw_entity);
Object UpgradeEntry(const unsigned char *txid);
const unsigned char *GetChunkDataInRange(int64_t *out_size,unsigned char* hashes,int chunk_count,int64_t start,int64_t count,int fHan);
uint32_t GetFormattedData(mc_Script *lpScript,const unsigned char **elem,int64_t *out_size,unsigned char* hashes,int chunk_count,int64_t total_size,int max_shown=-1);
Value OpReturnEntry(const unsigned char *elem,size_t elem_size,uint256 txid, int vout);
Value OpReturnFormatEntry(const unsigned char *elem,int64_t elem_size,uint256 txid, int vout, uint32_t format, string *format_text_out,uint32_t status);
Value OpReturnFormatEntry(const unsigned char *elem,size_t elem_size,uint256 txid, int vout, uint32_t format, string *format_text_out);
Value OpReturnFormatEntry(const unsigned char *elem,size_t elem_size,uint256 txid, int vout, uint32_t format);
Value DataItemEntry(const CTransaction& tx,int n,set <uint256>& already_seen,uint32_t stream_output_level);
Object FilterEntry(const unsigned char *txid,uint32_t output_level,uint32_t filter_type);
Object AssetEntry(const unsigned char *txid,int64_t quantity,uint32_t output_level);
Object VariableEntry(const unsigned char *txid,uint32_t output_level);
Array VariableHistory(mc_EntityDetails *last_entity,int count,int start,uint32_t output_level);
string ParseRawOutputObject(Value param,CAmount& nAmount,mc_Script *lpScript,int *eErrorCode);
bool FindPreparedTxOut(CTxOut& txout,COutPoint outpoint,string& reason);
bool GetTxInputsAsTxOuts(const CTransaction& tx, vector <CTxOut>& inputs, vector <string>& errors,string& reason);
CScript GetScriptForString(string source,uint32_t entity_type,mc_EntityDetails *entity);
vector <pair<CScript, CAmount> > ParseRawOutputMultiObject(Object sendTo,int *required);
CScript ParseRawMetadata(Value param,uint32_t allowed_objects,mc_EntityDetails *given_entity,mc_EntityDetails *found_entity);
vector<string> ParseStringList(Value param);
void ParseEntityIdentifier(Value entity_identifier,mc_EntityDetails *entity,uint32_t entity_type);
bool AssetCompareByRef(Value a,Value b);
Array AssetArrayFromAmounts(mc_Buffer *asset_amounts,int issue_asset_id,uint256 hash,int show_type);
void ParseRawAction(string action,bool& lock_it, bool& sign_it,bool& send_it);
bool paramtobool(Value param);
bool paramtobool(Value param,bool strict);
int paramtoint(Value param,bool check_for_min,int min_value,string error_message);
int64_t paramtoint64(Value param,bool check_for_min,int64_t min_value,string error_message);
int ParseBlockIdentifier(Value blockset_identifier);
int ParseRescanParameter(Value rescan_identifier, bool *fRescan);
vector<int> ParseBlockSetIdentifier(Value blockset_identifier);
vector<unsigned char> ParseRawFormattedData(const Value *value,uint32_t *data_format,mc_Script *lpDetailsScript,uint32_t in_options,uint32_t *out_options,int *errorCode,string *strError);
void ParseRawDetails(const Value *value,mc_Script *lpDetails,mc_Script *lpDetailsScript,int *errorCode,string *strError);
void ParseRawValue(const Value *value,mc_Script *lpDetails,mc_Script *lpDetailsScript,size_t *max_size,int *errorCode,string *strError);
bool mc_IsJsonObjectForMerge(const Value *value,int level);
Value mc_MergeValues(const Value *value1,const Value *value2,uint32_t mode,int level,int *error);
Value mc_ExtractDetailsJSONObject(const unsigned char *script,uint32_t total);
Value mc_ExtractValueJSONObject(mc_EntityDetails *lpEnt);
int mc_GetEntityIndex(mc_EntityDetails *lpEnt);
void AppendOffChainFormatData(uint32_t data_format,uint32_t out_options,mc_Script *lpDetailsScript,vector<unsigned char>& vValue,vector<uint256>* vChunkHashes,int *errorCode,string *strError);
int mc_BinaryCacheFile(string id,int mode);
void mc_RemoveBinaryCacheFile(string id);
bool AvailableFromStatus(uint32_t status);
string OffChainError(uint32_t status,int *errorCode); 
bool RawDataParseRestrictParameter(const Value& param,uint32_t *restrict,uint32_t *permissions,int *errorCode,string *strError);
void CheckWalletError(int err,uint32_t entity_type,string message);


#endif	/* RPCMULTICHAINUTILS_H */

