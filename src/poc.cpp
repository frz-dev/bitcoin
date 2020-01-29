#include <poc.h>
#include <net.h>
#include <netmessagemaker.h>
#include <protocol.h>
#include <time.h>

class CNode;
class CNetMsgMaker;
class CAddress;

void CNetMon::sendPoC(CNode *pto, CPoC *poc){
    const CNetMsgMaker msgMaker(pto->GetSendVersion());
    //TODO? create poc here?

    CNode* pfrom = g_netmon->getNode(poc->target_addr)->getCNode();
    if(!pfrom){ LogPrint(BCLog::NET, "[POC] WARNING: pfrom not found\n"); return;} //TODO: return false

    //Send POC
    LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pto->addr.ToString(),poc->id,poc->target,poc->monitor);
    g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::POCCHALLENGE, *poc));

    //Set timeout
    poc->timeout = GetTimeMicros()+((pfrom->nPingUsecTime + pto->nPingUsecTime)*4); //TODO: get pingtime in PEERS and add it to timeout
}

void CNetMon::sendAlert(CPeer *peer, std::string type){

    CPoCAlert alert(type, peer->addr, peer->addrBind);

    LogPrint(BCLog::NET, "[POC] Sending \"ALERT\": type=%s, a1=%s, a2=%s\n", alert.type, alert.addr1, alert.addr2);

    //Send to node A
    CNode *pA = peer->node->getCNode();
    g_connman->PushMessage(pA, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));

    //Send to node B (if connected)
    CNetNode *pBn = g_netmon->getNode(peer->addr);
    if(pBn){
        CNode *pB = pBn->getCNode();
        g_connman->PushMessage(pB, CNetMsgMaker(PROTOCOL_VERSION).Make(NetMsgType::POCALERT, alert));
    }
}

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
