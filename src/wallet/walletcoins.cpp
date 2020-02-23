// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2014-2016 The Bitcoin Core developers
// Original code was distributed under the MIT software license.
// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "wallet/wallet.h"
#include "wallet/wallettxs.h"
#include "utils/utilparse.h"
#include "coincontrol.h"
#include "script/sign.h"
#include "utils/utilmoneystr.h"
#include "rpc/rpcprotocol.h"
#include "custom/custom.h"

extern mc_WalletTxs* pwalletTxsMain;
void MultiChainTransaction_SetTmpOutputScript(const CScript& script1);
int64_t MultiChainTransaction_OffchainFee(int64_t total_offchain_size);
void AppendSpecialRowsToBuffer(mc_Buffer *amounts,uint256 hash,int expected_allowed,int expected_required,int *allowed,int *required,CAmount nValue);

using namespace std;

bool debug_print=false;
bool csperf_debug_print=false;

void CAssetGroupTree::Clear()
{
    nAssetsPerGroup=0;
    nMaxAssetsPerGroup=0;
    nOptimalGroupCount=0;
    nMode=0;
    nSingleAssetGroupCount=0;
    lpAssets=NULL;
    lpAssetGroups=NULL;
    lpTmpGroupBuffer=NULL;
}

void CAssetGroupTree::Destroy()
{
    if(lpAssets)
    {
        delete lpAssets; 
    }
    if(lpAssetGroups)
    {
        delete lpAssetGroups; 
    }
    if(lpTmpGroupBuffer)
    {
        mc_Delete(lpTmpGroupBuffer);
    }
    Clear();
}

void CAssetGroupTree::Dump()
{
    CAssetGroup *thisGroup;
    int *aptr;
    unsigned char *assetrefbin;
    int i,j,s;
    
    if(debug_print)printf("Asset Grouping. Assets: %d. Group Size: %d. Group Count: %d. Single-Asset Groups: %d.\n",
            lpAssets->GetCount(),nAssetsPerGroup,lpAssetGroups->GetCount()-1,nSingleAssetGroupCount);
    if(fDebug)LogPrint("mchn","mchn: Asset Grouping. Assets: %d. Group Size: %d. Group Count: %d. Single-Asset Groups: %d.\n",
            lpAssets->GetCount(),nAssetsPerGroup,lpAssetGroups->GetCount()-1,nSingleAssetGroupCount);
    for(i=1;i<lpAssetGroups->GetCount();i++)                                    
    {    
        thisGroup=(CAssetGroup*)lpAssetGroups->GetRow(i);
        if(debug_print)
        {
            printf("Group: %4d. Asset Count: %d\n",i,thisGroup->nSize);
            aptr=(int*)(lpAssetGroups->GetRow(i)+sizeof(CAssetGroup));
            s=thisGroup->nSize;
            if(s<0)
            {
                s=1;
            }
            for(j=0;j<s;j++)
            {
                assetrefbin=(unsigned char*)lpAssets->GetRow(aptr[j]);
                
                string assetref="";
                if(mc_GetABRefType(assetrefbin) == MC_AST_ASSET_REF_TYPE_SHORT_TXID)
                {
                    for(int i=0;i<8;i++)
                    {
                        assetref += strprintf("%02x",assetrefbin[MC_AST_SHORT_TXID_OFFSET+MC_AST_SHORT_TXID_SIZE-i-1]);
                    }
                }
                else
                {
                    assetref += itostr((int)mc_GetLE(assetrefbin,4));
                    assetref += "-";
                    assetref += itostr((int)mc_GetLE(assetrefbin+4,4));
                    assetref += "-";
                    assetref += itostr((int)mc_GetLE(assetrefbin+8,2));
                }
                printf("              Asset: %d: %s\n",j+1,assetref.c_str());
            }
        }
    }
    
}

/*
 * Resizes asset group set to cointain given number of new assets
 */

int CAssetGroupTree::Resize(int newAssets)
{
    int err;
    int n,i;
    int last_full;
    CAssetGroup *thisGroup;
    
    n=nAssetsPerGroup;
    while(nOptimalGroupCount*n < lpAssets->GetCount()+newAssets-nSingleAssetGroupCount)
    {
        n*=2;
    }
    
    if(n == nAssetsPerGroup)                                                    // Current value is OK
    {
        return MC_ERR_NOERROR;    
    }
    if(n > nMaxAssetsPerGroup)                                                  // We cannot increase assets-per-group, we have no choice but increase number of groups
    {
        return MC_ERR_NOERROR;    
    }

    if(debug_print)printf("Asset grouping resize %d -> %d\n",nAssetsPerGroup,n);
    if(fDebug)LogPrint("mchn","Asset grouping resize %d -> %d\n",nAssetsPerGroup,n);
    
    mc_Buffer *new_asset_groups_buffer;
    
    new_asset_groups_buffer=new mc_Buffer;
    
    err=new_asset_groups_buffer->Initialize(sizeof(CAssetGroup),sizeof(CAssetGroup)+n*sizeof(int),MC_BUF_MODE_DEFAULT);
    if(err)
    {
        return err;
    }
                                                                                // Old assets stay stay in the same group, no change in lpAssets is needed
                                                                                // But group size is increased, group are partially filled
                   
    new_asset_groups_buffer->Realloc(lpAssetGroups->GetCount());
    new_asset_groups_buffer->SetCount(lpAssetGroups->GetCount());
    for(i=0;i<lpAssetGroups->GetCount();i++)                                    
    {
        memset(new_asset_groups_buffer->GetRow(i),0,sizeof(CAssetGroup)+n*sizeof(int));
        memcpy(new_asset_groups_buffer->GetRow(i),lpAssetGroups->GetRow(i),sizeof(CAssetGroup)+nAssetsPerGroup*sizeof(int));
    }

    last_full=0;                                                                // Restore list of previously full groups
    for(i=0;i<lpAssetGroups->GetCount();i++)                                    
    {    
        thisGroup=(CAssetGroup*)lpAssetGroups->GetRow(i);
        if(thisGroup->nSize == nAssetsPerGroup)
        {
            thisGroup->nNextGroup=last_full;
            last_full=thisGroup->nThisGroup;
        }        
    }
    
    *((int*)(new_asset_groups_buffer->GetRow(0)+sizeof(CAssetGroup)) + nAssetsPerGroup)=last_full;    
    
    nAssetsPerGroup=n;
    
    delete lpAssetGroups;
    lpAssetGroups=new_asset_groups_buffer;
            
    return MC_ERR_NOERROR;    
}

int CAssetGroupTree::Initialize(int assetsPerGroup,int maxAssetsPerGroup,int optimalGroupCount,int mode)
{
    int err;
    
    Destroy();
    
    if(assetsPerGroup == 0)
    {
        return MC_ERR_NOERROR;
    }
    
    nAssetsPerGroup=assetsPerGroup;
    nMaxAssetsPerGroup=maxAssetsPerGroup;
    nOptimalGroupCount=optimalGroupCount;
    nMode=mode;
    
    lpAssets=new mc_Buffer;
    err=lpAssets->Initialize(MC_AST_ASSET_QUANTITY_OFFSET,MC_AST_ASSET_QUANTITY_OFFSET+sizeof(int),MC_BUF_MODE_MAP);
    
    if(err)
    {
        return err;
    }
    
                                                                                // key - asset group header
                                                                                // value - list of asset ids in the group
    
    lpAssetGroups=new mc_Buffer;
    err=lpAssetGroups->Initialize(sizeof(CAssetGroup),sizeof(CAssetGroup)+assetsPerGroup*sizeof(int),MC_BUF_MODE_DEFAULT);
    
    if(err)
    {
        return err;
    }
    
                                                                                // Group id is 1-based.
                                                                                // 0-row is used for storing id of the group with given number of assets 0-(assetsPerGroup-1)
                                                                                // All groups with identical number of assets are linked in the list using nNextGroup field
    
    err=lpAssetGroups->Realloc(1);
    if(err)
    {
        return err;
    }
    
    lpAssetGroups->SetCount(1);
    memset(lpAssetGroups->GetRow(0),0,sizeof(CAssetGroup)+assetsPerGroup*sizeof(int));
    
    lpTmpGroupBuffer=(int*)mc_New(maxAssetsPerGroup*sizeof(int));
    if(lpTmpGroupBuffer == NULL)
    {
        return MC_ERR_ALLOCATION;
    }
    
    return MC_ERR_NOERROR;
}
                       
/* 
 * Finds asset group big enough to contain given number of new assets.
 */

CAssetGroup *CAssetGroupTree::FindAndShiftBestGroup(int assets)
{
    if(assets > nAssetsPerGroup)                                                // Too many assets - cannot find single group
    {
        return NULL;
    }
    
    int max_underfilled_size,i;
    int *iptr;
    
    iptr=(int*)(lpAssetGroups->GetRow(0)+sizeof(CAssetGroup));
    
    max_underfilled_size=-1;                                                    // Find group with maximal size, having free space for required asset number
    i=nAssetsPerGroup-assets;
    while( (i >= 0) && (max_underfilled_size == -1))
    {
        if(iptr[i])
        {
            max_underfilled_size=i;
        }
        i--;
    }
    
    CAssetGroup *thisGroup;
    
    if(max_underfilled_size < 0)                                                // Not found - add new group
    {
        CAssetGroup assetGroup;
        assetGroup.nThisGroup=lpAssetGroups->GetCount();
        assetGroup.nNextGroup=0;
        assetGroup.nSize=0;
        memset(lpTmpGroupBuffer,0,nAssetsPerGroup*sizeof(int));
        iptr[0]=assetGroup.nThisGroup;                                          // There is no group with 0 assets, this is the first
        if(lpAssetGroups->Add(&assetGroup,lpTmpGroupBuffer))
        {
            return NULL;
        }
        max_underfilled_size=0;
    }
    
    thisGroup=(CAssetGroup*)lpAssetGroups->GetRow(iptr[max_underfilled_size]);
    
    iptr[max_underfilled_size]=thisGroup->nNextGroup;                           // Pop up found group from the list
    
    if(max_underfilled_size+assets<nAssetsPerGroup)                             // There is still free space in the group even after adding new assets
    {
        thisGroup->nNextGroup=iptr[max_underfilled_size+assets];                // Pushing it to new location (previous value is linked to the new group)
        iptr[max_underfilled_size+assets]=thisGroup->nThisGroup;
    }
    else
    {
        thisGroup->nNextGroup=0;                                                // The group is full
    }
    
    return thisGroup;    
}

int CAssetGroupTree::GroupCount()
{
    return lpAssetGroups->GetCount();
}

/*
 * Returns group id of list of assets if all belong to the same group or error
 * Adds new assets to the groups if addIfNeeded==1
 */

int CAssetGroupTree::GetGroup(mc_Buffer* assets, int addIfNeeded)
{
    if(nAssetsPerGroup == 0)                                                    // No grouping
    {
        return -1;
    }

    int group_id,last_asset_count,new_asset_count,only_asset;
    int i,g;
    int *iptr;
    int *aptr;
    unsigned char *assetRef;
    CAssetGroup *thisGroup;
    
    only_asset=-1;
    group_id=-2;                                                                // No assets in this buffer
    last_asset_count=lpAssets->GetCount();
    for(i=0;i<assets->GetCount();i++)
    {
        assetRef=assets->GetRow(i);
//        if(mc_GetLE(assetRef+MC_AST_ASSET_REF_TYPE_OFFSET,MC_AST_ASSET_REF_TYPE_SIZE) != MC_AST_ASSET_REF_TYPE_SPECIAL)// Special rows are ignored - there is no real assets with asset-ref block=0
        if( (mc_GetABRefType(assetRef) != MC_AST_ASSET_REF_TYPE_SPECIAL) &&
            (mc_GetABRefType(assetRef) != MC_AST_ASSET_REF_TYPE_GENESIS) )                    
//        if(mc_GetLE(assetRef,4) > 0)                             
        {
            if(only_asset == -1)
            {
                only_asset=i;
            }
            else
            {
                only_asset=-2;                                                  // More than one asset
            }
            g=GetGroup(assetRef,0);
            if(g>0)
            {
                if(group_id != -2)
                {
                    if(g != group_id)
                    {
                        group_id = -3;                                          // Assets belongs to different groups
                    }
                }
                else
                {
                    group_id=g;
                }
            }
            else
            {
                if(lpAssets->Add(assetRef,&g))
                {
                    lpAssets->SetCount(last_asset_count);
                    return -1;                                                  // Internal error
                }                
            }
        }        
    }
    
    if(last_asset_count == lpAssets->GetCount())
    {
        return group_id;                                                        // No new assets
    }

                                                                                // We have new assets
    new_asset_count=lpAssets->GetCount();                                       
            
    if(group_id == -2)
    {
        group_id=0;                                                             // Don't belong to any of the groups
    }
    
    if(!addIfNeeded)
    {
        lpAssets->SetCount(last_asset_count);
        return 0;
    }
    
    thisGroup=NULL;
    
    if(only_asset >= 0)                                                         // Assets which cannot be combined with other
    {
        thisGroup=AddSingleAssetGroup(assets->GetRow(only_asset));
        if(thisGroup)
        {
            return thisGroup->nThisGroup;            
        }
    }
    
//    Resize(new_asset_count - last_asset_count);
    Resize(0);                                                                  // Assets already added

    if(group_id > 0)                                                                // There are old assets, so we know what should be group id
    {
        thisGroup=(CAssetGroup*)lpAssetGroups->GetRow(group_id);
        if(thisGroup->nSize + (new_asset_count - last_asset_count) <= nAssetsPerGroup)// There is large enough space in the group
        {
            iptr=(int*)(lpAssetGroups->GetRow(0)+sizeof(CAssetGroup));
            iptr[thisGroup->nSize]=thisGroup->nNextGroup;                       // Pop found group from the list
            if(thisGroup->nSize+(new_asset_count - last_asset_count)<nAssetsPerGroup)// There is still free space in the group
            {
                thisGroup->nNextGroup=iptr[thisGroup->nSize+(new_asset_count - last_asset_count)];// Connection other groups with this size
                iptr[thisGroup->nSize+(new_asset_count - last_asset_count)]=thisGroup->nThisGroup;// Push found group into new list
            }
            else
            {
                thisGroup->nNextGroup=0;
            }
        }
        else
        {
            thisGroup=NULL;                                                     // Not enough space - we have to put new assets in different groups one by one
        }
    }
    else
    {
        thisGroup=FindAndShiftBestGroup(new_asset_count - last_asset_count);    // Try to put assets in the same group
    }
       
                                                                                
    if(thisGroup)                                                               // We have group with enough free space
                                                                                // Group is already in the right place in the tree, but size is not updated yet 
    {
        for(i=last_asset_count;i<new_asset_count;i++)
        {
            *(int*)(lpAssets->GetRow(i)+MC_AST_ASSET_QUANTITY_OFFSET)=thisGroup->nThisGroup;
        }
        aptr=(int*)(lpAssetGroups->GetRow(thisGroup->nThisGroup)+sizeof(CAssetGroup));
        for(i=last_asset_count;i<new_asset_count;i++)
        {
            aptr[thisGroup->nSize]=i;
            thisGroup->nSize++;
            *(int*)(lpAssets->GetRow(i)+MC_AST_ASSET_QUANTITY_OFFSET)=thisGroup->nThisGroup;
        }                    
        if(group_id < 0)                                                        // Though we were able to put new assets in new group, if old assets belong to different group
                                                                                // we should return -3
        {
            return group_id;                                        
        }
        return thisGroup->nThisGroup;
    }
    
    lpAssets->SetCount(last_asset_count);                                       // Rewinding assets list
    for(i=last_asset_count;i<new_asset_count;i++)                               // Adding assets one by one
    {
        g=GetGroup(lpAssets->GetRow(i),1);                                      // lpAsset buffer is untouched in rewind
        if(g<0)
        {
            return g;                                                           // Return in case of error
        }
    }                
    
    return 0;
}


