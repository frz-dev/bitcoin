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

// class CProxiedTransaction
// {
// public:
//     const uint256 txid;
//     NodeId lastProxy;
// };


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




/***** PROXY SET *****/
std::vector<CNode*> CConnman::GetInboundPeers(){
    std::vector<CNode*> peers;

    //Get 'fInbound' peers
    LOCK(cs_vNodes);
    for (int i=0; i<vNodes.size(); i++){
        if(vNodes[i]->fInbound == true)
            peers.push_back(vNodes[i]);
    }
 
    return peers;
}

std::vector<CNode*> CConnman::GetOutboundPeers(){
    std::vector<CNode*> peers;

    //Get 'fInbound' peers
    LOCK(cs_vNodes);
    for (int i=0; i<vNodes.size(); i++){
        if(vNodes[i]->fInbound == false)
            peers.push_back(vNodes[i]);
    }
 
    return peers;
}

// Returns a set 'num' of inbound or outbound nodes
// std::vector<CNode*> CConnman::GetRandomNodes(bool fInbound, int num){
//     std::vector<CNode*> randNodes;

//     //Get 'fInbound' peers
//     LOCK(cs_vNodes);
//     for (int i=0; i<vNodes.size(); i++){
//         if(vNodes[i]->fInbound == fInbound)
//             randNodes.push_back(vNodes[i]);
//     }
     
//     //Shuffle elements
//     std::random_shuffle(randNodes.begin(), randNodes.end());
//     //Pick first PROXY_SET_SIZE elements
//     if(randNodes.size()>num)
//         randNodes.resize(num);

//     return randNodes;
// }

// void CConnman::UpdateProxySets(CNode *node){
//     int inProxies = gArgs.GetArg("-inrelays", PROXY_SET_SIZE);
//     int outProxies = gArgs.GetArg("-outrelays", PROXY_SET_SIZE);

//     if(!node->fInbound){
//         // if(vOutProxies.size()<outProxies)
//             // vOutProxies.push_back(node);
//         // else
//         //     GenerateProxySets();
//         // vInProxies = GetInboundPeers();
//     }
//     else{
//         // if(vInProxies.size()<inProxies)
//             vInProxies.push_back(node);
//         // else
//         //     GenerateProxySets();
//     }
// }

// Picks PROXY_SET_SIZE outbound and inbound peers to be used as proxies for the next epoch
// void CConnman::GenerateProxySets(){
//     //get proxy sizes from command line
//     int inProxies = gArgs.GetArg("-inrelays", PROXY_SET_SIZE);
//     int outProxies = gArgs.GetArg("-outrelays", PROXY_SET_SIZE);
//     LogPrint(BCLog::NET, "[FRZ] inProxies: %d - outProxies:%d\n", inProxies, outProxies);

//     LOCK(cs_vProxyPeers);
//     vOutProxies.clear();
//     vOutProxies = GetRandomNodes(false, outProxies);
//     vInProxies.clear();
//     if(inProxies==0)
//         vInProxies = GetInboundPeers();
//     else
//         vInProxies = GetRandomNodes(true, inProxies);

//     LogPrint(BCLog::NET, "[FRZ] Proxy Peers: [");
//     LogPrint(BCLog::NET, "OUT:");
//     for (int i=0; i<vOutProxies.size(); i++)
//         LogPrint(BCLog::NET, "%s - ", vOutProxies[i]->addr.ToString());
//     // if(vInProxies.size()>0){
//     //     LogPrint(BCLog::NET, "IN:");
//     //     for (int i=0; i<vInProxies.size(); i++)
//     //         LogPrint(BCLog::NET, "%s - ", vInProxies[i]->addr.ToString());
//     // }
//     LogPrint(BCLog::NET, "]\n");
// }

/***** PROXIED TX *****/
CTransactionRef FindProxiedTx(const uint256 txid){
    for(auto ptx : vProxiedTransactions){
        if(ptx->GetHash() == txid)
            return ptx;
    }
    return nullptr;
}

void removeProxiedTransaction(CTransactionRef ptx){
    // CTransactionRef tx = FindProxiedTx(ptx->GetHash());

    // if(tx)
    //     vProxiedTransactions.erase(tx);
    
    for (auto it = vProxiedTransactions.begin(); it != vProxiedTransactions.end();)
    {
        if ((*it)->GetHash() == ptx->GetHash()) {
            it = vProxiedTransactions.erase(it);
            it--;
        }
        // else {
        //    ++it;
        // }
    }

    // std::vector<CTransactionRef>::iterator toErease;
    // toErease=std::find(vProxiedTransactions.begin(), vProxiedTransactions.end(), 
    // [&]( CTransaction *tx ) { return tx->GetHash() == ptx->GetHash(); });

    // // And then erase if found
    // if (toErease!=vProxiedTransactions.end()){
    //     vProxiedTransactions.erase(toErease);
    // }
}

