/*POC*/
#include <poc.h>
#include <net.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <time.h>

class CNode;
class CNetMsgMaker;
class CAddress;

/**/
std::string getOurAddr(CNode *p){
    CAddress addrBind = p->addrBind;
    int ourport = GetListenPort();

    std::string ouraddr = addrBind.ToStringIP() + ":" + std::to_string(ourport);

    return ouraddr;
}

std::string getOurAddrIP(CNode *p){
    CAddress addrBind = p->addrBind;
    
    std::string ourIP = addrBind.ToStringIP();

    return ourIP;
}

void CNetMon::sendMon(CNode *pnode){
    if(g_netmon){
        LogPrint(BCLog::NET, "[POC] Sending MON\n");
        const CNetMsgMaker msgMaker(pnode->GetSendVersion());
        std::string ouraddr = getOurAddr(pnode);
        g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::MON, ouraddr));
    }
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


// /* sendPoC */
// void CNetMon::sendPoC(CNode *pto, CPoC *poc){
//     const CNetMsgMaker msgMaker(pto->GetSendVersion());

//     CNode* pfrom = g_netmon->getNetNode(poc->target_addr)->getCNode();
//     if(!pfrom){ LogPrint(BCLog::NET, "[POC] WARNING: pfrom not found\n"); return;} //TODO: return false

//     //Set timeout
//     int64_t nNow = GetTimeMicros();
//     int64_t ping = pto->nPingUsecTime+pfrom->nPingUsecTime;
// LogPrint(BCLog::NET, "[POC] [DBG]: ping (microsecs):%d\n", (int)(ping));

//     if(ping == 0){
// LogPrint(BCLog::NET, "[POC] [DBG]: ping is 0, setting to MAX\n");
//         poc->timeout = nNow+MAX_VERIFICATION_TIMEOUT;
//         }
//     else
//         poc->timeout = nNow+(ping*6);

// LogPrint(BCLog::NET, "[POC] [DBG]: timeout (microsecs):%d (MAX:%d)\n", (int)((poc->timeout-nNow)),(int)(MAX_VERIFICATION_TIMEOUT));

//     //Send POC
//     LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pto->addr.ToString(),poc->id,poc->target,poc->monitor);
//     g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::POC, *poc));    
// }


/* createPoC */
CPoC * createPoC(CNetNode *ptarget){
    CNode *pnode = ptarget->getCNode();
    int pocId = rand() % 100000; //TODO get better random number

    //Create poc
    std::string ouraddr = getOurAddr(pnode);
    std::string targetaddr = pnode->addr.ToString();
    CPoC *poc = new CPoC(pocId, ouraddr, targetaddr);

    //Set timeout
    //TODO-POC: set timeout as max of last poc round
    int64_t nNow = GetTimeMicros();
//     int64_t ping = ptarget->nPingUsecTime;
// LogPrint(BCLog::NET, "[POC] [DBG]: ping (microsecs):%d\n", (int)(ping));

    poc->timeout = nNow+MAX_VERIFICATION_TIMEOUT;
    if(poc->timeout > ptarget->nextPoCRound){
        LogPrint(BCLog::NET, "[POC] WARNING: pnode is NULL\n");
        poc->timeout = ptarget->nextPoCRound-1000;
    }

    ptarget->poc = poc;

    return poc;
}

// void CNetMon::sendPoC(CNode *pto){
//     const CNetMsgMaker msgMaker(pto->GetSendVersion());

//     //Send PoC message
//     LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pto->addr.ToString(),poc->id,poc->target,poc->monitor);
//     g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::POC, pto->netNode->poc));    
// }

/* sendVerified */
void sendVerified(CNode *pto){
    return;
}