/*
 * Returns group id of the asset 
 * Adds single assets
 */

CAssetGroup *CAssetGroupTree::AddSingleAssetGroup(unsigned char *assetRef)
{
    mc_EntityDetails entity;
    int group_id,asset_id;
    int *aptr;
    
    if(mc_gState->m_Assets->FindEntityByFullRef(&entity,assetRef))
    {
        if( entity.Permissions() & (MC_PTP_SEND | MC_PTP_RECEIVE) )
        {
            asset_id=lpAssets->GetCount()-1;
            group_id=lpAssetGroups->GetCount();
            CAssetGroup assetGroup;
            assetGroup.nThisGroup=group_id;
            assetGroup.nNextGroup=0;
            assetGroup.nSize=-1;
            memset(lpTmpGroupBuffer,0,nAssetsPerGroup*sizeof(int));
            if(lpAssetGroups->Add(&assetGroup,lpTmpGroupBuffer))
            {
                return NULL;
            }
            *(int*)(lpAssets->GetRow(asset_id)+MC_AST_ASSET_QUANTITY_OFFSET)=group_id;
            aptr=(int*)(lpAssetGroups->GetRow(group_id)+sizeof(CAssetGroup));
            aptr[0]=asset_id;
            nSingleAssetGroupCount++;
            return (CAssetGroup *)(lpAssetGroups->GetRow(group_id));
        }
    }
    
    return NULL;
}


/*
 * Returns group id of the asset 
 * Adds to the groups if addIfNeeded==1
 */

int CAssetGroupTree::GetGroup(unsigned char *assetRef,int addIfNeeded)
{
    if(nAssetsPerGroup == 0)
    {
        return -1;
    }
    
    int row,group_id,asset_id;
    int *aptr;
    
    row=lpAssets->Seek(assetRef);
    
    if(row >= 0)                                                                // Found
    {
        return *(int*)(lpAssets->GetRow(row)+MC_AST_ASSET_QUANTITY_OFFSET);
    }
    
    if(!addIfNeeded)
    {
        return 0;
    }
    
    Resize(1);                                                                  // New asset
    
    CAssetGroup *thisGroup;
       
    asset_id=lpAssets->GetCount();
    group_id=0;
    if(lpAssets->Add(assetRef,&group_id))
    {
        return -1;
    }
    
    thisGroup=FindAndShiftBestGroup(1);                                         // Find a group with at least one free space
    if(thisGroup == NULL)
    {
        return -1;
    }
    
    *(int*)(lpAssets->GetRow(asset_id)+MC_AST_ASSET_QUANTITY_OFFSET)=thisGroup->nThisGroup;        
    aptr=(int*)(lpAssetGroups->GetRow(thisGroup->nThisGroup)+sizeof(CAssetGroup));
    aptr[thisGroup->nSize]=asset_id;
    thisGroup->nSize++;
    
    return group_id;
} 

int64_t mc_GetABCoinQuantity(void *ptr,int coin_id)
{
    return (int64_t)mc_GetLE((unsigned char*)ptr+MC_AST_ASSET_QUANTITY_OFFSET+coin_id*MC_AST_ASSET_QUANTITY_SIZE,MC_AST_ASSET_QUANTITY_SIZE);        
}

void mc_SetABCoinQuantity(void *ptr,int coin_id,int64_t quantity)
{
    mc_PutLE((unsigned char*)ptr+MC_AST_ASSET_QUANTITY_OFFSET+coin_id*MC_AST_ASSET_QUANTITY_SIZE,&quantity,MC_AST_ASSET_QUANTITY_SIZE);        
}



void DebugPrintAssetTxOut(uint256 hash,int index,unsigned char* assetrefbin,int64_t quantity)
{
    string txid=hash.GetHex();
    
    if(debug_print)
    {
        printf("TxOut: %s-%d ",txid.c_str(),index);        
        if(mc_GetABRefType(assetrefbin) == MC_AST_ASSET_REF_TYPE_SPECIAL)
        {
            printf("Special:        %08x%08x",(uint32_t)mc_GetLE(assetrefbin,4),(uint32_t)mc_GetLE(assetrefbin+4,4));
        }
        else
        {
            for(int i=MC_AST_SHORT_TXID_OFFSET+MC_AST_SHORT_TXID_SIZE-1;i>=MC_AST_SHORT_TXID_OFFSET;i--)
            {
                printf("%02x",assetrefbin[i]);
            }
        }
        printf(" %ld\n",quantity);        
    }
}



struct CompareCOutputByDepthAndScriptSizeDesc
{
    bool operator()(const COutput& t1,
                    const COutput& t2) const
    {
        if(t1.tx)
        {
            if(t1.tx->vout[t1.i].scriptPubKey.size() == t2.tx->vout[t2.i].scriptPubKey.size())
            {
                return t1.nDepth > t2.nDepth;            
            }
            return t1.tx->vout[t1.i].scriptPubKey.size() > t2.tx->vout[t2.i].scriptPubKey.size();
        }
        if(t1.coin.m_TXOut.scriptPubKey.size() == t2.coin.m_TXOut.scriptPubKey.size())
        {
            return t1.nDepth > t2.nDepth;            
        }
        return t1.coin.m_TXOut.scriptPubKey.size() > t2.coin.m_TXOut.scriptPubKey.size();
//        return t1.nDepth > t2.nDepth;
    }
};


std::string COutput::ToString() const
{
    return strprintf("COutput(%s, %d, %d) [%s]", tx->GetHash().ToString(), i, nDepth, FormatMoney(tx->vout[i].nValue));
}


std::string CExchangeStatus::ToString() const
{
    return strprintf("CExchangeStatus(lock: %d, time: %d, hash: %s", nLockStatus, nTimestamp, nOfferHash.GetHex().c_str());
}



/*
 * Return unspent UTXOs belonging to the specified address, or all addresses of addresses==NULL
 */

void AvalableCoinsForAddress(CWallet *lpWallet,vector<COutput>& vCoins, const CCoinControl* coinControl,const set<CTxDestination>* addresses,uint32_t flags)
{
    double start_time=mc_TimeNowAsDouble();
    double last_time,this_time;
    last_time=start_time;
    uint160 addr=0;
    
    if(addresses)
    {    
        if(addresses->size() == 1)
        {
            set<CTxDestination>::const_iterator it = addresses->begin();
            CTxDestination addressRet=*it;        
            const CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
            const CScriptID *lpScriptID=boost::get<CScriptID> (&addressRet);
            if(lpKeyID)
            {
                addr=*(uint160*)lpKeyID;
            }
            if(lpScriptID)
            {
                addr=*(uint160*)lpScriptID;
            }
        }
    }
    
    lpWallet->AvailableCoins(vCoins, true, coinControl,true,true,addr,NULL,flags);
    this_time=mc_TimeNowAsDouble();    
    if(fDebug)LogPrint("mcperf","mcperf: AvailableCoins: Time: %8.6f \n", this_time-last_time);
    last_time=this_time;
    
    if(addresses == NULL)
    {
        return;
    }
    
    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                            // We already processed from-address
    {
        if(addresses->size() == 1)
        {
            return;
        }        
    }
    
    vector<COutput> vFilteredCoins;

    BOOST_FOREACH(const COutput& out, vCoins)
    {
        CTxOut txout;
        out.GetHashAndTxOut(txout);
        const CScript& script1 = txout.scriptPubKey;        
        CTxDestination addressRet;        
        if(ExtractDestinationScriptValid(script1, addressRet))
        {
            const CKeyID *KeyID=boost::get<CKeyID> (&addressRet);
            if(KeyID)
            {
                if(addresses)
                {
                    if(addresses->count(addressRet))
                    {
                        vFilteredCoins.push_back(out);
                    }
                }
                else
                {
                    vFilteredCoins.push_back(out);
                }
            }                
        }                        
    }    
    
    vCoins=vFilteredCoins;
    this_time=mc_TimeNowAsDouble();    
    if(fDebug)LogPrint("mcperf","mcperf: Address Filtering: Time: %8.6f \n", this_time-last_time);
    last_time=this_time;
}

/*
 * Updates input asset matrix
 */

bool InsertCoinIntoMatrix(int coin_id,                                          // IN  coin id in the vCoins array
                          uint256 hash,int out_i,                               // IN  txid/vout of the UTXO
                          mc_Buffer *tmp_amounts,                               // IN  Asset values of the UTXO
                          mc_Buffer *out_amounts,                               // IN  Only assets found in this buffer will be added, if NULL - all assets
                          mc_Buffer *in_amounts,                                // OUT Buffer to fill, rows -assets, columns - coins
                          mc_Buffer *in_map,                                    // OUT txid/vout->coin id map (used in coin selection)
                          unsigned char *in_row,int in_size,                    // TMP temporary buffer row and its size
                          int *in_special_row,                                  // IN  Coordinates of special rows
                          int64_t pure_native)                                  // IN  Pure native currency flag
{
    unsigned char buf_map[32+4+4];
    int row,err;
    int64_t quantity;
    
    
    memcpy(buf_map,&hash,32);                                                   // Updating txid/vout->coin id map
    mc_PutLE(buf_map+32,&out_i,4);
    mc_PutLE(buf_map+36,&coin_id,4);
    in_map->Add(buf_map,buf_map+36);

    for(int i=0;i<tmp_amounts->GetCount();i++)                                  // Inserting asset amounts into the matrix
    {
        quantity=mc_GetABQuantity(tmp_amounts->GetRow(i));
        if( (out_amounts == NULL) || (out_amounts->Seek(tmp_amounts->GetRow(i)) >= 0) )// Only assets found in out_amounts if specified
        {
            row=in_amounts->Seek(tmp_amounts->GetRow(i));
            if(row < 0)                                                         // New asset
            {
                memset(in_row,0,in_size);
                memcpy(in_row,tmp_amounts->GetRow(i),MC_AST_ASSET_QUANTITY_OFFSET);
                mc_SetABCoinQuantity(in_row,coin_id,quantity);
                err=in_amounts->Add(in_row);
                if(err)
                {
                    return false;
                }
            }
            else                                                                // Old asset. We didn't insert this coin yet, so current value is 0 
            {
                mc_SetABCoinQuantity(in_amounts->GetRow(row),coin_id,quantity);
            }
        }
    }    

    quantity=0;                                                                 // Setting "selected" flag to 0
    mc_SetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id,quantity);
    quantity=1;                                                                 // Setting "parsed" flag to 1
    mc_SetABCoinQuantity(in_amounts->GetRow(in_special_row[1]),coin_id,quantity);                   
            
    mc_SetABCoinQuantity(in_amounts->GetRow(in_special_row[4]),coin_id,pure_native);                   
    
    return true;
}

bool ParseFromCSCache(const COutput& out,mc_Buffer *amounts,CTxDestination& dest,int *required,bool *txout_with_inline_data,bool *is_empty)
{
    if(out.coin.m_CSDetails.m_Active)
    {
        dest=out.coin.m_CSDetails.m_CSDestination;
        *required=out.coin.m_CSDetails.m_Required;
        *txout_with_inline_data=out.coin.m_CSDetails.m_WithInlineData;
        *is_empty=out.coin.m_CSDetails.m_IsEmpty;
        amounts->Clear();
        for(unsigned int i=0;i<out.coin.m_CSAssets.size();i++)
        {
            amounts->Add(out.coin.m_CSAssets[i].m_Asset);
        }
       
//        printf("P (%s,%d) (%s,%d,%d,%d,%d)\n",out.coin.m_OutPoint.hash.ToString().c_str(),out.i,CBitcoinAddress(dest).ToString().c_str(),amounts->GetCount(),*required,*txout_with_inline_data,*is_empty);
 
        return true;
    }
    return false;
}

bool StoreInCSCache(uint256 hash,uint32_t n,mc_Buffer *amounts,const CTxDestination& dest,int required,bool txout_with_inline_data,CAmount nValue)
{    
    if(pwalletTxsMain == NULL)
    {
        return false;
    }
    if(mc_gState->m_NetworkParams->IsProtocolMultichain() == 0)
    {
        return false;        
    }

    mc_CoinAssetBufRow asset_row;
    bool is_empty=true;
    map<COutPoint, mc_Coin>::iterator it = pwalletTxsMain->m_UTXOs[0].find(COutPoint(hash,n));
    if(it != pwalletTxsMain->m_UTXOs[0].end())
    {
        it->second.m_CSDetails.m_Active=true;
        it->second.m_CSDetails.m_CSDestination=dest;
        it->second.m_CSDetails.m_Required=required;
        it->second.m_CSDetails.m_WithInlineData=txout_with_inline_data;
        if(nValue > 0)
        {
            is_empty=false;
        }
        for(int i=0;i<amounts->GetCount();i++)
        {
            if(mc_GetABRefType(amounts->GetRow(i)) != MC_AST_ASSET_REF_TYPE_SPECIAL)
            {
                memcpy(asset_row.m_Asset,amounts->GetRow(i),MC_AST_ASSET_FULLREF_BUF_SIZE);
                it->second.m_CSAssets.push_back(asset_row);
                is_empty=false;
            }
        }
        it->second.m_CSDetails.m_IsEmpty=is_empty;
//        printf("S (%s,%d) (%s,%d,%d,%d,%d)\n",hash.ToString().c_str(),n,CBitcoinAddress(dest).ToString().c_str(),(int)it->second.m_CSAssets.size(),required,txout_with_inline_data,is_empty);
    }
    
    return is_empty;
}


int RecalculateCSCacheAllowed(CTxDestination& dest,int expected_allowed,map<uint32_t, uint256>* mapSpecialEntity)
{
    int allowed=CheckRequiredPermissions(dest,expected_allowed,mapSpecialEntity,NULL);
    if(expected_allowed & MC_PTP_SEND)                
    {
        if(allowed & MC_PTP_SEND)
        {
            if(allowed & MC_PTP_RECEIVE)
            {
                allowed -= MC_PTP_RECEIVE;
            }
            else
            {
                allowed -= MC_PTP_SEND;
            }
        }
    }
    
    return allowed;
}


