// Copyright (c) 2014-2019 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#include "multichain/multichain.h"

#include "chainparams/chainparams.h"
#include "utils/util.h"
#include "utils/utilstrencodings.h"
#include "structs/hash.h"

#include "utils/random.h"
#include "structs/base58.h"

using namespace std;

void mc_GetCompoundHash160(void *result,const void  *hash1,const void  *hash2)
{
    unsigned char conc_subkey[2*20];
    
    memcpy(conc_subkey,hash1,20);
    memcpy(conc_subkey+20,hash2,20);
    *(uint160*)result= Hash160(conc_subkey,conc_subkey+40);    
}

int mc_RandomEncodedBase58String(char * dest,int size)
{
//    GetRandBytes((unsigned char*)dest, size);
    GetStrongRandBytes((unsigned char*)dest, size);
    string str=EncodeBase58((unsigned char*)(&dest[0]),(unsigned char*)(&dest[0])+size);
    strcpy(dest,str.c_str());
    return MC_ERR_NOERROR;
}
       
        
int mc_GenerateConfFiles(const char *network_name)
{
    FILE *fileHan;
    char rpcpwd[64];
    
    fileHan=mc_OpenFile(NULL,"multichain",".conf","r",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR);
    if(fileHan == NULL)
    {
        fileHan=mc_OpenFile(NULL,"multichain",".conf","w",MC_FOM_RELATIVE_TO_DATADIR);
    }
    if(fileHan)
    {
        mc_CloseFile(fileHan);
    }
    
    fileHan=mc_OpenFile(network_name,"multichain",".conf","r",MC_FOM_RELATIVE_TO_DATADIR | MC_FOM_CREATE_DIR);
    if(fileHan == NULL)
    {
        fileHan=mc_OpenFile(network_name,"multichain",".conf","w",MC_FOM_RELATIVE_TO_DATADIR);
        if(fileHan)
        {
            fprintf(fileHan,"rpcuser=multichainrpc\n");
            mc_RandomEncodedBase58String(rpcpwd,32);
            fprintf(fileHan,"rpcpassword=%s\n",rpcpwd);
//            mc_CloseFile(fileHan);
        }       
    }
    if(fileHan)
    {
        mc_CloseFile(fileHan);
    }

    return MC_ERR_NOERROR;
}



