/*REBREL*/
//#include <rebrel.h>
#include <sync.h>
#include <net.h>
#include <primitives/transaction.h>
#include <netmessagemaker.h>
#include <txmempool.h>
#include <scheduler.h>
#include <net_processing.h>

#define PROXY_SET_SIZE 2
const std::chrono::seconds PROXIED_BROADCAST_TIMEOUT {1 * 60};
std::vector<CTransactionRef> vProxiedTransactions GUARDED_BY(cs_vProxiedTransactions);
RecursiveMutex cs_vProxiedTransactions;

std::vector<CNode*> vOutProxies GUARDED_BY(cs_vProxyPeers);
std::vector<CNode*> vInProxies GUARDED_BY(cs_vProxyPeers);
RecursiveMutex cs_vProxyPeers;

/*** REACHABILITY ***/
bool CConnman::TestReachable(const CAddress &addr){
    // if(!IsReachable(addr) || !addr.IsRoutable());
    //     return false;
    LogPrint(BCLog::NET, "[FRZ] Testing addr reachability (%s)\n", addr.ToString());

    /*ConnectNode*/
    CNode* pnode = ConnectNode(addr, addr.ToString().c_str(), false, false, false);
    if (!pnode){
        LogPrint(BCLog::NET, "[FRZ] addr is not reachable (%s)\n", addr.ToString());
        return false;
    }
        
    pnode->fFeeler = true;
    m_msgproc->InitializeNode(pnode);
    {
        LOCK(cs_vNodes);
        vNodes.push_back(pnode);
    }

    LogPrint(BCLog::NET, "[FRZ] addr is reachable (%s)\n", addr.ToString());
    return true;
}

bool CConnman::IsPeerReachable(const CNode *pnode){

    /*Is outbound ?*/
    if(!pnode->fInbound)
        return true;

    /*IsRoutable ?*/
    if(!TestReachable(pnode->addr))
        return false;    
}

/*** PROXY ***/

/* Send PROXYTX message */
void sendProxyTx(CNode *pproxy, const CTransactionRef& tx, CConnman& connman){
    const CNetMsgMaker msgMaker(pproxy->GetSendVersion());
    connman.PushMessage(pproxy, msgMaker.Make(NetMsgType::PROXYTX, *tx));
}

/* Pick random proxy and push transaction */
void ProxyTx(const CTransactionRef& tx, CNode *pfrom, CConnman& connman){
    //Pick random proxy P
    bool fInbound;

    LOCK(cs_vProxyPeers);
    //If we are proxying a new tx, pick an outbound node
    if(pfrom == nullptr) 
        fInbound = false;
    else 
        fInbound = pfrom->fInbound;
    //Pick a random proxy from the set
    CNode * proxyNode = nullptr;
    if(fInbound){
        if(vInProxies.size()>0){
            int i = (rand() % (vInProxies.size()));
            proxyNode = vInProxies.at(i);
            //do not proxy to pfrom
            if(proxyNode==pfrom) proxyNode = vInProxies.at( (i+1)%vInProxies.size() );
        }
        else{
            LogPrint(BCLog::NET, "[FRZ] ERROR: no in proxies\n");
        }
    }
    else{
        if(vOutProxies.size()>0){
            int i = (rand() % vOutProxies.size());
            proxyNode = vOutProxies.at(i);
            //do not proxy to pfrom
            if(proxyNode==pfrom) proxyNode = vOutProxies.at((i+1)%vOutProxies.size());
        }
        else{
            LogPrint(BCLog::NET, "[FRZ] ERROR: no in proxies\n");
        }
    }

    //Push transaction
    if(proxyNode){
        sendProxyTx(proxyNode, tx, connman);

        tx->proxied = true;
        // auto current_time = GetTime<std::chrono::seconds>();
        // tx->m_next_broadcast_test = current_time + PROXIED_BROADCAST_TIMEOUT;

        //add to proxied set
        LOCK(cs_vProxiedTransactions);
        vProxiedTransactions.push_back(tx);
    }
    else{
        LogPrint(BCLog::NET, "[FRZ] ERROR: no proxy node available\n");
    }
}

