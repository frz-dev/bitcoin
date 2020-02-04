#include <poc.h>
#include <net.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <time.h>

class CNode;
class CNetMsgMaker;
class CAddress;

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

/* sendPoC */
void CNetMon::sendPoC(CNode *pto, CPoC *poc){
    const CNetMsgMaker msgMaker(pto->GetSendVersion());
    //TODO? create poc here?

    CNode* pfrom = g_netmon->getNode(poc->target_addr)->getCNode();
    if(!pfrom){ LogPrint(BCLog::NET, "[POC] WARNING: pfrom not found\n"); return;} //TODO: return false

    //Set timeout
    int64_t nNow = GetTimeMicros();
    int64_t ping = pto->nPingUsecTime+pfrom->nPingUsecTime;
LogPrint(BCLog::NET, "[POC] DEBUG: ping (microsecs):%d\n", (int)(ping));

    if(ping == 0){
LogPrint(BCLog::NET, "[POC] DEBUG: ping is 0, setting to MAX\n");
        poc->timeout = nNow+MAX_VERIFICATION_TIMEOUT;
        }
    else
        poc->timeout = nNow+(ping*6);

LogPrint(BCLog::NET, "[POC] DEBUG: timeout (microsecs):%d (MAX:%d)\n", (int)((poc->timeout-nNow)),(int)(MAX_VERIFICATION_TIMEOUT));

    //Send POC
    LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pto->addr.ToString(),poc->id,poc->target,poc->monitor);
    g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::POCCHALLENGE, *poc));    
}


/* sendAlert */
void CNetMon::sendAlert(CPeer *peer, std::string type){
    if(!peer){ LogPrint(BCLog::NET, "[POC] ERROR !peer\n"); return;}

    CPoCAlert alert(type, peer->addr, peer->addrBind);

    LogPrint(BCLog::NET, "[POC] Sending \"ALERT\": type=%s, a1=%s, a2=%s\n", alert.type, alert.addr1, alert.addr2);

    //Send to node A
    //if(!peer->node) LogPrint(BCLog::NET, "[POC] ERROR !peer->node\n");
    CNode *pA;
    if(peer->node)
        pA = peer->node->getCNode();
    else 
        pA = g_connman->FindNode(peer->addr);
    if(pA){
        LogPrint(BCLog::NET, "[POC] Sending \"ALERT\" to %s\n", pA->addr.ToString());
        g_connman->PushMessage(pA, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));    
    }

    //Send to node B (if connected)
    CNetNode *pBn = g_netmon->findNodeByPeer(peer->addrBind,peer->addr); //getNode(peer->addr);
    if(pBn){
        CNode *pB = pBn->getCNode();
        if(pB){
            LogPrint(BCLog::NET, "[POC] Sending \"ALERT\" to %s\n", pB->addr.ToString());
            g_connman->PushMessage(pB, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));
        }
    }

    if(type != "unverified")
        g_netmon->removePeer(*peer);
LogPrint(BCLog::NET, "[POC] DEBUG: Remove OK\n");

    //TODO: decrease reputation of nodes that keep claiming unverified connections
}

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

    //If we can't connect, send ALERT
    if(!ppeer){
        LogPrint(BCLog::NET, "[POC] ERROR: could not connect\n");
        
        std::string type("connect");
        sendAlert(peer,type);

        return NULL;
    }

    return ppeer;
}
