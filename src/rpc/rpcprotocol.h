// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef BITCOIN_RPCPROTOCOL_H
#define BITCOIN_RPCPROTOCOL_H

#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <boost/iostreams/concepts.hpp>
#include <boost/iostreams/stream.hpp>

#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_writer_template.h"

//! HTTP status codes
enum HTTPStatusCode
{
    HTTP_OK                    = 200,
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE   = 503,
};

//! Bitcoin RPC error codes
enum RPCErrorCode
{
    //! Standard JSON-RPC 2.0 errors
    RPC_INVALID_REQUEST  = -32600,
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS   = -32602,
    RPC_INTERNAL_ERROR   = -32603,
    RPC_PARSE_ERROR      = -32700,

    //! General application defined errors
    RPC_MISC_ERROR                  = -1,  //! std::exception thrown in command handling
    RPC_FORBIDDEN_BY_SAFE_MODE      = -2,  //! Server is in safe mode, and command is not allowed in safe mode
    RPC_TYPE_ERROR                  = -3,  //! Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY      = -5,  //! Invalid address or key
    RPC_OUT_OF_MEMORY               = -7,  //! Ran out of memory during operation
    RPC_INVALID_PARAMETER           = -8,  //! Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR              = -20, //! Database error
    RPC_DESERIALIZATION_ERROR       = -22, //! Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR                = -25, //! General error during transaction or block submission
    RPC_VERIFY_REJECTED             = -26, //! Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN     = -27, //! Transaction already in chain
    RPC_IN_WARMUP                   = -28, //! Client still warming up

    //! Aliases for backward compatibility
    RPC_TRANSACTION_ERROR           = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED        = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN= RPC_VERIFY_ALREADY_IN_CHAIN,

    //! P2P client errors
    RPC_CLIENT_NOT_CONNECTED        = -9,  //! Bitcoin is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD  = -10, //! Still downloading initial blocks
    RPC_CLIENT_NODE_ALREADY_ADDED   = -23, //! Node is already added
    RPC_CLIENT_NODE_NOT_ADDED       = -24, //! Node has not been added before

    //! Wallet errors
    RPC_WALLET_ERROR                = -4,  //! Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS   = -6,  //! Not enough funds in wallet or account
    RPC_WALLET_INVALID_ACCOUNT_NAME = -11, //! Invalid account name
    RPC_WALLET_KEYPOOL_RAN_OUT      = -12, //! Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED        = -13, //! Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -14, //! The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE      = -15, //! Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED    = -16, //! Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED     = -17, //! Wallet is already unlocked
    
    //! MultiChain specific errors
    
    RPC_NOT_ALLOWED                 = -701,  //! Given action is not allowed in current database state 
    RPC_NOT_SUPPORTED               = -702,  //! API not supported
    RPC_NOT_SUBSCRIBED              = -703,  //! Not subscribed to asset/stream
    RPC_INSUFFICIENT_PERMISSIONS    = -704,  //! Insufficient permission
    RPC_DUPLICATE_NAME              = -705,  //! Entity with this name already found 
    RPC_UNCONFIRMED_ENTITY          = -706,  //! Unconfirmed entity
    RPC_EXCHANGE_ERROR              = -707,  //! Invalid exchange or error in one of exchange components
    RPC_ENTITY_NOT_FOUND            = -708,  //! Entity with specified identifier not found
    RPC_WALLET_ADDRESS_NOT_FOUND    = -709,  //! Address not found in the wallet 
    RPC_TX_NOT_FOUND                = -710,  //! tx not found
    RPC_BLOCK_NOT_FOUND             = -711,  //! block not found
    RPC_OUTPUT_NOT_FOUND            = -712,  //! Output not found in UTXO database and mempool
    RPC_OUTPUT_NOT_DATA             = -713,  //! Output doesn't contain data
    RPC_INPUTS_NOT_MINE             = -714,  //! Inputs in transaction don't belong to this wallet 
    RPC_WALLET_OUTPUT_NOT_FOUND     = -715,  //! Output not found in the wallet
    RPC_WALLET_NO_UNSPENT_OUTPUTS   = -716,  //! No unspent outputs in this wallet
    RPC_GENERAL_FILE_ERROR          = -717,  //! General file error
    RPC_UPGRADE_REQUIRED            = -718,  //! Upgrade required
    
};

std::string HTTPPost(const std::string& strMsg, const std::map<std::string,std::string>& mapRequestHeaders);
std::string HTTPError(int nStatus, bool keepalive,
                      bool headerOnly = false);
std::string HTTPReplyHeader(int nStatus, bool keepalive, size_t contentLength,
                      const char *contentType = "application/json");
std::string HTTPReply(int nStatus, const std::string& strMsg, bool keepalive,
                      bool headerOnly = false,
                      const char *contentType = "application/json");
bool ReadHTTPRequestLine(std::basic_istream<char>& stream, int &proto,
                         std::string& http_method, std::string& http_uri);
int ReadHTTPStatus(std::basic_istream<char>& stream, int &proto);
int ReadHTTPHeaders(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet);
int ReadHTTPMessage(std::basic_istream<char>& stream, std::map<std::string, std::string>& mapHeadersRet,
                    std::string& strMessageRet, int nProto, size_t max_size);
std::string JSONRPCRequest(const std::string& strMethod, const json_spirit::Array& params, const json_spirit::Value& id);
json_spirit::Object JSONRPCReplyObj(const json_spirit::Value& result, const json_spirit::Value& error, const json_spirit::Value& id);
std::string JSONRPCReply(const json_spirit::Value& result, const json_spirit::Value& error, const json_spirit::Value& id);
json_spirit::Object JSONRPCError(int code, const std::string& message);

#endif // BITCOIN_RPCPROTOCOL_H
