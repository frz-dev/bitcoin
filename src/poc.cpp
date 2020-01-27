#include <poc.h>
#include <net.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <time.h>

class CNode;
class CNetMsgMaker;
class CAddress;

/* sendPoC */
void CNetMon::sendPoC(CNode *pto, CPoC *poc){
    int64_t nNow = GetTimeMicros();
    const CNetMsgMaker msgMaker(pto->GetSendVersion());
    //TODO? create poc here?

    CNode* pfrom = g_netmon->getNode(poc->target_addr)->getCNode();
    if(!pfrom){ LogPrint(BCLog::NET, "[POC] WARNING: pfrom not found\n"); return;} //TODO: return false

    //Set timeout
    if(pto->nPingUsecTime+pfrom->nPingUsecTime <= 0) 
        poc->timeout = nNow + MAX_VERIFICATION_TIMEOUT;
    else
        poc->timeout = nNow+((pto->nPingUsecTime+pfrom->nPingUsecTime)*3);

    //TODO: peer.timeout = poc.timeout
    CPeer *peer = pfrom->netNode->getPeer(poc->id);
    if(peer) peer->timeout = poc->timeout;

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
    if(!peer->node) LogPrint(BCLog::NET, "[POC] ERROR !peer->node\n");
    CNode *pA;
    if(peer->node)
        pA = peer->node->getCNode();
    else 
        pA = g_connman->FindNode(peer->addr);
    if(pA){
        LogPrint(BCLog::NET, "[POC] Sending \"ALERT\" to %s\n", pA->addr.ToString());
        g_connman->PushMessage(pA, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));    

        //TODO? : it crashes
        // CNetNode *pAn = pA->netNode;
        // if(pAn)
        //     pAn->removePeer(peer);

    }

    //Send to node B (if connected)
    CNetNode *pBn = g_netmon->getNode(peer->addr);
    if(pBn){
        CNode *pB = pBn->getCNode();
        if(pB){
            LogPrint(BCLog::NET, "[POC] Sending \"ALERT\" to %s\n", pB->addr.ToString());
            g_connman->PushMessage(pB, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));
        }
        //TODO? : it crashes
        // pBn->removePeer(peer);
    }

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
