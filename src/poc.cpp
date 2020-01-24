#include <netmessagemaker.h>
#include <poc.h>

class CNetMsgMaker;

void CNetMon::sendPoC(CNode *pto, CPoC poc){
    const CNetMsgMaker msgMaker(pto->GetSendVersion());

    //TODO SetTimer
    LogPrint(BCLog::NET, "[POC] Sending POC to %s: id:%d|target:%s|monitor:%s\n", pto->addr.ToString(),poc.id,poc.monitor,poc.target);
    g_connman->PushMessage(pto, msgMaker.Make(NetMsgType::POCCHALLENGE, poc));

    removeNode(pto->addr.ToString());
}