/*
 * Find relevant coins for asset transfer
 */

bool FindRelevantCoins(CWallet *lpWallet,                                       // IN  Wallet (to access grouping object)
                       vector<COutput>& vCoins,                                 // IN  unspent coins
                       mc_Buffer *out_amounts,                                  // IN  assets amounts to be found
                       int expected_required,                                   // IN  expected permissions
                       bool *no_send_coins,                                     // OUT flag, there are relevant coins without send permission
                       bool *inline_coins,                                      // OUT flag, there are relevant coins without send permission
                       mc_Buffer *in_amounts,                                   // OUT Buffer to fill, rows - assets, columns - coins
                       mc_Buffer *in_map,                                       // OUT txid/vout->coin id map (used in coin selection)
                       mc_Buffer *tmp_amounts,                                  // TMP temporary asset-quantity buffer
                       unsigned char *in_row,int in_size,                       // TMP temporary buffer row and its size
                       mc_Script *lpScript,                                     // TMP temporary multichain script object
                       int *in_special_row,                                     // IN  Coordinates of special rows
                       map<uint32_t, uint256>* mapSpecialEntity,
                       std::string& strFailReason)                              // OUT error message
{
    int coin_id=0;
    int group_id;
    int64_t pure_native;
    
    bool check_for_inline_coins=GetBoolArg("-lockinlinemetadata",true);
    bool txout_with_inline_data=false;
    bool found_in_cache=false;
    bool use_cache=false;
    bool good_destination=false;
    map<CTxDestination,int> mapAllowed;
    
    if(vCoins.size() > 2)
    {
        use_cache=true;
    }
    
    if(debug_print)printf("debg: Inputs - normal\n");
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        string strError;
        int allowed=expected_required;
        int required=expected_required;
        int out_i;
        bool is_relevant;
        CTxDestination dest;
        bool is_empty=false;
        
        CTxOut txout;
        uint256 hash=out.GetHashAndTxOut(txout);
        
        out_i=out.i;
        tmp_amounts->Clear();
        if(custom_good_for_coin_selection(txout.scriptPubKey) &&
            ( (use_cache && (found_in_cache=ParseFromCSCache(out,tmp_amounts,dest,&required,&txout_with_inline_data,&is_empty))) ||     
            ParseMultichainTxOutToBuffer(hash,txout,tmp_amounts,lpScript,&allowed,&required,mapSpecialEntity,strError)))
        {
                                                                                // All coins are taken, possible future optimization
/*            
            is_relevant=false;
            i=0;
            while(!is_relevant && i<tmp_amounts->GetCount())
            {
//                if( (mc_GetLE(assetRef+MC_AST_ASSET_REF_TYPE_OFFSET,MC_AST_ASSET_REF_TYPE_SIZE) != MC_AST_ASSET_REF_TYPE_SPECIAL)
                      || (mc_GetLE(tmp_amounts->GetRow(i)+4,4) != MC_PTP_SEND) )
                {
                    if(out_amounts->Seek(tmp_amounts->GetRow(i)) >= 0)
                    {
                        is_relevant=true;
                    }
                }
                i++;
            }            
 */ 
            group_id=lpWallet->lpAssetGroups->GetGroup(tmp_amounts,1);          // Assign group to all assets, we don't care at this stage about coin group
            if(group_id == -1)                                                  // Only about error
            {
                strFailReason=_("Internal error: Cannot put assets into groups");
                return false;                
            }
            
            pure_native=0;
            if(group_id == -2)
            {
                pure_native=1;
            }
            
            for(int i=0;i<tmp_amounts->GetCount();i++)
            {
                if(debug_print)DebugPrintAssetTxOut(hash,out_i,tmp_amounts->GetRow(i),mc_GetABQuantity(tmp_amounts->GetRow(i)));
            }
            
            is_relevant=true;
            if(required & MC_PTP_ISSUE)                                         // This txout contains unconfirmed genesis, cannot be spent
            {
                is_relevant=false;
            }
            if(is_relevant)
            {
                if(found_in_cache)
                {
                    std::map<CTxDestination,int>::iterator it=mapAllowed.find(dest);
                    if(it == mapAllowed.end())
                    {
//                        allowed=RecalculateCSCacheAllowed(dest,expected_required,mapSpecialEntity);
                        allowed=CheckRequiredPermissions(dest,expected_required,mapSpecialEntity,NULL);
                        mapAllowed.insert(make_pair(dest, allowed));
                    }
                    else
                    {
                        allowed=it->second;
                    }
                    if(expected_required & MC_PTP_SEND)                
                    {
                        if(allowed & MC_PTP_SEND)
                        {
                            if(allowed & MC_PTP_RECEIVE)
                            {
                                allowed -= MC_PTP_RECEIVE;
                            }
                            else
                            {
                                allowed -= MC_PTP_SEND;
                                if(mc_gState->m_Features->AnyoneCanReceiveEmpty())                                
                                {
                                    if(is_empty)
                                    {
                                        allowed |= MC_PTP_SEND;
                                    }
                                }
                            }
                        }
                    }
                    
                    AppendSpecialRowsToBuffer(tmp_amounts,hash,expected_required,expected_required,&allowed,&required,txout.nValue);
                }
                
                good_destination=true;
                if(!found_in_cache)
                {
                    is_empty=false;
                    txout_with_inline_data=HasPerOutputDataEntries(txout,lpScript);
                    dest=CNoDestination();
                    good_destination=ExtractDestinationScriptValid(txout.scriptPubKey, dest);
                    if(use_cache && good_destination)
                    {
                        is_empty=StoreInCSCache(hash,out_i,tmp_amounts,dest,required,txout_with_inline_data,txout.nValue);
                    }
                }
                
                if(!check_for_inline_coins)
                {
                    txout_with_inline_data=false;
                }
                
                if(!is_empty)
                {
                    if(mc_gState->m_Features->PerAssetPermissions())
                    {
                        if(allowed & MC_PTP_SEND)
                        {
                            if(good_destination)
                            {
                                string strPerAssetFailReason;

                                vector<CTxDestination> addressRets;
                                addressRets.push_back(dest);

                                if(!mc_VerifyAssetPermissions(tmp_amounts,addressRets,1,MC_PTP_SEND,strPerAssetFailReason) || 
                                   !mc_VerifyAssetPermissions(tmp_amounts,addressRets,1,MC_PTP_RECEIVE,strPerAssetFailReason))
                                {
                                    allowed -= MC_PTP_SEND;                                
                                }
                            }
                        }
                    }
                }
                    
                
                if(!txout_with_inline_data)
                {
                    if(allowed & MC_PTP_SEND)
                    {                    
                        if(!InsertCoinIntoMatrix(coin_id,hash,out_i,tmp_amounts,out_amounts,in_amounts,in_map,in_row,in_size,in_special_row,pure_native))
                        {
                            strFailReason=_("Internal error: Cannot update input amount matrix");
                            return false;
                        }
                    }
                    else
                    {
                        *no_send_coins=true;
                    }
                }
                else
                {
                    *inline_coins=true;
                }
            }
        }
        tmp_amounts->Clear();
        coin_id++;
    }
    
    return true;
}

struct CompareByFirst
{
    bool operator()(const pair<int, int>& t1,
                    const pair<int, int>& t2) const
    {
        return (t1.first < t2.first);
    }
};

struct CompareBySecond
{
    bool operator()(const pair<int, int>& t1,
                    const pair<int, int>& t2) const
    {
        return (t1.second < t2.second);
    }
};


/* 
 * Selecting coins fro auto-combine
 */

bool FindCoinsToCombine(CWallet *lpWallet,                                      // IN  Wallet (to access grouping object)
                        vector<COutput>& vCoins,                                // IN  unspent coins    
                        int min_conf,int min_inputs,int max_inputs,             // IN  Auto-combine parameters
                        mc_Buffer *in_amounts,                                  // OUT Buffer to fill, rows - assets, columns - coins
                        mc_Buffer *in_map,                                      // OUT txid/vout->coin id map (used in coin selection)
                        mc_Buffer *tmp_amounts,                                 // TMP temporary asset-quantity buffer
                        unsigned char *in_row,int in_size,                      // TMP temporary buffer row and its size
                        mc_Script *lpScript,                                    // TMP temporary multichain script object
                        int *in_special_row,                                    // IN  Coordinates of special rows
                        std::string& strFailReason)                             // OUT error message
{
    int coin_id=0;
    int group_id,group_count,i,pure_native_group;
    int count=0;
    int full_count,this_count,pure_native_count;
    CAmount total_native;
    int total_native_hit;
    vector <pair<int,int> > active_groups;                                      // Groups found in UTXOs
    bool check_for_inline_coins=GetBoolArg("-lockinlinemetadata",true);
    bool txout_with_inline_data;
    
    group_count=lpWallet->lpAssetGroups->GroupCount();
    pure_native_count=0;
    active_groups.resize(group_count);
    for(i=0;i<group_count;i++)
    {
        active_groups[i]=make_pair(i,0);
    }   
    
    if(debug_print)printf("debg: Inputs - combine, bad coins\n");
    BOOST_FOREACH(const COutput& out, vCoins)
    {       
        string strError;
        int allowed=MC_PTP_SEND;
        int required=MC_PTP_SEND;
        int out_i;
        
        if( (count<max_inputs) && (out.nDepth >= min_conf) )
        {
            CTxOut txout;
            uint256 hash=out.GetHashAndTxOut(txout);
            out_i=out.i;
            tmp_amounts->Clear();
            if(custom_good_for_coin_selection(txout.scriptPubKey) && 
                    ParseMultichainTxOutToBuffer(hash,txout,tmp_amounts,lpScript,&allowed,&required,strError))
            {
                txout_with_inline_data=false;
                if(check_for_inline_coins)
                {
                    txout_with_inline_data=HasPerOutputDataEntries(txout,lpScript);
                }
                if( !txout_with_inline_data && ((required & MC_PTP_ISSUE) == 0) )                            // Ignore txouts containing unconfirmed geneses
                {
                    group_id=lpWallet->lpAssetGroups->GetGroup(tmp_amounts,1);      // Find group id, insert new assets if needed
                    if(debug_print)printf("%s-%d: group %d\n",hash.ToString().c_str(),out.i,group_id);
                    if(group_id == -1)
                    {
                        strFailReason=_("Internal error: Cannot update asset grouping information");
                        return false;
                    }
                    else
                    {   
                        if( (group_id == 0)  ||                                 // We couldn't add new asset into one group
                            (group_id == -3) )                                  // Assets in this txout belongs to different groups
                                                                                // Take it
                        {
                            for(int i=0;i<tmp_amounts->GetCount();i++)
                            {
                                if(debug_print)DebugPrintAssetTxOut(hash,out_i,tmp_amounts->GetRow(i),mc_GetABQuantity(tmp_amounts->GetRow(i)));
                            }
                            if(!InsertCoinIntoMatrix(coin_id,hash,out_i,tmp_amounts,NULL,in_amounts,in_map,in_row,in_size,in_special_row,0))
                            {
                                strFailReason=_("Internal error: Cannot update input amount matrix");
                                return false;
                            }                
                            count++;
                        }
                        else
                        {
                            if(group_id == -2)                                      // Pure native currency
                            {
                               group_id=0; 
                               pure_native_count++;
                            }                                        
                            while(group_id >= group_count)       
                            {
                                active_groups.push_back(make_pair(group_count,0));
                                group_count++;
                            }
                            if(group_id)
                            {
                                active_groups[group_id]=make_pair(group_id,active_groups[group_id].second+1);
                            }
                        }
                    }
                }
            }        
        }
            
        coin_id++;
    }
    
    pure_native_group=0;
    if(pure_native_count)
    {
        pure_native_group=group_count;//+1;
        active_groups.push_back(make_pair(group_count,pure_native_count));
    }
    
    if(count < max_inputs)                                                      // The number of out-of-group coins is not enough
    {
        sort(active_groups.begin(), active_groups.end(), CompareBySecond());    // Groups with few coins first
        full_count=0;
        for(i=0;i<(int)active_groups.size();i++)
        {
            this_count=active_groups[i].second;
            if(this_count<=1)                                                   // There is only one coin - nothing to combine
            {
                active_groups[i].second=0;
            }
            else
            {
                if(full_count >= max_inputs-1)                                  // We already have enough coins 
                {
                    this_count=0;
                }
                else
                {
                    if(full_count+this_count>max_inputs)                        // We need only part of this group
                    {
                        this_count=max_inputs-full_count;
                    }
                }
                active_groups[i].second=this_count;
            }
        }
        sort(active_groups.begin(), active_groups.end(), CompareByFirst());   // Sort them back
    }
    
    
    if(debug_print)printf("debg: Inputs - combine, multiple coins\n");
    
    coin_id=0;
    total_native=0;
    total_native_hit=0;
    
    BOOST_FOREACH(const COutput& out, vCoins)
    {       
        string strError;
        int allowed=MC_PTP_SEND;
        int required=MC_PTP_SEND;
        int out_i;
        
        if( (count<max_inputs) && (out.nDepth >= min_conf) )
        {
            CTxOut txout;
            uint256 hash=out.GetHashAndTxOut(txout);
            out_i=out.i;
            tmp_amounts->Clear();
            if(custom_good_for_coin_selection(txout.scriptPubKey) &&
                    ParseMultichainTxOutToBuffer(hash,txout,tmp_amounts,lpScript,&allowed,&required,strError))
            {
                if( (required & MC_PTP_ISSUE) == 0 )                            // Ignore txouts containing unconfirmed geneses
                {
                    group_id=lpWallet->lpAssetGroups->GetGroup(tmp_amounts,0);      // No adding this time, this will remove bad and newly created coins added in previous loop
                    if( (group_id > 0) || 
                        (group_id == -2) )                                          // Pure native currency
                    {
                        if(group_id == -2)
                        {
                           group_id=pure_native_group; 
                        }                                        
                        if(active_groups[group_id].second > 0)                      
                        {
                            if(total_native + txout.nValue <= MAX_MONEY)
                            {
                                for(int i=0;i<tmp_amounts->GetCount();i++)
                                {
                                    if(debug_print)DebugPrintAssetTxOut(hash,out_i,tmp_amounts->GetRow(i),mc_GetABQuantity(tmp_amounts->GetRow(i)));
                                }
                                if(!InsertCoinIntoMatrix(coin_id,hash,out_i,tmp_amounts,NULL,in_amounts,in_map,in_row,in_size,in_special_row,0))
                                {
                                    strFailReason=_("Internal error");
                                    return false;
                                }                
                                count++;       
                                active_groups[group_id].second-=1;
                                total_native+=txout.nValue;
                            }
                            else
                            {
                                total_native_hit=1;                                
                            }
                        }
                    }
                }
            }        
        }
            
        coin_id++;
    }
    
    if( (count<min_inputs) && ( (total_native_hit == 0) || (count == 1) ))                                                       // Not enough coins to combine
    {
        strFailReason = _("Not enough inputs");
        return false;
    }
    
                                                                                // All parsed coins are selected
    memcpy(in_amounts->GetRow(in_special_row[0]),in_amounts->GetRow(in_special_row[1]),in_size);
    
    
    return true;
}