/* sendPoC */
void CNetMon::startPoCRound(CNode *pnode){
    if(!pnode){ LogPrint(BCLog::NET, "[POC] WARNING: pnode is NULL\n"); return;}

    CPoC *poc = pnode->netNode->poc;
    //Create or update PoC    
    if(poc)
        poc->update();
    else
        poc = createPoC(pnode->netNode);

    //Send POC message
//    sendPoC(pnode);
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());

    LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pnode->addr.ToString(),poc->id,poc->target,poc->monitor);
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::POC, *poc));
}

void CNetMon::endPocRound(CNode *pnode){
    LogPrint(BCLog::NET, "[POC] endPocRound(%s)\n", pnode->GetAddrName());
    CNetNode *netnode = pnode->netNode;

    //Delete unverified peers
    // for (CPeer& peer : netnode->vPeers){
    //     if(peer.poc->id != netnode->poc->id){
    //         LogPrint(BCLog::NET, "[POC] removing peer %s\n", peer.addr);
    //         //netnode->removePeer(peer.addr); //TODO: g_netmon->removeConnection
    //     }
    // }
LogPrint(BCLog::NET, "[POC] endPocRound(%s) - CHK1\n");
    //Send VERIFIED
    sendVerified(pnode);
LogPrint(BCLog::NET, "[POC] endPocRound(%s) - CHK2\n");
    //update Freq
    netnode->updateFreq();
LogPrint(BCLog::NET, "[POC] endPocRound(%s) - CHK3\n");
    //Reset timeout
    //node->poc->timeout = 0;
    netnode->poc->fExpired = true;
}

/* sendAlert */
// void CNetMon::sendAlert(CPeer *peer, std::string type){
//     if(!peer){ LogPrint(BCLog::NET, "[POC] ERROR !peer\n"); return;}

//     CPoCAlert alert(type, peer->addr, peer->addrBind);

//     LogPrint(BCLog::NET, "[POC] Sending \"ALERT\": type=%s, a1=%s, a2=%s\n", alert.type, alert.addr1, alert.addr2);

//     //Send to node A
//     //if(!peer->node) LogPrint(BCLog::NET, "[POC] ERROR !peer->node\n");
//     CNode *pA;
//     if(peer->node)
//         pA = peer->node->getCNode();
//     else 
//         pA = g_connman->FindNode(peer->addr);
//     if(pA){
//         LogPrint(BCLog::NET, "[POC] Sending \"ALERT\" to %s\n", pA->addr.ToString());
//         g_connman->PushMessage(pA, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));    
//     }

//     //Send to node B (if connected)
//     CNetNode *pBn = g_netmon->findNodeByPeer(peer->addrBind,peer->addr); //getNetNode(peer->addr);
//     if(pBn){
//         CNode *pB = pBn->getCNode();
//         if(pB){
//             LogPrint(BCLog::NET, "[POC] Sending \"ALERT\" to %s\n", pB->addr.ToString());
//             g_connman->PushMessage(pB, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));
//         }
//     }

//     if(type != "unverified")
//         g_netmon->removePeer(*peer);
// LogPrint(BCLog::NET, "[POC] [DBG]: Remove OK\n");

//     //TODO: decrease reputation of nodes that keep claiming unverified connections
// }

/* connectNode */
CNode * CNetMon::connectNode(std::string addr){
    LogPrint(BCLog::NET, "[POC] Connecting to: %s\n", addr.c_str());

    CAddress paddr(CService(), NODE_NONE);
    g_connman->OpenNetworkConnection(paddr, false, nullptr, addr.c_str(), false, false, true);
    return g_connman->FindNode(addr);
}

CNode* CNetMon::connectNode(CPeer *peer){
    if(peer->fInbound) return NULL;

    /* Connect to peer */
    CNode *ppeer = connectNode(peer->addr);

    // //If we can't connect, send ALERT
    // if(!ppeer){
    //     LogPrint(BCLog::NET, "[POC] ERROR: could not connect\n");
        
    //     std::string type("connect");
    //     sendAlert(peer,type);

    //     return NULL;
    // }

    return ppeer;
}
