/*REBREL*/
//#include <rebrel.h>
#include <sync.h>
#include <net.h>
#include <primitives/transaction.h>
#include <netmessagemaker.h>

#define PROXY_SET_SIZE 5
const std::chrono::seconds PROXIED_BROADCAST_TIMEOUT {1 * 60};
std::vector<CTransactionRef> vProxiedTransactions GUARDED_BY(cs_vProxiedTransactions);
RecursiveMutex cs_vProxiedTransactions;

float proxyTx = 0.5;

std::vector<CNode*> vProxyPeers GUARDED_BY(cs_vProxyPeers);
RecursiveMutex cs_vProxyPeers;

void ProxyTx(const CTransactionRef& tx, CConnman& connman){
    //Pick random proxy P
    LOCK(cs_vProxyPeers);
    int i = (rand() % vProxyPeers.size()) - 1;
    CNode *proxy = vProxyPeers.at(i);

    /*INV version*/
    //Relay tx to P
    //proxy->PushTxInventory(txid);
    //});
    /**/

    const CNetMsgMaker msgMaker(proxy->GetSendVersion());
    connman.PushMessage(proxy, msgMaker.Make(NetMsgType::PROXYTX, *tx));

    tx->proxied = true;
    auto current_time = GetTime<std::chrono::seconds>();
    tx->m_next_broadcast_test = current_time + PROXIED_BROADCAST_TIMEOUT;
    //add to proxied set
    LOCK(cs_vProxiedTransactions);
    vProxiedTransactions.push_back(tx);

    //TODO: Set timeout or just calculate from nTimeBroadcasted

}

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

// Returns a set 'num' of reachable or unreachable nodes
std::vector<CNode*> CConnman::GetRandomNodes(bool reachable, int num){
    std::vector<CNode*> randNodes;

    LOCK(cs_vNodes);
    //Copy vNodes into vProxyPeers
    bool fReachable = IsThisReachable();

    //If we are reachable, get unreachable peers
    //otherwise, get reachable peers
    for (int i=0; i<vNodes.size(); i++){
        if(vNodes[i]->fReachable == reachable)
            randNodes.push_back(vNodes[i]);
    }
     
    //Shuffle elements
    std::random_shuffle(randNodes.begin(), randNodes.end());
    //Pick first PROXY_SET_SIZE elements
    if(randNodes.size()>num)
        randNodes.resize(num);

    return randNodes;
}

void CConnman::GenerateProxySet(void){
    LOCK(cs_vProxyPeers);
    vProxyPeers.clear();

    //Pick random nodes from peer list
    //Use percentage of current peers (MIN n nodes)
    vProxyPeers = GetRandomNodes(IsThisReachable(), PROXY_SET_SIZE);

    LogPrint(BCLog::NET, "[FRZ] vProxyPeers: [");
    for (int i=0; i<vProxyPeers.size(); i++)
        LogPrint(BCLog::NET, "- %s - ", vProxyPeers[i]->addr.ToString());
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

}