int mc_MultichainParams::Build(const unsigned char* pubkey, int pubkey_size) 
{
    if(m_Status == MC_PRM_STATUS_VALID)
    {
        return MC_ERR_NOERROR;
    }
    
    if(m_Status != MC_PRM_STATUS_GENERATED)
    {
        return MC_ERR_INVALID_PARAMETER_VALUE;
    }
    
    mc_RandomSeed(mc_TimeNowAsUInt());
    
    unsigned char hash[32];
    
    int err,size;
    CBlock genesis;
    CMutableTransaction txNew;
    int look_for_genesis=1;
    int intBits=GetInt64Param("powminimumbits");
    uint32_t nBits,timestamp;
    int i;
    const unsigned char *ptr;
//    const unsigned char *pubkey_hash=(unsigned char *)Hash160(pubkey,pubkey+pubkey_size).begin();

    unsigned char pubkey_hash[20];
    uint160 pkhash=Hash160(pubkey,pubkey+pubkey_size);
    memcpy(pubkey_hash,&pkhash,20);

    size_t elem_size;
    const unsigned char *elem;
    int root_stream_name_size;
    const unsigned char *root_stream_name;
    mc_Script *lpDetails;
    mc_Script *lpDetailsScript;
    
    nBits    = 0x7fffff;        
    for(i=0;i<(intBits-1)%8;i++)
    {
        nBits /= 2;
    }
    nBits   += 0x20000000;    
    for(i=0;i<(intBits-1)/8;i++)
    {
        nBits   -= 0x01000000;    
    }      
    

    err=SetParam("genesispubkey",(const char*)pubkey,pubkey_size);
    if(err)
    {
        return err;
    }
    
    uint256 bnProofOfWorkLimit=~uint256(0) >> intBits;
    uint256 bnProofOfWork=~uint256(0);
       
    root_stream_name_size=0;
    root_stream_name=NULL;
    
    root_stream_name=(unsigned char *)GetParam("rootstreamname",&root_stream_name_size);        
    if(IsProtocolMultichain() == 0)
    {
        root_stream_name_size=0;
    }    
    
    while(look_for_genesis)
    {                
        genesis.nBits=nBits;
        timestamp=mc_TimeNowAsUInt();
        
        txNew.vin.resize(1);
        
        if(root_stream_name_size > 1)
        {
            txNew.vout.resize(2);                        
        }
        else
        {
            txNew.vout.resize(1);                                    
        }
        
        ptr=(unsigned char *)GetParam("chaindescription",&size);        
        txNew.vin[0].scriptSig = CScript() << nBits << CScriptNum(4) << vector<unsigned char>(ptr, ptr + size - 1);
                
        txNew.vout[0].nValue = GetInt64Param("initialblockreward");// * COIN;
        
        if(mc_gState->m_NetworkParams->IsProtocolMultichain())
        {
            txNew.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << vector<unsigned char>(pubkey_hash, pubkey_hash + 20) << OP_EQUALVERIFY << OP_CHECKSIG;
        }
        else
        {
            txNew.vout[0].scriptPubKey = CScript() << vector<unsigned char>(pubkey, pubkey + pubkey_size) << OP_CHECKSIG;       
        }
        
        if(IsProtocolMultichain())
        {
            mc_Script *lpScript;
            
            lpScript=new mc_Script;
            
            lpScript->SetPermission(MC_PTP_GLOBAL_ALL,0,0xffffffff,timestamp);
            
            elem = lpScript->GetData(0,&elem_size);
            txNew.vout[0].scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP;
            
            delete lpScript;            
        }

        if(root_stream_name_size > 1)
        {        
            txNew.vout[1].nValue=0;
            lpDetails=new mc_Script;
            lpDetails->AddElement();
            if(GetInt64Param("rootstreamopen"))
            {
                unsigned char b=1;        
                lpDetails->SetSpecialParamValue(MC_ENT_SPRM_ANYONE_CAN_WRITE,&b,1);        
            }
            
            if( (root_stream_name_size > 1) && (root_stream_name[root_stream_name_size - 1] == 0x00) )
            {
                root_stream_name_size--;
            }
            
            lpDetails->SetSpecialParamValue(MC_ENT_SPRM_NAME,root_stream_name,root_stream_name_size);
    
            size_t bytes;
            const unsigned char *script;
            script=lpDetails->GetData(0,&bytes);
    
            lpDetailsScript=new mc_Script;
            
            lpDetailsScript->SetNewEntityType(MC_ENT_TYPE_STREAM,0,script,bytes);

            elem = lpDetailsScript->GetData(0,&elem_size);
            txNew.vout[1].scriptPubKey=CScript();
            txNew.vout[1].scriptPubKey << vector<unsigned char>(elem, elem + elem_size) << OP_DROP << OP_RETURN;                        
            
            delete lpDetails;
            delete lpDetailsScript;
        }        
        
        
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock=uint256(0);
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = timestamp;
        genesis.nNonce=0;
        while(look_for_genesis)
        {
            genesis.nNonce++;
            bnProofOfWork=genesis.GetHash();
            if((genesis.nNonce % 0x1000000) == 0)
            {
                printf("%02x x 2^24 nonces checked\n",genesis.nNonce >> 24);
            }
            if(bnProofOfWork.CompareTo(bnProofOfWorkLimit) < 0)
            {
                look_for_genesis=0;
            }
            if(genesis.nNonce>=0xffffffff)
            {
                look_for_genesis=0;
            }
        }
        if(genesis.nNonce>=0xffffffff)            
        {
            look_for_genesis=1;            
        }
    }
    
    err=SetParam("genesisversion",genesis.nVersion);
    if(err)
    {
        return err;
    }
    err=SetParam("genesistimestamp",genesis.nTime);
    if(err)
    {
        return err;
    }
    err=SetParam("genesisnbits",genesis.nBits);
    if(err)
    {
        return err;
    }
    err=SetParam("genesisnonce",genesis.nNonce);
    if(err)
    {
        return err;
    }
    err=SetParam("genesispubkeyhash",(const char*)pubkey_hash,20);
    if(err)
    {
        return err;
    }    
    
    mc_HexToBin(hash,genesis.GetHash().ToString().c_str(),32);
    err=SetParam("genesishash",(const char*)hash,32);
    if(err)
    {
        return err;
    }
       
   
    CalculateHash(hash);
    
    err=SetParam("chainparamshash",(const char*)hash,32);
    if(err)
    {
        return err;
    }
    
    err=Validate();
    if(err)
    {
        return err;
    }

    err=Write(1);
    if(err)
    {
        return err;
    }
            
    return MC_ERR_NOERROR;
}


