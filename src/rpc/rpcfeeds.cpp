// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.


#include "rpc/rpcserver.h"
#include "json/json_spirit_ubjson.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"
#include "community/community.h"


Value createfeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("createfeed API");
    
    return pEF->FED_RPCCreateFeed(params);
}

Value deletefeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("deletefeed API");
    
    return pEF->FED_RPCDeleteFeed(params);
}

Value pausefeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("pausefeed API");
    
    return pEF->FED_RPCPauseFeed(params);
}

Value resumefeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("resumefeed API");
    
    return pEF->FED_RPCResumeFeed(params);
}

Value purgefeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("purgefeed API");
    
    return pEF->FED_RPCPurgeFeed(params);
}

Value listfeeds(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("listfeeds API");
       
    return pEF->FED_RPCListFeeds(params);
}

Value getdatarefdata(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("getdatarefdata API");
       
    return pEF->DRF_RPCGetDataRefData(params);
}


Value datareftobinarycache(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("datareftobinarycache API");
       
    return pEF->DRF_RPCDataRefToBinaryCache(params);
}

Value addtofeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("addtofeed API");
    
    return pEF->FED_RPCAddToFeed(params);
}

Value updatefeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 5)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("updatefeed API");
    
    return pEF->FED_RPCUpdateFeed(params);
}




