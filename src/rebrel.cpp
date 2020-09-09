#include <net.h>
#include <sync.h>

#define PROXY_SET_SIZE 5
std::vector<CNode*> vProxyPeers GUARDED_BY(cs_vProxyPeers);
RecursiveMutex cs_vProxyPeers;

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

// bool CConnman::IsThisReachable(const CAddress &addr){
//     // fListen ?
//     if(!fListen)
//         return false;

//     // If we have inbound peers, then we are reachable
//     //TODO: check inbound peers

//     // Try to connect to ourself

//     return TestReachable(addr);
// }

bool CConnman::IsPeerReachable(const CNode *pnode){

    /*Is outbound ?*/
    if(!pnode->fInbound)
        return true;

    /*IsRoutable ?*/
    if(!TestReachable(pnode->addr))
        return false;


    
}

void CConnman::GenerateProxySet(void){
    //Pick random nodes from peer list
    //Use percentage of current peers (MIN n nodes)
    vProxyPeers.clear();

    LOCK(cs_vNodes);
    LOCK(cs_vProxyPeers);
    //Copy vNodes into vProxyPeers
    for (int i=0; i<vNodes.size(); i++) 
        vProxyPeers.push_back(vNodes[i]);
    //Shuffle elements
    std::random_shuffle(vProxyPeers.begin(), vProxyPeers.end());
    //Pick first PROXY_SET_SIZE elements
    if(vProxyPeers.size()>PROXY_SET_SIZE)
        vProxyPeers.resize(PROXY_SET_SIZE);

    if(vProxyPeers.size()>0){
        LogPrint(BCLog::NET, "[FRZ] vProxyPeers: ");
        for (int i=0; i<vProxyPeers.size(); i++)
            LogPrint(BCLog::NET, "- %s - ", vProxyPeers[i]->addr.ToString());
        LogPrint(BCLog::NET, "\n");
    }
}

bool CConnman::ProxyTx(const CTransaction *tx){
    //Pick random proxy P
    int i = (rand() % vProxyPeers.size()) - 1;
    CNode *proxy = vProxyPeers.at(i);

    //Relay tx to P
    //Set timeout

}
