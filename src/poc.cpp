#include <poc.h>
#include <net.h>
#include <netmessagemaker.h>

//class CNode;
class CNetMsgMaker;

void CNetMon::sendPoC(CNode *pto, CPoC poc){
    const CNetMsgMaker msgMaker(pto->GetSendVersion());

    LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pto->addr.ToString(),poc.id,poc.monitor,poc.target);
    g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::POCCHALLENGE, poc));

    //TODO SetTimer
    // CNetNode *node = g_netmon->getNode(pto->addr.ToString());
    // node->getPeer()
    // peer.poc->timeout = 0;

    //removeNode(pto->addr.ToString());
}