void SetTxBroadcasted(CTransactionRef ptx){
    //Mark transaction as broadcasted
    ptx->broadcasted = true;

}

void BroadcastProxyTx(CTransactionRef ptx, CConnman& connman){
    LogPrint(BCLog::NET, "[FRZ] Broadcasting proxytx %s\n", ptx->GetHash().ToString());
    RelayTransaction(ptx->GetHash(), connman);
    CTransactionRef proxiedTx=FindProxiedTx(ptx->GetHash());
    if(proxiedTx)
        proxiedTx->broadcasted = true;
}

/*** PROXY ***/

/* Send PROXYTX message */
void sendProxyTx(CNode *pproxy, const CTransactionRef& tx, CConnman& connman){
    const CNetMsgMaker msgMaker(pproxy->GetSendVersion());
    connman.PushMessage(pproxy, msgMaker.Make(NetMsgType::PROXYTX, *tx));
    // tx->proxied = true;
    vProxiedTransactions.push_back(tx);
}

/* Pick random proxy and push transaction */
void ProxyTx(const CTransactionRef& tx, CNode *pfrom, CConnman& connman){
    LogPrint(BCLog::NET, "[FRZ] Relaying proxy transaction %s\n", tx->GetHash().ToString());
    //Pick random proxy P
    bool fInbound;

    CTransactionRef proxiedTx = FindProxiedTx(tx->GetHash());

    LOCK(cs_vProxyPeers);
    //If we are proxying a new tx, pick an outbound node
    if(pfrom == nullptr) 
        fInbound = false;
    else 
        fInbound = pfrom->fInbound;
    //Pick a random proxy from the set
    CNode * proxyNode = nullptr;
    std::vector<CNode*> proxySet;
    if(fInbound)
        proxySet = connman.GetInboundPeers();
    else
        proxySet = connman.GetOutboundPeers();

    if(proxySet.size()>0){
        //proxies to exclude
        NodeId pfromId, lastProxyId;
        pfromId = lastProxyId = -1;
        if(pfrom) pfromId = pfrom->GetId();
        if(proxiedTx) lastProxyId = proxiedTx->lastProxyRelay;

        //select random proxy
        int i = (rand() % (proxySet.size()));
        proxyNode = proxySet.at(i);
        int tried = 1;

        //do not proxy to pfrom
        if(proxyNode->GetId()==pfromId || proxyNode->GetId()==lastProxyId){
            NodeId prevId = proxyNode->GetId();
            proxyNode = proxySet.at( (i+1)%(proxySet.size()) );
            LogPrint(BCLog::NET, "[FRZ] proxyNode = pfrom or lastProxy (%d). changing to %d\n", prevId, proxyNode->GetId());

            //do not proxy to previous proxy for the same tx
            if(proxyNode->GetId()==pfromId || proxyNode->GetId()==lastProxyId){
                NodeId prevId = proxyNode->GetId();
                proxyNode = proxySet.at( (i+1)%(proxySet.size()) );
                LogPrint(BCLog::NET, "[FRZ] proxyNode = pfrom or lastProxy (%d). changing to %d\n", prevId, proxyNode->GetId());
            }
        }
    }
    else{
        LogPrint(BCLog::NET, "[FRZ] WARNING: proxy set is empty\n");
    }

    //Push transaction
    if(proxyNode){
        LogPrint(BCLog::NET, "[FRZ] Sending proxytx %s to %s proxy peer=%d)\n", tx->GetHash().ToString(), proxyNode->fInbound?"inbound":"outbound", proxyNode->GetId());
        sendProxyTx(proxyNode, tx, connman);

        if(proxiedTx){
            proxiedTx->lastProxyRelay = proxyNode->GetId();
        }
    }
    else{
        LogPrint(BCLog::NET, "[FRZ] ERROR: no proxy node available. Broadcasting\n");
        BroadcastProxyTx(tx, connman);
    }
}

void PeerLogicValidation::ReattemptProxy(CScheduler& scheduler){
    LogPrint(BCLog::NET, "[FRZ] ReattemptProxy\n");
    // std::set<uint256> unbroadcast_txids = m_mempool.GetUnbroadcastTxs();

    // LOCK(cs_vProxiedTransactions);
    for (auto tx : vProxiedTransactions) {
        // Sanity check: all unbroadcast txns should exist in the mempool
        if (!tx->broadcasted) {
            LogPrint(BCLog::NET, "[FRZ] ReProxying tx %s\n", tx->GetHash().ToString());
            ProxyTx(tx, nullptr, *connman); 
        } else {
            ;
            // removeProxiedTransaction(tx);
        }
    }

    // Schedule next run for 10-15 minutes in the future.
    // We add randomness on every cycle to avoid the possibility of P2P fingerprinting.
    const std::chrono::milliseconds delta = std::chrono::seconds{30};
    scheduler.scheduleFromNow([&] { ReattemptProxy(scheduler); }, delta);

}