/*
 * Fills change amount buffer "selected"-"output"
 */

bool CalculateChangeAmounts(CWallet *lpWallet,                                  // IN  Wallet (to access grouping object)
                            vector<COutput>& vCoins,                            // IN  unspent coins    
                            CAmount TotalOutAmount,                             // IN  native currency amount used for change
                            mc_Buffer *out_amounts,                             // IN  assets amounts to be found                            
                            int expected_required,                              // IN  expected permissions
                            mc_Buffer *in_amounts,                              // IN  selected asset amounts
                            mc_Buffer *change_amounts,                          // OUT change amounts, asset-quantity buffer with additional field - group
                            mc_Buffer *tmp_amounts,                             // TMP temporary asset-quantity buffer
                            mc_Script *lpScript,                                // TMP temporary multichain script object
                            int *in_special_row,                                // IN  Coordinates of special rows
                            map<uint32_t, uint256>* mapSpecialEntity,           // IN  Special permission entry, like issue txid of follow-on issuance  
                            set<CTxDestination>* usedAddresses,                 // OUT List of addresses used in selection.
                            std::string& strFailReason)                         // OUT error message
{
    int coin_id=0;
    unsigned char buf[MC_AST_ASSET_FULLREF_BUF_SIZE+sizeof(int)];
    int64_t quantity;
    string strError;
    int allowed=expected_required;
    int required=expected_required;
    int out_i,row,group_id;
    int err;        

    if(debug_print)printf("debg: Selected:\n");
    
    usedAddresses->clear();
    change_amounts->Clear();
    BOOST_FOREACH(const COutput& out, vCoins)                                   // Inputs
    {
                                                                                // Coin is taken
        if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[1]),coin_id))
        {
                                                                                // Coin is selected
            if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id))
            {
                CTxOut txout;
                uint256 hash=out.GetHashAndTxOut(txout);
                out_i=out.i;
                tmp_amounts->Clear();                                     
                allowed=expected_required;
                required=expected_required;
                if(ParseMultichainTxOutToBuffer(hash,txout,tmp_amounts,lpScript,&allowed,&required,mapSpecialEntity,strError))
                {
                    for(int i=0;i<tmp_amounts->GetCount();i++)
                    {
                        if(debug_print)DebugPrintAssetTxOut(hash,out.i,tmp_amounts->GetRow(i),mc_GetABQuantity(tmp_amounts->GetRow(i)));
                        if(fDebug)LogAssetTxOut("Input : ",hash,out_i,tmp_amounts->GetRow(i),mc_GetABQuantity(tmp_amounts->GetRow(i)));
                    }
                    for(int i=0;i<tmp_amounts->GetCount();i++)                  // Add all assets onto change buffer, not only those from out_amounts
                    {
                        quantity=mc_GetABQuantity(tmp_amounts->GetRow(i));
                        if( (mc_GetABRefType(tmp_amounts->GetRow(i)) != MC_AST_ASSET_REF_TYPE_SPECIAL) &&
                            (mc_GetABRefType(tmp_amounts->GetRow(i)) != MC_AST_ASSET_REF_TYPE_GENESIS) )                    
                        {                                                       // All assets should be already populated into groups as coin is parsed
                            group_id=lpWallet->lpAssetGroups->GetGroup(tmp_amounts->GetRow(i),0);
                        }
                        else
                        {
                            group_id=0;                                         // Native or special asset - no group
                        }
                        row=change_amounts->Seek(tmp_amounts->GetRow(i));
                        if(row < 0)                                             // New asset
                        {
                            memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE+sizeof(int));
                            memcpy(buf,tmp_amounts->GetRow(i),MC_AST_ASSET_QUANTITY_OFFSET);
                            mc_SetABQuantity(buf,quantity);
                            mc_PutLE(buf+MC_AST_ASSET_FULLREF_BUF_SIZE,&group_id,sizeof(int));
                            err=change_amounts->Add(buf);
                            if(err)
                            {
                                strFailReason=_("Internal error: Cannot update change asset matrix");
                                return false;
                            }
                        }
                        else                                                            // Old asset
                        {
                            quantity+=mc_GetABQuantity(change_amounts->GetRow(row));
                            mc_SetABQuantity(change_amounts->GetRow(row),quantity);
                        }
                    }    
                    
                    const CScript& script1 = txout.scriptPubKey;        
                    CTxDestination addressRet;        
                    if(ExtractDestinationScriptValid(script1, addressRet))
                    {
                        if(usedAddresses->count(addressRet) == 0)
                        {
                            usedAddresses->insert(addressRet);
                        }
                    }                                            
                }
                else
                {
                    strFailReason=strError;
                    return false;                    
                }
            }
        }
        coin_id++;
    }
        
    for(int i=0;i<out_amounts->GetCount();i++)                                  // Outputs
    {
        if(mc_GetABRefType(out_amounts->GetRow(i)) != MC_AST_ASSET_REF_TYPE_GENESIS)
        {
            quantity=-mc_GetABQuantity(out_amounts->GetRow(i));
            if(quantity)
            {
                row=change_amounts->Seek(out_amounts->GetRow(i));
                if(row < 0)                                                     // New asset, we don't care about group, it is already bad - negative amount 
                {
                    memset(buf,0,MC_AST_ASSET_FULLREF_BUF_SIZE+sizeof(int));
                    memcpy(buf,out_amounts->GetRow(i),MC_AST_ASSET_QUANTITY_OFFSET);
                    mc_SetABQuantity(buf,quantity);
                    err=change_amounts->Add(buf);
                    if(err)
                    {
                        return false;
                    }
                }
                else                                                                // Old asset
                {
                    quantity+=mc_GetABQuantity(change_amounts->GetRow(row));
                    mc_SetABQuantity(change_amounts->GetRow(row),quantity);
                }
            }
        }
    }    
        
    if(debug_print)printf("debg: Change:\n");
    for(int i=0;i<change_amounts->GetCount();i++)
    {
        if(debug_print)DebugPrintAssetTxOut(0,0,change_amounts->GetRow(i),mc_GetABQuantity(change_amounts->GetRow(i)));
    }
    
    return true;
    
}

/*
 * Finds address for change
 */


bool FindChangeAddress(CWallet *lpWallet,CTxDestination& address,const set<CTxDestination>* addresses, CReserveKey& reservekey, const CCoinControl* coinControl,int expected_required,map<uint32_t, uint256>* mapSpecialEntity)
{
    int required;
    bool change_address_found=true;
    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
    {
        address = coinControl->destChange;
    }
    else
    {
        required=expected_required;
        if(required & MC_PTP_SEND)
        {
            required-=MC_PTP_SEND;
        }
        required |= MC_PTP_RECEIVE;
        
        if(addresses)
        {
            if(addresses->size() == 1)
            {
                address=*addresses->begin();
                const unsigned char *aptr;
                aptr=GetAddressIDPtr(address);
                if(mc_gState->m_Permissions->CanReceive(NULL,aptr))
                {
                    return true;
                }   
                else
                {
                    address=CNoDestination();
                    return false;
                }
            }
        }
        
        CPubKey vchPubKey;
        if(!lpWallet->GetKeyFromAddressBook(vchPubKey,MC_PTP_SEND | required,addresses,mapSpecialEntity))
        {
            if(!lpWallet->GetKeyFromAddressBook(vchPubKey,MC_PTP_SEND | MC_PTP_RECEIVE,addresses))
            {
                if(!lpWallet->GetKeyFromAddressBook(vchPubKey,required,addresses))
                {
                    if(!lpWallet->GetKeyFromAddressBook(vchPubKey,MC_PTP_RECEIVE,addresses))
                    {
                        change_address_found=false;
                    }
                }
            }
        }
        if(change_address_found)
        {
            address=vchPubKey.GetID();
        }
        else
        {
            address=CNoDestination();
        }        
    }
    return change_address_found;
}

/* 
 * Select coins we have to use 
 */

bool SelectCoinsToUse(const vector<COutPoint>* lpCoinsToUse,                            // IN  List of coins to be used in this transaction
                      mc_Buffer *in_map,                                        // IN  txid/vout->coin id map (used in coin selection)
                      mc_Buffer *in_amounts,                                    // IN/OUT Input amounts buffer, output - selected row
                      int *in_special_row,                                      // IN  Coordinates of special rows
                      std::string& strFailReason)                               // OUT error message
{
    int64_t quantity;
    int coin_id;
    unsigned char buf_map[32+4+4];
    
    if(lpCoinsToUse == NULL)
    {
        return true;
    }
    
    BOOST_FOREACH(const COutPoint& coin, *lpCoinsToUse)
    {
                                                                                // Setting "selected" flag
        uint256 hash=coin.hash;
        int out_i;
        out_i=coin.n;

        memcpy(buf_map,&hash,32);
        mc_PutLE(buf_map+32,&out_i,4);
        int row=in_map->Seek(buf_map);
        
        if(row<0)
        {
            strFailReason="Could not find pre-selected output";
            return false;
        }
        
        coin_id=mc_GetLE(in_map->GetRow(row)+36,4);
        quantity=1;
        mc_SetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id,quantity);                                                        
    }            
    
    return true;
}
/* 
 * Select coins for specific asset
 */


bool SelectAssetCoins(CWallet *lpWallet,                                        // IN  Wallet (to access grouping object)
                      CAmount nTotalOutValue,                                   // IN  Total output value to find
                      vector<COutput>& vCoins,                                  // IN  unspent coins    
                      mc_Buffer *in_map,                                        // IN  txid/vout->coin id map (used in coin selection)
                      mc_Buffer *in_amounts,                                    // IN/OUT Input amounts buffer, output - selected row
                      int *in_special_row,                                      // IN  Coordinates of special rows
                      int in_asset_row,                                         // IN  Row asset we are looking coins for
                      const CCoinControl* coinControl)
{
    int64_t quantity;
    CAmount nTotalInValue;
    CAmount nTotalSelectedValue;
    CAmount nTargetValue;
    int coin_id;
    unsigned char buf_map[32+4+4];
    bool found_selected=false;
    
    nTotalInValue=0;
    nTotalSelectedValue=0;
    if(in_asset_row>=0)
    {
        for(coin_id=0;coin_id<(int)vCoins.size();coin_id++)
        {
                                                                                // Coin is parsed
            if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[1]),coin_id))
            {
                if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id))
                {
                    found_selected=true;
                }
                quantity=mc_GetABCoinQuantity(in_amounts->GetRow(in_asset_row),coin_id);
                if(quantity)
                {
                    nTotalInValue+=quantity;                            
                                                                                // Coin is selected
                    if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id))
                    {
                        nTotalSelectedValue+=quantity;
                    }                                
                }
            }
        }
    }
    
    nTargetValue=nTotalOutValue-nTotalSelectedValue;
    if(debug_print)printf("debg: Row: %d. In: %ld; Out: %ld; Selected: %ld; Target: %ld \n",in_asset_row,nTotalInValue,nTotalOutValue,nTotalSelectedValue,nTargetValue);
    
    if(nTotalOutValue>nTotalInValue)                                            // Insufficient funds    
    {                        
        return false;
    }               

    
    if(( nTargetValue > 0) || (!found_selected) )
    {
        set<pair<uint256,unsigned int> > setAssetCoins;
        CAmount nAssetValueIn = 0;
        
        int in_prefered_row=in_asset_row;   
        for(int i=0;i<7;i++)
        {
            if(in_asset_row == in_special_row[i])
            {
                in_prefered_row=4;                                              // Special asset - prefer pure native currency coins
            }
        }
                                                                                // Selecting coins covering remaining asset amount
        if(!lpWallet->SelectMultiChainCoins(nTargetValue,vCoins,in_map,in_amounts,in_special_row[0],in_asset_row,in_prefered_row,setAssetCoins, nAssetValueIn, coinControl))
        {
            return false;
        }                        
        
        BOOST_FOREACH(PAIRTYPE(uint256, unsigned int) pcoin, setAssetCoins)
        {
                                                                                // Setting "selected" flag
            uint256 hash=pcoin.first;
            int out_i;
            out_i=pcoin.second;

            memcpy(buf_map,&hash,32);
            mc_PutLE(buf_map+32,&out_i,4);
            int row=in_map->Seek(buf_map);
            coin_id=mc_GetLE(in_map->GetRow(row)+36,4);
            quantity=1;

            mc_SetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id,quantity);                                                        
        }        
    }    

    return true;
}

/*
 * Build signed transaction with given outputs, inputs and change amounts and fee 
 * Returns 0 on success
 * Returns negative value on error
 * Returns missing native currency value if positive
 */


