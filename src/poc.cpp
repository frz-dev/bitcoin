/*POC*/
#include <poc.h>
#include <net.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <time.h>

class CNode;
class CNetMsgMaker;
class CAddress;

/* Return the IP part of an address */
std::string getIPAddr(std::string addr){
    std::string::size_type cut = addr.find(':');
    if (cut != std::string::npos)
        return addr.substr(0, cut);
    else
        return addr;
}

/**/
std::string getOurAddr(CNode *p){
    CAddress addrBind = p->addrBind;
    int ourport = GetListenPort();

    std::string ouraddr = addrBind.ToStringIP() + ":" + std::to_string(ourport);

    return ouraddr;
}

void sendMon(CNode *pnode){
    LogPrint(BCLog::NET, "[POC] Sending MON\n");
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());
    std::string ouraddr = getOurAddr(pnode);
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::MON, ouraddr));
}

/*POCMAL*/
/* Send a MAL message to communicate this is a malicious node */
void sendMal(CNode *pnode){
    LogPrint(BCLog::NET, "[POC] Sending MAL\n");
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());

    std::vector<std::string> malPeers;
    malPeers.clear();
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::MAL, malPeers));
}

// void sendRegister(CNode *pnode, std::string monaddr){
//     LogPrint(BCLog::NET, "[POC] Sending REGISTER\n");

//     //Send REGISTER
//     std::string ouraddr = getOurAddr(pnode);
//     const CNetMsgMaker msgMaker(pnode->GetSendVersion());
//     g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::REGISTER, ouraddr));
// }

void initPoCConn(CNode *pnode){
    if(!g_connman->NodeFullyConnected(pnode) || !pnode->fSuccessfullyConnected){
        LogPrint(BCLog::NET, "[POC] WARNING: Not fully connected. Postponing initialization (%s)\n");
        return;
    }
    
    pnode->fPoCInit = true;

    /*Monitor*/
    if(g_netmon)
        return sendMon(pnode);
    
    /*POCMAL*/
    /*Malicious*/
    if(gArgs.IsArgSet("-malicious"))
        return sendMal(pnode);

    /*Regular*/
    //Add CVerified vector for pnode
    CVerified ver(pnode->GetAddrName(), pnode->fInbound);
    //Init reputation for all monitors
    for (auto monitor : g_monitors){
        LogPrint(BCLog::NET, "[POC] Init reputation for %s\n", monitor->GetAddrName());
        ver.initMonitor(monitor->monAddr);
    }

    LogPrint(BCLog::NET, "[POC] Adding %s (bind:%s) to monitoring list\n", pnode->GetAddrName(),pnode->addrBind.ToString());
    return g_verified.push_back(ver);
}

/* setMaxTimeout */
void CNetMon::setMaxTimeout(){
    int64_t max_ping=0;
    std::vector<CNodeStats> vstats;
    g_connman->GetNodeStats(vstats);
        
    for (const CNodeStats& stats : vstats){
        CNode *node = g_connman->FindNode(stats.addr);
        if(node && (node->nPingUsecTime > max_ping))
            max_ping = node->nPingUsecTime;
    }

    LogPrint(BCLog::NET, "[POC] MAX PING = %d\n", (int)max_ping);
}


/* createPoC */
CPoC * createPoC(CNetNode *ptarget){
    LogPrint(BCLog::NET, "[POC] createPoC(%s)\n", ptarget->addr);
    CNode *pnode = ptarget->getCNode();
    int pocId = rand() % 1000000; //TODO get better random number

    //Create poc
    std::string ouraddr = getOurAddr(pnode);
    std::string targetaddr = pnode->addr.ToStringIP();
    CPoC *poc = new CPoC(pocId, ouraddr, targetaddr);

    //Set timeout as the maxtimeout seen in last poc round
    int64_t nNow = GetTimeMicros();
    poc->timeout = nNow+MAX_VERIFICATION_TIMEOUT;

    //Replace PoC
    ptarget->poc = poc;

    return poc;
}


