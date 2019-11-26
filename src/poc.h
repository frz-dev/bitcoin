#ifndef BITCOIN_POC_H
#define BITCOIN_POC_H

#include <memory>
#include <string>
#include <vector>
//#include <logging.h>

/* CPoC */
class CPoC
{
public:
    int id;
    std::string monitor;
    std::string target;
    //Digital Signature?

    CPoC(){
        id = 0;
        monitor = "";
        target = "";
    };

    CPoC(int i, const std::string& m, const std::string& t){
        id = i;
        monitor = m;
        target = t;
    };
    //~CPeer();

    template <typename Stream>
    void Serialize(Stream& s) const {
        s << id
          << monitor
          << target;
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        s >> id
          >> monitor
          >> target;
    }
};
/**/

/* CPeer */
class CPeer
{
public:
    std::string addr;
    std::string addrBind;
    bool fInbound;
    bool fVerified;
    int pocId;

    CPeer(){
        addr = "";
        addrBind = "";
        fInbound = false;
        fVerified = false;
        pocId = -1;
    };
    CPeer(std::string& a, std::string& aB, bool i){
        addr = a;
        addrBind = aB;
        fInbound = i;
        fVerified = false;
        pocId = -1;
    };
    //~CPeer();

    template <typename Stream>
    void Serialize(Stream& s) const {
        s << addr
          << addrBind
          << fInbound
          << fVerified;
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        s >> addr
          >> addrBind
          >> fInbound
          >> fVerified;
    }

#undef X
#define X(name) peer.name = name
    void copyPeer(CPeer &peer)
    {
        X(addr);
        X(addrBind);
        X(fInbound);
        X(fVerified);
        X(pocId); //?
    }
#undef X
};

/*POC: CNetwork*/
class CNetNode
{
public:
    std::string addr;
    std::vector<CPeer> vPeers;

    CNetNode(){}
    CNetNode(std::string a){
        addr = a;
    }

    void addPeer(CPeer p){
        vPeers.push_back(p);
    }

    CPeer* getPeer(std::string a){
        for (CPeer& peer : vPeers){
            if(peer.addr == a) return &peer;
        }
        return nullptr;
    }

    CPeer* getPeer(int pocId){
        for (CPeer& peer : vPeers){
            if(peer.pocId == pocId) return &peer;
        }
        return nullptr;
    }

    void copyNode(CNetNode &node){
        node.addr = addr;

        node.vPeers.clear();
        node.vPeers.reserve(vPeers.size());
        for (CPeer& ppeer : vPeers) {
            node.vPeers.emplace_back();
            ppeer.copyPeer(node.vPeers.back());
        }
    }
};

/* CNetMon */
class CNetMon
{
private:
    std::vector<CNetNode> vNetNodes;

public:
    void addNode(std::string addr){
        vNetNodes.push_back(CNetNode(addr));
    }

    CNetNode* getNode(std::string addr){
        for (CNetNode& node : vNetNodes){
            if(node.addr == addr) return &node;
        }
        return nullptr;
    }

    void GetNodes(std::vector<CNetNode>& vnetnodes){
        vnetnodes.clear();
        //{
        //LOCK(cs_vNetNodes);
        vnetnodes.reserve(vNetNodes.size());
        for (CNetNode& pnetnode : vNetNodes) {
            vnetnodes.emplace_back();
            pnetnode.copyNode(vnetnodes.back());
        }
        //}
    }
};
/**/

/*POC*/
extern std::unique_ptr<CNetMon> g_netmon;
/**/

#endif // BITCOIN_POC_H