CAmount BuildAssetTransaction(CWallet *lpWallet,                                // IN  Wallet object
                              CWalletTx& wtxNew,                                // OUT Resulting transaction
                              CTxDestination& change_address,                   // IN  Address for change
                              CAmount& nFeeRet,                                 // IN/OUT Required fee
                              const vector<pair<CScript, CAmount> >& vecSend,   // IN  Output amount/script array
                              vector<COutput>& vCoins,                          // IN  Unspent UTXOs
                              mc_Buffer *in_amounts,                            // IN  Input amount matrix
                              mc_Buffer *change_amounts,                        // IN  Change amount matrix
                              int required,                                     // IN  Required special permissions
                              CAmount min_output,                               // IN  minimum native currency value (to be set in change outputs))
                              mc_Buffer *tmp_amounts,                           // TMP temporary asset-quantity buffer
                              mc_Script *lpScript,                              // TMP temporary multichain script object
                              int *in_special_row,                              // IN  Coordinates of special rows
                              set<CTxDestination>* usedAddresses,               // In  List of addresses used in selection.
                              uint32_t flags,                                   // In  Coin selection flags
                              std::string& strFailReason)                       // OUT error message
{
    set <int> active_groups;
    
    int group_id;
    int change_count;
    int extra_change_count;
    CAmount missing_amount;
    int64_t quantity;
    
    CAmount nTotalInValue=0;
    CAmount nTotalChangeValue=0;
    CAmount default_change_output;
    CAmount change_amount;
    CAmount mandatory_fee=0;
    int64_t total_offchain_size=0;
        
    for(int i=0;i<change_amounts->GetCount();i++)                               // Finding relevant asset groups and calculating native currency total 
    {
        quantity=mc_GetABQuantity(change_amounts->GetRow(i));
        if(quantity<0)                                                          // We didn't find inputs properly
        {
            strFailReason=_("Internal error: negative change amount");
            return -1;                                
        }
        if(quantity>0)
        {
            group_id=mc_GetLE(change_amounts->GetRow(i)+MC_AST_ASSET_FULLREF_BUF_SIZE,sizeof(int));
            if(mc_GetABRefType(change_amounts->GetRow(i)) != MC_AST_ASSET_REF_TYPE_SPECIAL)
            //if(mc_GetLE(change_amounts->GetRow(i),4) != 0)
            {
                if(debug_print)printf("debg: Found asset belonging to group %d\n",group_id);
            }
            if(group_id)                                                        // We don't care about native and special assets
            {
                std::set<int>::const_iterator it = active_groups.find(group_id);
                if (it == active_groups.end())
                {
                    active_groups.insert(group_id);
                }
            }
            if( (mc_GetABRefType(change_amounts->GetRow(i)) == MC_AST_ASSET_REF_TYPE_SPECIAL) &&
                 (mc_GetLE(change_amounts->GetRow(i)+4,4) == MC_PTP_SEND) )
            {
                nTotalInValue=quantity;
            }
        }
    }
    
    change_count=0;
    if(nTotalInValue > nFeeRet)                                                 // Pure native currency change is required
    {
        change_count=1;
    }
    change_count+=active_groups.size();
    
    const unsigned char* change_aptr=GetAddressIDPtr(change_address);           
    
    if(change_aptr == NULL)
    {
        if(change_count>0)                                                      // We have to send change, but we couldn't find change address with receive permission 
        {
            strFailReason = _("Change address not found");
            return -2;
        }
    }
    else
    {
        if(change_count == 0)
        {
            change_count=1;
        }
    }
    
    extra_change_count=0;
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        BOOST_FOREACH(const CTxDestination& address, *usedAddresses)             // Sending empty change to all addresses used in inputs, except change_address
        {        
            const unsigned char* aptr=GetAddressIDPtr(address);
            if(aptr)
            {
                if((change_aptr == NULL) ||  memcmp(change_aptr,aptr,20))
                {                    
                    if(mc_gState->m_Permissions->CanReceive(NULL,aptr))
                    {
                        extra_change_count++;
                    }
                    else
                    {
                        if((mc_gState->m_Features->AnyoneCanReceiveEmpty() != 0) && (min_output == 0))                          
                        {
                            extra_change_count++;                            
                        }                        
                    }
                }                
            }
        }        
    }
    
    if(min_output >= 0)
    {
        default_change_output=min_output;
    }
    else
    {
        default_change_output=182;   // 34 + 148 (see CTxOut.IsDust for explanation)
        if(MCP_WITH_NATIVE_CURRENCY == 0)
        {
            default_change_output=0;
        }
    }
    
    if(MIN_OFFCHAIN_FEE)
    {
        total_offchain_size=0;
        BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)            // Original outputs
        {
            MultiChainTransaction_SetTmpOutputScript(s.first);
            mc_gState->m_TmpScript->ExtractAndDeleteDataFormat(NULL,NULL,NULL,&total_offchain_size);            
        }
        mandatory_fee=MultiChainTransaction_OffchainFee(total_offchain_size);
    }
    
    if(nFeeRet == 0)
    {
        nFeeRet=mandatory_fee;
    }
    
    missing_amount=nFeeRet+(change_count+extra_change_count)*default_change_output-nTotalInValue;
    if(missing_amount > 0)                                                  // Inputs don't carry enough native currency, go out and select additional coins 
    {
        if(fDebug)LogPrint("mcatxo","mcatxo: Missing amount: %ld. Fee: %ld, Change: %ld, Inputs: %ld\n",missing_amount,nFeeRet,change_count*min_output,nTotalInValue);
        if(debug_print)printf("Missing amount: %ld. Fee: %ld, Change: %ld, Inputs: %ld\n",missing_amount,nFeeRet,change_count*min_output,nTotalInValue);
        return missing_amount;
    }
    
    missing_amount = 0;
    
    while(missing_amount == 0)
    {
        CMutableTransaction txNew;
        txNew.vin.clear();
        txNew.vout.clear();
        nTotalChangeValue=0;
        
        BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)            // Original outputs
        {
            CTxOut txout(s.second, s.first);

            txNew.vout.push_back(txout);
        }

        int assets_per_opdrop;
        
        assets_per_opdrop=(MAX_SCRIPT_ELEMENT_SIZE-4)/(mc_gState->m_NetworkParams->m_AssetRefSize+MC_AST_ASSET_QUANTITY_SIZE);
 
        size_t elem_size;
        const unsigned char *elem;

        BOOST_FOREACH (const int& g, active_groups)                             // Asset change outputs
        {
            tmp_amounts->Clear();
            CScript scriptChange=GetScriptForDestination(change_address);
            for(int i=0;i<change_amounts->GetCount();i++)
            {
                group_id=mc_GetLE(change_amounts->GetRow(i)+MC_AST_ASSET_FULLREF_BUF_SIZE,sizeof(int));
                if(group_id == g)
                {
                    quantity=mc_GetABQuantity(change_amounts->GetRow(i));
                    if(quantity>0)
                    {
                        tmp_amounts->Add(change_amounts->GetRow(i));
                    }
                }
                                                                                // flush asset buffer into script]
                if( (tmp_amounts->GetCount()>=assets_per_opdrop) || (i == change_amounts->GetCount()-1) )
                {
                    if(tmp_amounts->GetCount())
                    {
                        if(debug_print)printf("debg: Change output, group %d:\n",g);
                        for(int i=0;i<tmp_amounts->GetCount();i++)
                        {
                            if(debug_print)DebugPrintAssetTxOut(0,0,tmp_amounts->GetRow(i),mc_GetABQuantity(tmp_amounts->GetRow(i)));
                            if(fDebug)LogAssetTxOut("Change: ",0,0,tmp_amounts->GetRow(i),mc_GetABQuantity(tmp_amounts->GetRow(i)));
                        }
                        lpScript->Clear();
                        lpScript->SetAssetQuantities(tmp_amounts,MC_SCR_ASSET_SCRIPT_TYPE_TRANSFER);
                        for(int element=0;element < lpScript->GetNumElements();element++)
                        {
                            elem = lpScript->GetData(element,&elem_size);
                            if(elem)
                            {
                                scriptChange << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
                            }
                            else
                            {
                                strFailReason = _("Internal error: cannot create asset transfer script");
                                return -1;
                            }
                        }
                        tmp_amounts->Clear();
                    }
                }
            }
            change_amount=min_output;
            if(change_amount<0)
            {
                CTxOut txout_for_size(0, scriptChange);
                change_amount=3*(::minRelayTxFee.GetFee(txout_for_size.GetSerializeSize(SER_DISK,0)+148u));                
            }
            CTxOut txout(change_amount, scriptChange);
            nTotalChangeValue += change_amount;
            txNew.vout.push_back(txout);            
        }
        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            BOOST_FOREACH(const CTxDestination& address, *usedAddresses)             // Sending empty change to all addresses used in inputs, except change_address
            {        
                const unsigned char* aptr=GetAddressIDPtr(address);
                if(aptr)
                {
                    if((change_aptr == NULL) ||  memcmp(change_aptr,aptr,20))
                    {
                        if( (mc_gState->m_Permissions->CanReceive(NULL,aptr) != 0) || 
                            ((mc_gState->m_Features->AnyoneCanReceiveEmpty() != 0) && (default_change_output == 0)) )
                        {
                            CScript scriptChange=GetScriptForDestination(address);
                            CAmount nAmount=default_change_output;
                            CTxOut txout(nAmount, scriptChange);
                            nTotalChangeValue += nAmount;
                            if(debug_print)printf("debg: Extra change: %ld\n",nAmount);
                            txNew.vout.push_back(txout);                            
                        }
                    }
                }
            }        
        }
        
        if(change_count > (int)active_groups.size())                            // Native currency change
        {
            CScript scriptChange=GetScriptForDestination(change_address);
            CAmount nAmount=nTotalInValue-nTotalChangeValue-nFeeRet;
            CTxOut txout(nAmount, scriptChange);
            if(debug_print)printf("debg: Native change output: %ld\n",nAmount);
            if(fDebug)LogAssetTxOut("Change: ",0,0,NULL,nAmount);
            txNew.vout.push_back(txout);            
        }

        double dPriority = 0;
        bool fScriptCached=false;
        
        int coin_id=0;
        BOOST_FOREACH(const COutput& out, vCoins)                               // Adding inputs
        {            
            if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[1]),coin_id))
            {
                if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id))
                {
                    CAmount nCredit;
                    int age;
                    if(out.tx)
                    {
                        age = out.tx->GetDepthInMainChain();
                        nCredit = out.tx->vout[out.i].nValue;
                    }
                    else
                    {
                        age = out.coin.GetDepthInMainChain();
                        nCredit=out.coin.m_TXOut.nValue;
                    }
                    if (age != 0)
                        age += 1;
                    dPriority += (double)nCredit * age;
                    CTxOut txout;
                    uint256 hash=out.GetHashAndTxOut(txout);
                    txNew.vin.push_back(CTxIn(hash,out.i));
                    if(required & MC_PTP_CACHED_SCRIPT_REQUIRED)
                    {
                        if(!fScriptCached)
                        {
                            if( (mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[6]),coin_id) != 0) || 
                                ( ( required & (MC_PTP_ADMIN | MC_PTP_MINE) ) == 0 ) )
                            {
                                int cs_offset;
                                CScript script3;        
                                size_t elem_size;
                                const unsigned char *elem;
                                
                                if(out.tx)
                                {
                                    script3 = out.tx->vout[out.i].scriptPubKey;
                                }
                                else
                                {
                                    script3=out.coin.m_TXOut.scriptPubKey;
                                }
                                CScript::const_iterator pc3 = script3.begin();
                                
                                lpScript->Clear();
                                lpScript->SetCachedScript(0,&cs_offset,-1,NULL,-1);       
                                lpScript->SetCachedScript(cs_offset,&cs_offset,(int)txNew.vin.size()-1,(unsigned char*)&pc3[0],script3.size());

                                CScript scriptCachedScript=CScript();
                                elem = lpScript->GetData(0,&elem_size);                                
                                scriptCachedScript << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;           

                                CTxOut txout(0, scriptCachedScript);
                                if(debug_print)printf("debg: Cached Script for input %d\n",(int)txNew.vin.size()-1);
                                if(fDebug)LogAssetTxOut("Cached Script: ",0,0,NULL,0);
                                txNew.vout.push_back(txout);            
                                fScriptCached=true;
                            }                            
                        }                        
                    }
                }
            }
            coin_id++;
        }

        if(txNew.vin.size() == 0)
        {
            strFailReason = _("Internal error: no inputs");
            return -2;            
        }
        
        int nIn=0;
        unsigned int nSignatureBytes=0;
        coin_id=0;
        BOOST_FOREACH(const COutput& out, vCoins)                               // Signing
        {            
            if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[1]),coin_id))
            {
                if(mc_GetABCoinQuantity(in_amounts->GetRow(in_special_row[0]),coin_id))
                {
                    CTxOut txout;
                    out.GetHashAndTxOut(txout);

                    if(flags & MC_CSF_SIGN)
                    {
                        if (!SignSignature(*lpWallet, txout.scriptPubKey, txNew, nIn++))
                        {
                            CTransaction printableTx=CTransaction(txNew);
                            if(fDebug)LogPrint("mchn","Cannot sign transaction input %d: (%s,%d), scriptPubKey %s \n",
                                    nIn-1,txNew.vin[nIn-1].prevout.hash.ToString().c_str(),txNew.vin[nIn-1].prevout.n,txout.scriptPubKey.ToString().c_str());

                            strFailReason = _("Signing transaction failed");
                            return -2;
                        }
                    }
                    else
                    {
                        if(txout.scriptPubKey.IsPayToScriptHash())              // We don't know what is the script generally speaking, using 2-of-3
                        {
                            nSignatureBytes+=256;                               // 73*2+33*3+...                
                        }
                        else
                        {
                            nSignatureBytes+=112;
                        }
                    }                    
                }
            }
            coin_id++;
        }
        
        wtxNew.fTimeReceivedIsTxTime = true;
        wtxNew.fFromMe=true;
        wtxNew.BindWallet(lpWallet);
        *static_cast<CTransaction*>(&wtxNew) = CTransaction(txNew);

        // Limit size
        unsigned int nBytes = nSignatureBytes + ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK, PROTOCOL_VERSION);
        if (nBytes >= MAX_STANDARD_TX_SIZE)
        {
            strFailReason = _("Transaction too large");
            return -2;
        }
        dPriority = wtxNew.ComputePriority(dPriority, nBytes);

        // Can we complete this as a free transaction?
        if (fSendFreeTransactions && nBytes <= MAX_FREE_TRANSACTION_CREATE_SIZE)
        {
            // Not enough fee: enough priority?
            double dPriorityNeeded = mempool.estimatePriority(nTxConfirmTarget);
            // Not enough mempool history to estimate: use hard-coded AllowFree.
            if (dPriorityNeeded <= 0 && AllowFree(dPriority))
                break;

            // Small enough, and priority high enough, to send for free
            if (dPriorityNeeded > 0 && dPriority >= dPriorityNeeded)
                break;
        }

        CAmount nFeeNeeded = lpWallet->GetMinimumFee(nBytes, nTxConfirmTarget, mempool)+mandatory_fee;

        // If we made it here and we aren't even able to meet the relay fee on the next pass, give up
        // because we must be at the maximum allowed fee.
        if (nFeeNeeded < ::minRelayTxFee.GetFee(nBytes))
        {
            strFailReason = _("Transaction too large for fee policy");
            return -2;
        }

        if (nFeeRet >= nFeeNeeded)                                              // Done        
        {
            if(debug_print)printf("Transaction created. Fee: %ld, Change: %ld, Inputs: %ld\n",nFeeRet,nTotalChangeValue,nTotalInValue);
            return 0;
        }
        
        if(nFeeNeeded > nFeeRet)
        {
            nFeeRet = nFeeNeeded;
        }
        
        missing_amount=nFeeRet+nTotalChangeValue-nTotalInValue;
        if(missing_amount > 0)                                                  // Inputs don't carry enough native currency, go out and select additional coins 
        {
            if(debug_print)printf("Missing amount: %ld. Fee: %ld, Change: %ld, Inputs: %ld\n",missing_amount,nFeeRet,nTotalChangeValue,nTotalInValue);
            return missing_amount;
        }
        
        if(debug_print)printf("Increased fee: Fee: %ld, Change: %ld, Inputs: %ld\n",nFeeRet,nTotalChangeValue,nTotalInValue);

        missing_amount=0;                                                       // We have enough native currency, we can try again by increasing the fee  
    }    
    
    if(debug_print)printf("Transaction created (free). Fee: %ld, Change: %ld, Inputs: %ld\n",nFeeRet,nTotalChangeValue,nTotalInValue);
    
    return 0;
}

