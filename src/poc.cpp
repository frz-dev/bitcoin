#include <poc.h>
#include <net.h>
#include <netmessagemaker.h>
#include <time.h>

//class CNode;
class CNetMsgMaker;

void CNetMon::sendPoC(CNode *pto, CPoC *poc){
    const CNetMsgMaker msgMaker(pto->GetSendVersion());

    CNode* pfrom = g_netmon->getNode(poc->target_addr)->getCNode();
    if(!pfrom){ LogPrint(BCLog::NET, "[POC] WARNING: pfrom not found\n"); return;} //TODO: return false

    //Send POC
    LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pto->addr.ToString(),poc->id,poc->monitor,poc->target);
    g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::POCCHALLENGE, *poc));

    //Set timeout
    poc->timeout = GetTimeMicros()+((pfrom->nPingUsecTime + pto->nPingUsecTime)*20); //TODO: get pingtime in PEERS and add it to timeout
}
