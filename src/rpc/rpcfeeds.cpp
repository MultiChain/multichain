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

Value suspendfeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("suspendfeed API");
    
    return pEF->FED_RPCSuspendFeed(params);
}

Value deletefeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("deletefeed API");
    
    return pEF->FED_RPCDeleteFeed(params);
}

Value rescanfeed(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("rescanfeed API");
    
    return pEF->FED_RPCRescanFeed(params);
}

Value addfeedstreams(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("addfeedstreams API");
    
    return pEF->FED_RPCAddFeedStreams(params);
}

Value removefeedstreams(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("removefeedstreams API");
    
    return pEF->FED_RPCRemoveFeedStreams(params);
}

Value addfeedblocks(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("addfeedblocks API");
    
    return pEF->FED_RPCAddFeedBlocks(params);
}

Value removefeedblocks(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("removefeedblocks API");
    
    return pEF->FED_RPCRemoveFeedBlocks(params);
}

Value purgefeedfile(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("purgefeedfile API");
    
    return pEF->FED_RPCPurgeFeedFile(params);
}

Value listfeeds(const json_spirit::Array& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error("Help message not found\n");
    
    pEF->ENT_RPCVerifyEdition("listfeeds API");
       
    return pEF->FED_RPCListFeeds(params);
}