bool CheckOutputPermissions(const vector<pair<CScript, CAmount> >& vecSend,mc_Buffer *tmp_amounts,std::string& strFailReason,int *eErrorCode)
{
    int receive_required;
    int64_t quantity;
    int err;
    bool fIsMaybePurePermission,fIsGenesis;
    
    BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)            
    {
        txnouttype typeRet;
        int nRequiredRet;
        vector<CTxDestination> addressRets;
        if(!ExtractDestinations(s.first,typeRet,addressRets,nRequiredRet))
        {
            if(typeRet != TX_NULL_DATA)
            {
                strFailReason="Non-standard outputs are not supported in coin selection";
                *eErrorCode=RPC_INTERNAL_ERROR;
                return false;
            }
        }
        if(addressRets.size()>0)
        {
            receive_required=addressRets.size();
            if(typeRet == TX_MULTISIG)
            {
                receive_required-=nRequiredRet;
                receive_required+=1;
                if(receive_required>(int)addressRets.size())
                {
                    receive_required=addressRets.size();
                }
            }
            
            CScript::const_iterator pc1 = s.first.begin();

            mc_gState->m_TmpScript->Clear();
            mc_gState->m_TmpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(s.first.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
            
            tmp_amounts->Clear();
            if(!mc_ExtractOutputAssetQuantities(tmp_amounts,strFailReason,true))   
            {
                *eErrorCode=RPC_INTERNAL_ERROR;
                return false;
            }
            if(!mc_VerifyAssetPermissions(tmp_amounts,addressRets,receive_required,MC_PTP_RECEIVE,strFailReason))
            {
                *eErrorCode=RPC_NOT_ALLOWED;
                return false;
            }
            
            fIsMaybePurePermission=true;
            fIsGenesis=false;
            for (int e = 0; e < mc_gState->m_TmpScript->GetNumElements(); e++)
            {
                mc_gState->m_TmpScript->SetElement(e);
                err=mc_gState->m_TmpScript->GetAssetGenesis(&quantity);
                if(err == 0)
                {
                    fIsGenesis=true;
                    fIsMaybePurePermission=false;
                }         
                err=mc_gState->m_TmpScript->GetRawData(NULL,NULL);              
                if(err == 0)
                {
                    fIsMaybePurePermission=false;
                }
            }

            if(tmp_amounts->GetCount())                                         
            {
                if(fIsGenesis)
                {
                    strFailReason="Asset issuance and asset transfer are not allowed in one output";
                    *eErrorCode=RPC_NOT_ALLOWED;
                    return false;                    
                }
                fIsMaybePurePermission=false;                
            }
            
            if(s.second > 0)
            {
                fIsMaybePurePermission=false;    
            }
            
            if(!fIsMaybePurePermission)
                
            {
                if( (s.second > 0) || 
                    (tmp_amounts->GetCount() > 0) ||
                    (mc_gState->m_Features->AnyoneCanReceiveEmpty() == 0) )
                {
                    for(int a=0;a<(int)addressRets.size();a++)
                    {                            
                        CKeyID *lpKeyID=boost::get<CKeyID> (&addressRets[a]);
                        CScriptID *lpScriptID=boost::get<CScriptID> (&addressRets[a]);
                        if((lpKeyID == NULL) && (lpScriptID == NULL))
                        {
                            strFailReason="Wrong destination type";
                            *eErrorCode=RPC_INTERNAL_ERROR;
                            return false;
                        }
                        unsigned char* ptr=NULL;
                        if(lpKeyID != NULL)
                        {
                            ptr=(unsigned char*)(lpKeyID);
                        }
                        else
                        {
                            ptr=(unsigned char*)(lpScriptID);
                        }

                        bool fCanReceive=mc_gState->m_Permissions->CanReceive(NULL,ptr);

                        if(fCanReceive)                        
                        {
                            receive_required--;
                        }                                    
                    }
                    if(receive_required>0)
                    {
                        strFailReason="One of the outputs doesn't have receive permission";
                        *eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                        return false;
                    }
                }
            }            
        }            
    }
    
    return true;
}

bool IsLicenseTokenIssuanceDetails(const CTxOut& txout,mc_Script *lpScript)
{
    uint32_t type;
    int update;
    unsigned char script[MC_ENT_MAX_SCRIPT_SIZE];;
    int script_size;    
    const CScript& script1 = txout.scriptPubKey;        
    CScript::const_iterator pc1 = script1.begin();
    lpScript->Clear();
    lpScript->SetScript((unsigned char*)(&pc1[0]),(size_t)(script1.end()-pc1),MC_SCR_TYPE_SCRIPTPUBKEY);
    if(lpScript->GetNumElements() == 0)
    {
        return false;
    }
        
    lpScript->SetElement(0);
    if(lpScript->GetNewEntityType(&type,&update,script,&script_size) == 0)
    {
        if(type == MC_ENT_TYPE_LICENSE_TOKEN)
        {
            return true;
        }
    }
    
    return false;
}


bool CreateAssetGroupingTransaction(CWallet *lpWallet, const vector<pair<CScript, CAmount> >& vecSend,
                                CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl,
                                const set<CTxDestination>* addresses,int min_conf,int min_inputs,int max_inputs,const vector<COutPoint>* lpCoinsToUse,uint32_t flags,int *eErrorCode)
{   
    double start_time=mc_TimeNowAsDouble();
    double last_time,this_time;
    last_time=start_time;
    
    this_time=mc_TimeNowAsDouble();
    if(csperf_debug_print)if(vecSend.size())printf("Start                                   \n");
    last_time=this_time;
    
    if(eErrorCode)*eErrorCode=RPC_INVALID_PARAMETER;
    CAmount nValue = 0;
    BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)
    {
        if (nValue < 0)                                                         // Multichain allows protocol zero-value outputs
        {
            strFailReason = _("Transaction amounts must be non-negative");
            return false;
        }
        nValue += s.second;
    }
