/*REBREL*/
#include <uint256.h>

static constexpr std::chrono::minutes EPOCH_INTERVAL{1};

void ProxyTx(const CTransactionRef& tx, CConnman& connman);
CTransactionRef FindProxiedTx(const uint256 txid);
void SetTxBroadcasted(CTransactionRef ptx);