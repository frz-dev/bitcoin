/*REBREL*/
//#include <rebrel.h>
#include <sync.h>
#include <net.h>

#define PROXY_SET_SIZE 5

float proxyTx = 0.5;

std::vector<CNode*> vProxyPeers GUARDED_BY(cs_vProxyPeers);
RecursiveMutex cs_vProxyPeers;


void ProxyTx(const uint256& txid){ //, const CConnman& connman
    //Pick random proxy P
    int i = (rand() % vProxyPeers.size()) - 1;
    CNode *proxy = vProxyPeers.at(i);

    //Relay tx to P
    //connman.ForEachNode([&txid](CNode* pnode){
        proxy->PushTxInventory(txid);
    //});

    //Set timeout

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