/* sendVerified */
void sendVerified(CNode *pto){
    //Retrieve peer list
    std::vector<CVerified> vVerified;
    
    /* Add verified peers to list */
    for (const CPeer& peer : pto->netNode->vPeers){
        vVerified.push_back(CVerified(peer.addr, peer.fInbound, peer.poc->id));
    } 

// LogPrint(BCLog::NET, "[POC] \"VERIFIED\":");
// for (const CPeer& peer : vVerified){
//     LogPrint(BCLog::NET, "(%s,%s) ", peer.addr, peer.fInbound?"inbound":"outbound");
// }
// LogPrint(BCLog::NET, "\n");

    /*POCMALMON*/
    /* Malicious node */
    bool malicious = false;
    if(gArgs.IsArgSet("-malicious")) malicious = true;
    //If we are a malicious monitor, let's not confirm any peer
    if(malicious){
        LogPrint(BCLog::NET, "[POC] MALICIOUS: sending empty VERIFIED\n");
        vVerified.clear();
    }

    //Send VERIFIED message
    LogPrint(BCLog::NET, "[POC] Sending VERIFIED to %s\n", pto->GetAddrName());
    const CNetMsgMaker msgMaker(pto->GetSendVersion());
    g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::VERIFIED, vVerified));

    return;
}

/* sendPoC */
void CNetMon::startPoCRound(CNode *pnode){
    LogPrint(BCLog::NET, "[POC] startPoCRound(%s)\n", pnode->GetAddrName());

    if(!pnode){ LogPrint(BCLog::NET, "[POC] WARNING: pnode is NULL\n"); return;}
    CNetNode *pnetnode = pnode->netNode;
    if(!pnetnode){ LogPrint(BCLog::NET, "[POC] WARNING: netNode is NULL\n"); return;}

    CPoC *poc = pnetnode->poc;
    //Create or update PoC    
    if(poc)
        poc->update();
    else
        poc=createPoC(pnetnode);

    //Send POC message
//    sendPoC(pnode);
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());

    LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pnode->GetAddrName(),poc->id,poc->target,poc->monitor);
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::POC, *poc));

    //Let's wait for next PoC round starting from when this round ends
    //GetTimeMicros()
    pnode->nNextPoCRound = PoissonNextSend(poc->timeout, pnetnode->pocUpdateInterval); //pnetnode->pocUpdateInterval
    // if(poc->timeout > pnode->nNextPoCRound)
    //     poc->timeout = pnode->nNextPoCRound - 10000;

}

void CNetMon::endPocRound(CNode *pnode){
    LogPrint(BCLog::NET, "[POC] endPocRound(%s)\n", pnode->GetAddrName());
    CNetNode *netnode = pnode->netNode;

    //Delete unverified peers
    for (CPeer& peer : netnode->vPeers){
        //if outbound
        if(!peer.fInbound)
            //If last verified PoC is older than this round, consider peer as unverified
            if(peer.poc->id != netnode->poc->id){
                LogPrint(BCLog::NET, "[POC] removing unverified outbound peer %s of %s\n", peer.addr, pnode->GetAddrName());
                g_netmon->removeConnection(peer);
            }
        //if inbound
        else{
            if(peer.poc->fExpired){
                LogPrint(BCLog::NET, "[POC] removing unverified inbound peer %s of %s\n", peer.addr, pnode->GetAddrName());
                g_netmon->removeConnection(peer);
            }
        }

    }

    //Send VERIFIED
    sendVerified(pnode);

    //update Freq
    //TODO: check maxtimeout and adjust avgPocWait accordingly
    netnode->updateFreq();
    //Set PoC expired
    netnode->poc->fExpired = true;
}

/* connectNode */
CNode * CNetMon::connectNode(std::string addr){
    LogPrint(BCLog::NET, "[POC] Connecting to: %s\n", addr.c_str());

    CAddress paddr(CService(), NODE_NONE);
    g_connman->OpenNetworkConnection(paddr, false, nullptr, addr.c_str(), false, false, true);
    CNode *pnode = g_connman->FindNode(addr);
   
    if(pnode) {
        //Create NetNode
        pnode->netNode = addNetNode(pnode->addr.ToString(), pnode);
        if(!pnode->netNode) LogPrint(BCLog::NET, "[POC] WARNING- NetNode not added! %s\n", addr);
    }
    else
    {
        LogPrint(BCLog::NET, "[POC] WARNING- NetNode not added! %s\n", addr);
    }
    

    return pnode;

}

bool removeVerified(std::string addr){
    std::vector<CVerified>::iterator it = std::find_if(g_verified.begin(), g_verified.end(), [&](CVerified n) {
        return n.addr==addr;
    });
    
    if ( it != g_verified.end() ){
        LogPrint(BCLog::NET, "[POC] Removing CVerified: %s\n", addr);
        //Delete object and shrink vector
        g_verified.erase(it);
        g_verified.shrink_to_fit();

        return true;
    }

    return false;

}