void PeerLogicValidation::ReattemptProxy(CScheduler& scheduler){
    std::set<uint256> unbroadcast_txids = m_mempool.GetUnbroadcastTxs();

    for (const uint256& txid : unbroadcast_txids) {
        // Sanity check: all unbroadcast txns should exist in the mempool
        if (m_mempool.exists(txid)) {
            CTransactionRef ptx = m_mempool.get(txid);
            if(ptx->proxied && !ptx->broadcasted)
                ProxyTx(ptx, nullptr, *connman); 
                //TODO-REBREL: save fInbound and which proxies have been used -- we can add a vector to CNode to keep track of (unbroadcast) proxied transactions sent to it
        } else {
            m_mempool.RemoveUnbroadcastTx(txid, true);
        }
    }

    // Schedule next run for 10-15 minutes in the future.
    // We add randomness on every cycle to avoid the possibility of P2P fingerprinting.
    const std::chrono::milliseconds delta = std::chrono::minutes{5};
    scheduler.scheduleFromNow([&] { ReattemptProxy(scheduler); }, delta);

}


/***** PROXY SET *****/
std::vector<CNode*> CConnman::GetInboundPeers(){
    std::vector<CNode*> inPeers;

    //Get 'fInbound' peers
    LOCK(cs_vNodes);
    for (int i=0; i<vNodes.size(); i++){
        if(vNodes[i]->fInbound == true)
            inPeers.push_back(vNodes[i]);
    }
 
    return inPeers;
}

// Returns a set 'num' of inbound or outbound nodes
std::vector<CNode*> CConnman::GetRandomNodes(bool fInbound, int num){
    std::vector<CNode*> randNodes;

    //Get 'fInbound' peers
    LOCK(cs_vNodes);
    for (int i=0; i<vNodes.size(); i++){
        if(vNodes[i]->fInbound == fInbound)
            randNodes.push_back(vNodes[i]);
    }
     
    //Shuffle elements
    std::random_shuffle(randNodes.begin(), randNodes.end());
    //Pick first PROXY_SET_SIZE elements
    if(randNodes.size()>num)
        randNodes.resize(num);

    return randNodes;
}

// Picks PROXY_SET_SIZE outbound and inbound peers to be used as proxies for the next epoch
void CConnman::GenerateProxySets(){
    // int inProxies = PROXY_SET_SIZE;
    // int outProxies = PROXY_SET_SIZE;
    // if(gArgs.IsArgSet("-inrelays"))
    int inProxies = gArgs.GetArg("-inrelays", PROXY_SET_SIZE);
    int outProxies = gArgs.GetArg("-outrelays", PROXY_SET_SIZE);
    LogPrint(BCLog::NET, "[FRZ] inProxies: %d - outProxies:%d\n", inProxies, outProxies);

    LOCK(cs_vProxyPeers);
    vOutProxies.clear();
    vOutProxies = GetRandomNodes(false, outProxies);
    vInProxies.clear();
    vInProxies = GetRandomNodes(true, inProxies);

    LogPrint(BCLog::NET, "[FRZ] Proxy Peers: [");
    LogPrint(BCLog::NET, "OUT:");
    for (int i=0; i<vOutProxies.size(); i++)
        LogPrint(BCLog::NET, "%s - ", vOutProxies[i]->addr.ToString());
    // if(vInProxies.size()>0){
    //     LogPrint(BCLog::NET, "IN:");
    //     for (int i=0; i<vInProxies.size(); i++)
    //         LogPrint(BCLog::NET, "%s - ", vInProxies[i]->addr.ToString());
    // }
    LogPrint(BCLog::NET, "]\n");
}

/***** PROXIED TX *****/
CTransactionRef FindProxiedTx(const uint256 txid){
    for(auto ptx : vProxiedTransactions){
        if(ptx->GetHash() == txid)
            return ptx;
    }
    return nullptr;
}


void SetTxBroadcasted(CTransactionRef ptx){
    /*Delete tx from proxied transactions*/
    std::vector<CTransactionRef>::iterator toErease;
    toErease=std::find(vProxiedTransactions.begin(), vProxiedTransactions.end(), ptx);

    // And then erase if found
    if (toErease!=vProxiedTransactions.end()){
        vProxiedTransactions.erase(toErease);
    }

    //Mark transaction as broadcasted
    ptx->broadcasted = true;
}

void BroadcastProxyTx(CTransactionRef ptx, CConnman& connman){
    RelayTransaction(ptx->GetHash(), connman);
    SetTxBroadcasted(ptx);
}