//    if (vecSend.empty() || nValue < 0)
    if (nValue < 0)
    {
        strFailReason = _("Transaction amounts must be non-negative");
        return false;
    }
    
    nFeeRet=0;
    
    CWallet::minTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
    minRelayTxFee = CFeeRate(MIN_RELAY_TX_FEE);    
    
    mc_Buffer *out_amounts;                                         
    
    out_amounts=new mc_Buffer;
    mc_InitABufferMap(out_amounts);
        
    mc_Buffer *change_amounts;
    
    change_amounts=new mc_Buffer;
    change_amounts->Initialize(MC_AST_ASSET_QUANTITY_OFFSET,MC_AST_ASSET_FULLREF_BUF_SIZE+sizeof(int),MC_BUF_MODE_MAP);
    
    mc_Buffer *tmp_amounts;
    
    tmp_amounts=new mc_Buffer;
    mc_InitABufferMap(tmp_amounts);
    
    mc_Script *lpScript;
    lpScript=new mc_Script;    

    mc_Buffer *in_amounts;
    in_amounts=NULL;
    
    mc_Buffer *in_map;
    in_map=NULL;
    
    unsigned char *in_row;
    int required;
    int special_required=MC_PTP_ADMIN | MC_PTP_ACTIVATE | MC_PTP_ISSUE | MC_PTP_CREATE | MC_PTP_WRITE;
    uint256 hash;
    int32_t type;
    bool no_send_coins;
    bool inline_coins;
    vector<COutput> vCoins;
    CTxDestination change_address;
    CAmount min_output;
    CAmount missing_amount;
    CAmount nTotalOutValue;
    CMutableTransaction txNew;
    map<uint32_t, uint256> mapSpecialEntity;
    set<CTxDestination> usedAddresses;
    bool skip_error_message=false;
    
    in_row=NULL;
    out_amounts->Clear();

    hash=0;
    
    required=0;
    if(vecSend.size())
    {
        if(!CheckOutputPermissions(vecSend,tmp_amounts,strFailReason,eErrorCode))
        {
            goto exitlbl;
        }
                
        required=0;
        if( (addresses == NULL) || (addresses->size() != 1) )
        {
            BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)            // Filling buffer with output amounts
            {
                CTxOut txout(s.second, s.first);
                int this_required=MC_PTP_ALL;
                if(!ParseMultichainTxOutToBuffer(hash,txout,out_amounts,lpScript,NULL,&this_required,&mapSpecialEntity,strFailReason))
                {
                    goto exitlbl;
                }
                required |= this_required;
            }
        }    
        else
        {
            set<CTxDestination>::const_iterator it = addresses->begin();
            CTxDestination address_from=*it;        
            BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)            // Filling buffer with output amounts
            {
                CTxOut txout(s.second, s.first);
                int this_required=MC_PTP_SEND | MC_PTP_RECEIVE;
                if(!ParseMultichainTxOutToBuffer(hash,txout,out_amounts,lpScript,NULL,&this_required,&mapSpecialEntity,strFailReason))
                {
                    goto exitlbl;
                }                
                if(this_required & special_required)
                {
                    int this_allowed=CheckRequiredPermissions(address_from,this_required & special_required,&mapSpecialEntity,&strFailReason);
                    this_allowed &= (this_required & special_required);
                    if( this_allowed   != (this_required & special_required) )
                    {                        
                        goto exitlbl;                        
                    }
                    this_required -= (this_required & special_required);
                    mapSpecialEntity.clear();
                }
                required |= this_required;
            }
        }
        if(required & MC_PTP_ISSUE)
        {
            BOOST_FOREACH (const PAIRTYPE(CScript, CAmount)& s, vecSend)   
            {                
                CTxOut txout(s.second, s.first);
                if(IsLicenseTokenIssuanceDetails(txout,lpScript))
                {
                    if(required & MC_PTP_ISSUE)
                    {
                        required-=MC_PTP_ISSUE;
                    }
                }
            }            
            if( (required & MC_PTP_ISSUE) == 0)
            {
                for(int i=0;i<out_amounts->GetCount();i++)
                {
                    if(mc_GetABRefType(out_amounts->GetRow(i)) == MC_AST_ASSET_REF_TYPE_SPECIAL)
                    {
                        switch(mc_GetLE(out_amounts->GetRow(i)+4,4))
                        {
                            case MC_PTP_ISSUE | MC_PTP_SEND:
                                mc_SetABQuantity(out_amounts->GetRow(i),0);                                
                                break;
                        }                    
                    }
                }                
            }
        }        
    }
    else
    {
        required=MC_PTP_SEND;
        CTxOut txout(0, CScript());
        if(!ParseMultichainTxOutToBuffer(hash,txout,out_amounts,lpScript,NULL,&required,strFailReason))
        {
            goto exitlbl;
        }        
    }

    if(debug_print)printf("debg: Outputs: (required %d)\n",required);
    if(fDebug)LogPrint("mcatxo","mcatxo: ====== New transaction, required %d\n",required);
    for(int i=0;i<out_amounts->GetCount();i++)
    {
        if(debug_print)DebugPrintAssetTxOut(0,0,out_amounts->GetRow(i),mc_GetABQuantity(out_amounts->GetRow(i)));
        if(fDebug)LogAssetTxOut("Output: ",0,0,out_amounts->GetRow(i),mc_GetABQuantity(out_amounts->GetRow(i)));
    }

    this_time=mc_TimeNowAsDouble();
    if(csperf_debug_print)if(vecSend.size())printf("Output                  : %8.6f\n",this_time-last_time);
    last_time=this_time;
    
                                                                                // Getting available coin list   
    AvalableCoinsForAddress(lpWallet,vCoins,coinControl,addresses,flags);

    this_time=mc_TimeNowAsDouble();
    if(csperf_debug_print)if(vecSend.size())printf("Available               : %8.6f\n",this_time-last_time);
    if(fDebug)LogPrint("mcperf","Coin Selection, available coins time: %8.6f\n",this_time-last_time);
    last_time=this_time;
    
    int in_special_row[10];
    int in_size;
                                                                                // Input coin matrix
                                                                                // Rows - assets, columns - coins
    in_size=MC_AST_ASSET_QUANTITY_OFFSET+vCoins.size()*MC_AST_ASSET_QUANTITY_SIZE;
    in_amounts=new mc_Buffer;
    in_amounts->Initialize(MC_AST_ASSET_QUANTITY_OFFSET,in_size,MC_BUF_MODE_MAP);

    this_time=mc_TimeNowAsDouble();
    if(csperf_debug_print)if(vecSend.size())printf("Alloc                   : %8.6f (%d)\n",this_time-last_time,in_size);
    last_time=this_time;
                                                                                // Map coin -> index in in_amounts
                                                                                // We need this as in SelectMultiChainCoinsMinConf coins will be shuffled
                                                                                // Key: 32-byte txid, 4-byte output id. Value - index in in_amounts
    in_map=new mc_Buffer;
    in_map->Initialize(36,40,MC_BUF_MODE_MAP);
    
    in_row=(unsigned char*)mc_New(in_size);
    
        
    for(int i=0;i<10;i++)
    {
        in_special_row[i]=-1;
    }
                                                                                    // Special row, if coin is already selected (or not used at all) the value is 1
    in_special_row[0]=in_amounts->GetCount();
    memset(in_row,-1,in_size);
    memset(in_row,0,MC_AST_ASSET_QUANTITY_OFFSET);
    type=0;
    mc_PutLE(in_row+4,&type,4);
    mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
    in_amounts->Add(in_row);
    
                                                                                // Special row, if coin is taken for selection the value is 1
    in_special_row[1]=in_amounts->GetCount();
    memset(in_row,0,in_size);
    type=0;
    mc_PutLE(in_row+4,&type,4);
    mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
    in_amounts->Add(in_row);
    
                                                                                // Special row, saved selection
    in_special_row[2]=in_amounts->GetCount();
    memset(in_row,0,in_size);
    type=0;
    mc_PutLE(in_row+4,&type,4);
    mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
    in_amounts->Add(in_row);
    
                                                                                // Native currency row, coin value if input has send permission, 0 otherwise
    in_special_row[3]=in_amounts->GetCount();
    memset(in_row,0,in_size);
    type=MC_PTP_SEND;
    mc_PutLE(in_row+4,&type,4);
    mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
    in_amounts->Add(in_row);
    
                                                                                // Pure native currency flag, 1 if n o other assets were found in the coin
    in_special_row[4]=in_amounts->GetCount();
    memset(in_row,0,in_size);
    type=MC_PTP_SEND;
    mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
    mc_PutLE(in_row+4,&type,4);
    in_amounts->Add(in_row);
    
                                                                                // Issue row, coin value if input has issue permission, 0 otherwise
    if(required & MC_PTP_ISSUE)
    {
        in_special_row[5]=in_amounts->GetCount();
        memset(in_row,0,in_size);
        type=MC_PTP_ISSUE | MC_PTP_SEND;
        mc_PutLE(in_row+4,&type,4);
        mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
        in_amounts->Add(in_row);
    }
    
                                                                                // Admin row, coin value if input has admin permission, 0 otherwise
    if(required & MC_PTP_ADMIN)
    {
        in_special_row[6]=in_amounts->GetCount();
        memset(in_row,0,in_size);
        type=MC_PTP_ADMIN | MC_PTP_SEND;
        mc_PutLE(in_row+4,&type,4);
        mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
        in_amounts->Add(in_row);
    }
    
                                                                                // Activate row, coin value if input has activate permission, 0 otherwise
    if(required & MC_PTP_ACTIVATE)
    {
        in_special_row[7]=in_amounts->GetCount();
        memset(in_row,0,in_size);
        type=MC_PTP_ACTIVATE | MC_PTP_SEND;
        mc_PutLE(in_row+4,&type,4);
        mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
        in_amounts->Add(in_row);
    }
    
                                                                                // Write row, coin value if input has write permission, 0 otherwise
    if(required & MC_PTP_WRITE)
    {
        in_special_row[8]=in_amounts->GetCount();
        memset(in_row,0,in_size);
        type=MC_PTP_WRITE | MC_PTP_SEND;
        mc_PutLE(in_row+4,&type,4);
        mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
        in_amounts->Add(in_row);
    }
    
                                                                                // Open row, coin value if input has open permission, 0 otherwise
    if(required & MC_PTP_CREATE)
    {
        in_special_row[9]=in_amounts->GetCount();
        memset(in_row,0,in_size);
        type=MC_PTP_CREATE | MC_PTP_SEND;
        mc_PutLE(in_row+4,&type,4);
        mc_SetABRefType(in_row,MC_AST_ASSET_REF_TYPE_SPECIAL);
        in_amounts->Add(in_row);
    }
    
    this_time=mc_TimeNowAsDouble();
    if(csperf_debug_print)if(vecSend.size())printf("Initialize              : %8.6f\n",this_time-last_time);
    last_time=this_time;
    
    {
        LOCK2(cs_main, lpWallet->cs_wallet);
        {
            this_time=mc_TimeNowAsDouble();
            if(csperf_debug_print)if(vecSend.size())printf("Lock                    : %8.6f\n",this_time-last_time);
            last_time=this_time;
    
            no_send_coins=false;
            inline_coins=false;
            if(vecSend.size())                                                  // Normal transaction
            {
                                                                                // Find coins for relevant assets
                if(!FindRelevantCoins(lpWallet,vCoins,out_amounts,required,&no_send_coins,&inline_coins,in_amounts,in_map,tmp_amounts,in_row,in_size,lpScript,in_special_row,&mapSpecialEntity,strFailReason))
                {
                    goto exitlbl;
                }
                
                this_time=mc_TimeNowAsDouble();
                if(csperf_debug_print)if(vecSend.size())printf("Inputs                  : %8.6f\n",this_time-last_time);
                if(fDebug)LogPrint("mcperf","mcperf: CS: Input Parsing: Time: %8.6f \n", this_time-last_time);
                last_time=this_time;

                if(!SelectCoinsToUse(lpCoinsToUse,in_map,in_amounts,in_special_row,strFailReason))
                {
                    goto exitlbl;
                }
                
                this_time=mc_TimeNowAsDouble();
                if(csperf_debug_print)if(vecSend.size())printf("Select                  : %8.6f\n",this_time-last_time);
                if(fDebug)LogPrint("mcperf","mcperf: CS: Input Select: Time: %8.6f \n", this_time-last_time);
                last_time=this_time;
    
                for(int asset=0;asset<out_amounts->GetCount();asset++)
                {
                    if(mc_GetABRefType(out_amounts->GetRow(asset)) != MC_AST_ASSET_REF_TYPE_GENESIS)
                    {
                        int in_asset_row;
                        nTotalOutValue=mc_GetABQuantity(out_amounts->GetRow(asset));
                        in_asset_row=in_amounts->Seek(out_amounts->GetRow(asset));
        
                        if((nTotalOutValue > 0) ||                              // Can send 0 amount of specific asset if there are no outputs carrying this asset
                             ((mc_GetABRefType(out_amounts->GetRow(asset)) == MC_AST_ASSET_REF_TYPE_SPECIAL) &&
                             (mc_GetLE(out_amounts->GetRow(asset)+4,4) == MC_PTP_SEND)))// But not for native currency, otherwise we can get "no inputs"                                  
                        {
                            if(!SelectAssetCoins(lpWallet,nTotalOutValue,vCoins,in_map,in_amounts,in_special_row,in_asset_row,coinControl))
                            {
                                strFailReason = _("Insufficient funds.");
                                if(eErrorCode)*eErrorCode=RPC_WALLET_INSUFFICIENT_FUNDS;
                                if(mc_GetABRefType(out_amounts->GetRow(asset)) == MC_AST_ASSET_REF_TYPE_SPECIAL)
                                {
                                    switch(mc_GetLE(out_amounts->GetRow(asset)+4,4))
                                    {
                                        case MC_PTP_ISSUE | MC_PTP_SEND:
                                            if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                                            strFailReason = _("No unspent output with issue permission.");                                         
                                            break;
                                        case MC_PTP_CREATE | MC_PTP_SEND:
                                            if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                                            strFailReason = _("No unspent output with create permission.");                                         
                                            break;
                                        case MC_PTP_ACTIVATE | MC_PTP_SEND:
                                            if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                                            strFailReason = _("No unspent output with activate or admin permission.");                                         
                                            break;
                                        case MC_PTP_ADMIN | MC_PTP_SEND:
                                            if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                                            strFailReason = _("No unspent output with admin permission.");                                         
                                            break;
                                        case MC_PTP_WRITE | MC_PTP_SEND:
                                            if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                                            strFailReason = _("No unspent output with write permission.");                                         
                                            break;
                                        case MC_PTP_SEND:
                                            if(nTotalOutValue > 0)
                                            {                                                
                                                if(no_send_coins)
                                                {
                                                    if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                                                    strFailReason = _("Insufficient funds, but there are unspent outputs belonging to addresses without send or receive permission.");
                                                }                                                                                    
                                            }
                                            else
                                            {                        
                                                if(eErrorCode)*eErrorCode=RPC_WALLET_NO_UNSPENT_OUTPUTS;
                                                if( MCP_WITH_NATIVE_CURRENCY )
                                                {
                                                    strFailReason=_("No unspent outputs are available. Please send a transaction to this node or address first and wait for its confirmation."); 
                                                }
                                                else
                                                {
                                                    strFailReason=_("No unspent outputs are available. Please send a transaction, with zero amount, to this node or address first and wait for its confirmation.");                                                     
                                                }
/*
                                                if(required & MC_PTP_WRITE)     // publish always comes with addresses set, SEND fails before write
                                                {
                                                    strFailReason = _("No unspent output with write permission");                                                                                                 
                                                }
                                                else
                                                {
                                                    if( (addresses != NULL) && (addresses->size() == 1) )
                                                    {
                                                        strFailReason = _("No unspent output from this address");                                                                                      
                                                    }
                                                    else
                                                    {
                                                        strFailReason = _("No unspent outputs found in this wallet");                                                                                                                                  
                                                    }
                                                }
 */ 
                                            }
                                            break;
                                    }
                                }
                                else
                                {
                                    if(no_send_coins)
                                    {
                                        if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                                        strFailReason = _("Insufficient funds, but there are unspent outputs belonging to addresses without send or receive permission.");
                                    }                                    
                                }
                                
                                if(inline_coins)
                                {
                                    strFailReason += " There are outputs with inline metadata (lockinlinemetadata runtime parameter).";
                                }                                
                                goto exitlbl;                            
                            }                             
                        }                                                        
                    }
                }                
    
                this_time=mc_TimeNowAsDouble();
                if(csperf_debug_print)if(vecSend.size())printf("Select                  : %8.6f\n",this_time-last_time);
                if(fDebug)LogPrint("mcperf","mcperf: CS: Coin Selection: Time: %8.6f \n", this_time-last_time);
                last_time=this_time;                
            }
            else
            {
                if(!FindCoinsToCombine(lpWallet,vCoins,min_conf,min_inputs,max_inputs,in_amounts,in_map,tmp_amounts,in_row,in_size,lpScript,in_special_row,strFailReason))
                {
                                                                                // We didn't find enough coins to combine or error occurred
                    skip_error_message=true;
                    goto exitlbl;
                }                
            }
            
                                                                                // Calculate change
            if(!CalculateChangeAmounts(lpWallet,vCoins,nValue,out_amounts,required,in_amounts,change_amounts,tmp_amounts,lpScript,in_special_row,&mapSpecialEntity,&usedAddresses,strFailReason))
            {
                goto exitlbl;        
            }
            

                                                                                // Find change address
                                                                                //mAy be we don't need the change so the error is ignored
            FindChangeAddress(lpWallet,change_address,&usedAddresses,reservekey,coinControl,required,&mapSpecialEntity);
            nFeeRet = 0;
            
            min_output=-1;                                                      // Calculate minimal output for the change
            if(mc_gState->m_NetworkParams->IsProtocolMultichain())
            {
                min_output=MCP_MINIMUM_PER_OUTPUT;
            }            
                                                                                            // Storing selection before pure-native 
            memcpy(in_amounts->GetRow(in_special_row[2]),in_amounts->GetRow(in_special_row[0]),in_size);
            nTotalOutValue=nValue;
            
            this_time=mc_TimeNowAsDouble();
            if(csperf_debug_print)if(vecSend.size())printf("Address                 : %8.6f\n",this_time-last_time);
            last_time=this_time;

            missing_amount=BuildAssetTransaction(lpWallet,wtxNew,change_address,nFeeRet,vecSend,vCoins,in_amounts,change_amounts,required,min_output,tmp_amounts,lpScript,in_special_row,&usedAddresses,flags,strFailReason);
            if(missing_amount<0)                                                // Error
            {
                goto exitlbl; 
            }
            
            this_time=mc_TimeNowAsDouble();
            if(csperf_debug_print)if(vecSend.size())printf("Build                   : %8.6f\n",this_time-last_time);
            if(fDebug)LogPrint("mcperf","mcperf: Transaction Building: Time: %8.6f \n", this_time-last_time);
            last_time=this_time;
            
            
            while(missing_amount > 0)                                           // We still need more native currency
            {
                if(vecSend.size() == 0)                                         // We cannot select new coins as only combined outputs are parsed
                {
                    if(eErrorCode)*eErrorCode=RPC_WALLET_INSUFFICIENT_FUNDS;
                    strFailReason = _("Combine transaction requires extra native currency amount");
                    goto exitlbl;                                                
                }
                                                                                // Restoring pure-asset selection
                memcpy(in_amounts->GetRow(in_special_row[0]),in_amounts->GetRow(in_special_row[2]),in_size);
                nTotalOutValue+=missing_amount;
                                                                                // Select coins to cover native currency output and fee
                if(!SelectAssetCoins(lpWallet,nTotalOutValue,vCoins,in_map,in_amounts,in_special_row,in_special_row[3],coinControl))
                {
                    strFailReason = _("Insufficient funds");
                    if(eErrorCode)*eErrorCode=RPC_WALLET_INSUFFICIENT_FUNDS;
                    if(no_send_coins)
                    {
                        if(eErrorCode)*eErrorCode=RPC_INSUFFICIENT_PERMISSIONS;
                        strFailReason = _("Insufficient funds, but there are coins belonging to addresses without send permission.");
                    }
                    goto exitlbl;                            
                }                        
                                                                                // Calculate new change matrix
                if(!CalculateChangeAmounts(lpWallet,vCoins,nTotalOutValue,out_amounts,required,in_amounts,change_amounts,tmp_amounts,lpScript,in_special_row,&mapSpecialEntity,&usedAddresses,strFailReason))
                {
                    goto exitlbl;        
                }
                
                                                                                // Try to build transaction again
                missing_amount=BuildAssetTransaction(lpWallet,wtxNew,change_address,nFeeRet,vecSend,vCoins,in_amounts,change_amounts,required,min_output,tmp_amounts,lpScript,in_special_row,&usedAddresses,flags,strFailReason);                
                if(missing_amount<0)                                            // Error
                {
                    if(eErrorCode)*eErrorCode=RPC_WALLET_INSUFFICIENT_FUNDS;
                    goto exitlbl; 
                }
            }            
        }
    }
    
exitlbl:

    if(in_map)
    {
        delete in_map;
    }

    if(in_row)
    {
        mc_Delete(in_row);
    }

    if(in_amounts)
    {
        delete in_amounts;
    }
            
    delete lpScript;
    delete tmp_amounts;
    delete change_amounts;
    delete out_amounts;

    
    lpWallet->lpAssetGroups->Dump();
    
    if(strFailReason.size())
    {
        if(fDebug)LogPrint("mcatxo","mcatxo: ====== Error: %s\n",strFailReason.c_str());
        if(!skip_error_message)
        {
            LogPrintf("mchn: Coin selection: %s\n",strFailReason.c_str());
        }
        return false;
    }
    
    this_time=mc_TimeNowAsDouble();
    if(csperf_debug_print)if(vecSend.size())printf("CS Exit                 : %8.6f\n",this_time-last_time);
    last_time=this_time;
    
    if(fDebug)LogPrint("mcatxo","mcatxo: ====== Created: %s\n",wtxNew.GetHash().ToString().c_str());
    return true;
}

bool CWallet::CreateMultiChainTransaction(const vector<pair<CScript, CAmount> >& vecSend,
                                CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl,
                                const set<CTxDestination>* addresses,int min_conf,int min_inputs,int max_inputs,const vector<COutPoint>* lpCoinsToUse, int *eErrorCode)
{
    if( (lpAssetGroups != NULL) && (lpAssetGroups != 0) )
    {
        return CreateAssetGroupingTransaction(this, vecSend,wtxNew,reservekey,nFeeRet,strFailReason,coinControl,addresses,min_conf,min_inputs,max_inputs,lpCoinsToUse,MC_CSF_ALLOW_SPENDABLE_P2SH | MC_CSF_SIGN, eErrorCode);        
    } 
    return true;
}

bool CWallet::UpdateUnspentList(const CWalletTx& wtx, bool update_inputs)
{
    const CWalletTx* pcoin = &wtx;
    const uint256& wtxid = pcoin->GetHash();

    int unspent_count=0;
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, not used in this case

    int nDepth = pcoin->GetDepthInMainChain();
    if (nDepth >= 0)
    {
        for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
            isminetype mine = IsMine(pcoin->vout[i]);
            if (!(IsSpent(wtxid, i)) && mine != ISMINE_NO)
            {
                unspent_count++;
            }
        }
    }

    std::map<uint256, int>::const_iterator mit = mapUnspent.find(wtxid);

    if(unspent_count)
    {
        if (mit == mapUnspent.end())
        {
            mapUnspent.insert(make_pair(wtxid, unspent_count));
        }
    }            
    else
    {
        if (mit != mapUnspent.end())
        {
            mapUnspent.erase(wtxid);
        }            
    }

    if(update_inputs)
    {
        BOOST_FOREACH(const CTxIn& txin, wtx.vin)
        {
            std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(txin.prevout.hash);

            if (it != mapWallet.end())
            {
                UpdateUnspentList(it->second,false);
            }                
        }           
    }

    return true;
}

bool CWallet::InitializeUnspentList()
{
    LOCK2(cs_main, cs_wallet);
    mapUnspent.clear();

    int asset_count=0;
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, not used in this case

    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const uint256& wtxid = it->first;
        const CWalletTx* pcoin = &(*it).second;

        int unspent_count=0;

        int nDepth = pcoin->GetDepthInMainChain();
        if (nDepth >= 0)
        {
            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                isminetype mine = IsMine(pcoin->vout[i]);
                if (!(IsSpent(wtxid, i)) && (mine & ISMINE_SPENDABLE) != ISMINE_NO)
                {
                    unspent_count++;
                }
            }
        }

        if(unspent_count)
        {
            mapUnspent.insert(make_pair(wtxid, unspent_count));
        }            
    }

    if(lpAssetGroups)
    {
        delete lpAssetGroups;
        lpAssetGroups=NULL;
    }

    lpAssetGroups=new CAssetGroupTree;

    int assets_per_opdrop;

    assets_per_opdrop=(MAX_SCRIPT_ELEMENT_SIZE-4)/(mc_gState->m_NetworkParams->m_AssetRefSize+MC_AST_ASSET_QUANTITY_SIZE);

    int max_assets_per_group=assets_per_opdrop*MCP_STD_OP_DROP_COUNT;

    lpAssetGroups->Initialize(1,max_assets_per_group,32,1);
    
    vector <COutput> vCoins;

    mc_Buffer *tmp_amounts;

    tmp_amounts=new mc_Buffer;
    mc_InitABufferMap(tmp_amounts);

    mc_Script *lpScript;
    lpScript=new mc_Script;    
    if( (tmp_amounts != NULL) && (lpScript != NULL) )                       // Count assets in unspent coins
    {
        AvailableCoins(vCoins, true, NULL,true,true);            
        sort(vCoins.begin(),vCoins.end(),CompareCOutputByDepthAndScriptSizeDesc());
        tmp_amounts->Clear();
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            string strError;
            CTxOut txout;
            uint256 hash=out.GetHashAndTxOut(txout);

            ParseMultichainTxOutToBuffer(hash,txout,tmp_amounts,lpScript,NULL,NULL,strError);
        }
        asset_count=tmp_amounts->GetCount();
        if(mc_gState->m_Features->PerAssetPermissions())
        {
            mc_EntityDetails entity;
            for(int i=0;i<tmp_amounts->GetCount();i++)
            {
                if(mc_gState->m_Assets->FindEntityByFullRef(&entity,tmp_amounts->GetRow(i)))
                {
                    if( entity.Permissions() & (MC_PTP_SEND | MC_PTP_RECEIVE) )
                    {
                        asset_count--;
                    }
                }
            }
        }
        if(asset_count > 0)                                                     // Resize asset grouping to prevent crazy autocombine on 
                                                                                 // already autocombined with higher assets-per-group setting   
        {
            lpAssetGroups->Resize(asset_count);
        }
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            string strError;

            CTxOut txout;
            uint256 hash=out.GetHashAndTxOut(txout);
            tmp_amounts->Clear();
            ParseMultichainTxOutToBuffer(hash,txout,tmp_amounts,lpScript,NULL,NULL,strError);
            lpAssetGroups->GetGroup(tmp_amounts,1);
        }
        if(fDebug)LogPrint("mchn","mchn: Found %d assets in %d groups\n",asset_count,lpAssetGroups->GroupCount()-1);
        lpAssetGroups->Dump();
    }        

    if(tmp_amounts)
    {
        delete tmp_amounts;
    }

    if(lpScript)
    {
        delete lpScript;
    }


    if(fDebug)LogPrint("mchn","mchn: Unspent list initialized: Total: %d, Unspent: %d\n",mapWallet.size(),mapUnspent.size());

    return true;        
}

void CWallet::PurgeSpentCoins(int min_depth,int max_coins)   
{
    int unspent_count=0;
    int skipped=0;
    int count=0;
    int total;
    
//    if(mc_gState->m_WalletMode & MC_WMD_ADDRESS_TXS)                          // Not supported, not used
    
    double this_time,last_time;
    this_time=mc_TimeNowAsDouble();
    last_time=this_time;
    
    set<uint256> should_keep;
    for (map<uint256, int>::const_iterator itUnspent = mapUnspent.begin(); itUnspent != mapUnspent.end(); ++itUnspent)
    {
        const uint256& wtxid = itUnspent->first;
        if(should_keep.count(wtxid) == 0)
        {
            should_keep.insert(wtxid);
        }        
/*
        std::map<uint256, CWalletTx>::const_iterator it = mapWallet.find(wtxid);

        if (it != mapWallet.end())
        {
            const CWalletTx* pcoin = &(*it).second;
            BOOST_FOREACH(const CTxIn& txin, pcoin->vin)
            {
                if(mapWallet.count(txin.prevout.hash))
                {
                    if(should_keep.count(txin.prevout.hash) == 0)
                    {
                        should_keep.insert(txin.prevout.hash);
                    }        
                }
            }
        }
*/
    }    
    
    vector <uint256> to_erase;
    to_erase.resize(max_coins);
    
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const uint256& wtxid = it->first;
//        std::map<uint256, int>::const_iterator itUnspent = mapUnspent.find(wtxid);
                
//        if (itUnspent == mapUnspent.end())
        if(should_keep.count(wtxid) == 0)
        {
            const CWalletTx* pcoin = &(*it).second;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth > min_depth)
            {
                to_erase[count]=wtxid;
                count++;
            }
            else
            {
                skipped++;
            }

            if(count>=max_coins)
            {
                goto exitlbl;
            }
        }            
        else
        {
            unspent_count++;            
            skipped++;
        }
        

    }    

    
    
exitlbl:    

    total=mapWallet.size();
    for(int i=0;i<count;i++)
    {
        const uint256& wtxid = to_erase[i];
        setPurged.insert(wtxid);
        EraseFromWallet(wtxid);
    }
        
    this_time=mc_TimeNowAsDouble();
    if(fDebug)LogPrint("mchn","mchn: Wallet coins: Total: %d, Unspent: %d, Kept: %d, Purged: %d, Skipped: %d, Time: %8.6f\n",total,mapUnspent.size(),should_keep.size(),count,skipped,this_time-last_time);
}
    

uint256 COutput::GetHashAndTxOut(CTxOut& txout) const
{
    if(tx)
    {        
        txout=tx->vout[i];
        return tx->GetHash();
    }
    txout=coin.m_TXOut;
    return coin.m_OutPoint.hash;
}

bool COutput::IsTrusted() const
{
    if(tx)
    {        
        return tx->IsTrusted(nDepth);
    }
    return coin.IsTrusted();
}

bool COutput::IsTrustedNoDepth() const
{
    if(tx)
    {                
        return tx->IsTrusted((nDepth >= 0) ? 0 : -1);
    }
    return coin.IsTrustedNoDepth();
}


bool OutputCanSend(COutput out)
{            
    if(mc_gState->m_NetworkParams->IsProtocolMultichain())
    {
        if(MCP_ANYONE_CAN_SEND == 0)
        {
            CTxOut txout;
            out.GetHashAndTxOut(txout);
            const CScript& script1 = txout.scriptPubKey;        
            CTxDestination addressRet;        
            if(ExtractDestinationScriptValid(script1, addressRet))
            {
                CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);
                if(lpKeyID != NULL)
                {
                    if(!mc_gState->m_Permissions->CanSend(NULL,(unsigned char*)(lpKeyID)))
                    {
                        return false;
                    }                            
                }
                else                            
                {
                    return false;
                }

            }
            else
            {
                return false;
            }                
        }
    }
            
    return true;
}


bool CWallet::CreateAndCommitOptimizeTransaction(CWalletTx& wtx,std::string& strFailReason,const std::set<CTxDestination>* addresses,int min_conf,int min_inputs,int max_inputs)
{
    CReserveKey reservekey(this);
    
    LOCK(cs_wallet_send);
    
    std::vector <CScript> scriptPubKeys;
    CScript scriptOpReturn=CScript();
    CAmount nValue=0;    
    CAmount nFeeRequired;
        
    if (!CreateTransaction(scriptPubKeys, nValue, scriptOpReturn, wtx, reservekey, nFeeRequired, strFailReason, NULL, addresses, min_conf, min_inputs, max_inputs))
    {
        if (nValue + nFeeRequired > GetBalance())
            strFailReason = strprintf("This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
    }
    else
    {    
        if(fDebug)LogPrint("mchn","Committing wallet optimization tx. Inputs: %ld, Outputs: %ld\n",wtx.vin.size(),wtx.vout.size());
        
        if (CommitTransaction(wtx, reservekey, strFailReason))
        {
            if(fDebug)LogPrint("mchn","Committing wallet optimization tx completed\n");
            return true;
        }
    }    
    
    return false;
}

/*
 * Should be called under cs_main and cs_wallet
 */


int CWallet::OptimizeUnspentList()
{
    if(mc_TimeNowAsUInt()<nNextUnspentOptimization)
    {
        return false;
    }

    if(fDebug)LogPrint("mchn","mchn: Wallet optimization\n");
    
    double start_time=mc_TimeNowAsDouble();
    
    int min_conf=GetArg("-autocombineminconf", 1);
    int min_inputs=GetArg("-autocombinemininputs", 50);
    int max_inputs=GetArg("-autocombinemaxinputs", 100);
    int next_delay=GetArg("-autocombinedelay", 1);
    int min_outputs=min_inputs;
    int max_combine_txs=2000*Params().TargetSpacing();
    if(max_inputs > 0)
    {
        max_combine_txs /= max_inputs;
    }
    if(max_combine_txs < 1)
    {
        max_combine_txs=1;
    }
    max_combine_txs=GetArg("-autocombinemaxtxs", max_combine_txs);

    vector<COutput> vCoins;        
    AvailableCoins(vCoins, true, NULL,true,true);
        
    map <CTxDestination, int> addressesToOptimize;
    vector <int> txOutCounts;
    int pos;
    int total=0;
//    PurgeSpentCoins(8,1000);
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        const CScript& script1 = out.coin.m_TXOut.scriptPubKey;        
        if(out.fSpendable)
        {
            CTxDestination addressRet;        
            if(ExtractDestinationScriptValid(script1, addressRet))
            {
                CKeyID *lpKeyID=boost::get<CKeyID> (&addressRet);        

                if(lpKeyID)
                {
                    if (out.nDepth >= (((out.coin.m_Flags & MC_TFL_FROM_ME)) > 0 ? min_conf : ((min_conf == 0) ? 1 : min_conf)))
                    {
                        map <CTxDestination, int>::iterator mi=addressesToOptimize.find(addressRet);
                        if (mi != addressesToOptimize.end())
                        {
                            pos=mi->second;
                            txOutCounts[pos]+=1;
                        }
                        else
                        {
                            pos=txOutCounts.size();
                            addressesToOptimize.insert(pair<CTxDestination, int>(addressRet, pos));
                            txOutCounts.push_back(1);
                        }
                        total++;
                    }
                }
            }
        }                        
    }        
           
    int tx_sent=0;
    
    if(fDebug)LogPrint("mchn","mchn: Found %d UTXOs in %d addresses\n",total,(int)addressesToOptimize.size());
    
    while( (tx_sent<max_combine_txs) && (total > 0) )
    {
        start_time=mc_TimeNowAsDouble();
        bool result=false;
        total=0;
        
        vector<CTxDestination> vAddresses;        
        for (map<CTxDestination, int>::iterator it = addressesToOptimize.begin();
             it != addressesToOptimize.end();
             ++it)
        {
            const unsigned char *aptr;

            aptr=GetAddressIDPtr(it->first);
            if(aptr)
            {
                if(mc_gState->m_Permissions->CanSend(NULL,aptr))
                {
                    if(mc_gState->m_Permissions->CanReceive(NULL,aptr)) 
                    {
                        if(txOutCounts[it->second] >= min_outputs)
                        {
                            vAddresses.push_back(it->first);
                            total+=txOutCounts[it->second];
                            if(fDebug)LogPrint("mchn","mchn: Optimization required for address %s: %d UTXOs\n",CBitcoinAddress(it->first).ToString().c_str(),txOutCounts[it->second]);
                        }
                    }
                }
            }
        }   

        if(vAddresses.size())
        {
            random_shuffle(vAddresses.begin(), vAddresses.end(), GetRandInt);

            BOOST_FOREACH (const CTxDestination& dest, vAddresses)
            {
                if(!result)
                {
                    if(fDebug)LogPrint("mchn","mchn: Performing optimization for address %s\n",CBitcoinAddress(dest).ToString().c_str());
                    
                    map <CTxDestination, int>::iterator it=addressesToOptimize.find(dest);
                    if (it != addressesToOptimize.end())
                    {
                        if(txOutCounts[it->second] >= min_outputs)
                        {
                            set<CTxDestination> thisAddresses;
                            set<CTxDestination>* lpAddresses; 
                            string strError;

                            thisAddresses.clear();
                            thisAddresses.insert(it->first);
                            lpAddresses=&thisAddresses;

                            const CKeyID *lpKeyID=boost::get<CKeyID> (&(it->first));        
                            CBitcoinAddress bitcoin_address=CBitcoinAddress(*lpKeyID);
                            if(lpKeyID)
                            {
                                CWalletTx wtx;
                                result=CreateAndCommitOptimizeTransaction(wtx,strError,lpAddresses,min_conf,min_inputs,max_inputs);
                                if(result)
                                {
                                    LogPrintf("Combine transaction for address %s (%d inputs,%d outputs): %s; Time: %8.3fs\n",
                                            bitcoin_address.ToString().c_str(), wtx.vin.size(),wtx.vout.size(),wtx.GetHash().GetHex().c_str(),mc_TimeNowAsDouble()-start_time);
                                    total-=txOutCounts[it->second];
                                    txOutCounts[it->second]-=wtx.vin.size();
                                    txOutCounts[it->second]+=wtx.vout.size();
                                    total+=txOutCounts[it->second];
                                    tx_sent++;
                                }
                                else
                                {
                                    txOutCounts[it->second]=0;                                    
                                }
                            }
                        }
                    }
                }
            }
        }
    }
        
    nNextUnspentOptimization=mc_TimeNowAsUInt()+next_delay;
    return tx_sent;
}
