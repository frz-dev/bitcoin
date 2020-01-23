#ifndef BITCOIN_POC_H
#define BITCOIN_POC_H

#include <memory>
#include <string>
#include <vector>
#include <logging.h>
#include <algorithm> //remove()

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
    
    bool operator==(const CPeer &peer) const {
        return this->addr == peer.addr;
    }

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

/*POC: CPoCAlert*/
class CPoCAlert
{
public:
    std::string type;
    std::string addr1;
    std::string addr2;
    int pocId;

    CPoCAlert(){
        type = "";
        addr1 = "";
        addr2 = "";
        pocId = -1;
    };
    CPoCAlert(std::string& t, std::string& a1, std::string& a2, int p){
        type = t;
        addr1 = a1;
        addr2 = a2;
        pocId = p;
    };
    
    template <typename Stream>
    void Serialize(Stream& s) const {
        s << type
          << addr1
          << addr1
          << pocId;
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        s >> type
          >> addr1
          >> addr1
          >> pocId;
    }
};

/*POC: CNetwork*/
class CNetNode
{
public:
    std::string addr;
    std::vector<CPeer> vPeers; //TODO move CPeer info here and make vector<CNetNode> -- how to handle pocId?

    CNetNode(){}
    CNetNode(std::string a){
        addr = a;
    }

    bool replacePeers(std::vector<CPeer> newvPeers){
        std::vector<CPeer>().swap(vPeers);
        vPeers = newvPeers;
    }

    bool operator==(const CNetNode &node) const {
        return this->addr == node.addr;
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

    bool removePeer(std::string a){
        std::vector<CPeer>::iterator it = std::find_if(vPeers.begin(), vPeers.end(), [&](CPeer p) {return p.addr==a;});

        if ( it != vPeers.end() ){
            vPeers.erase(it);
            return true;
        }

        return false;
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

    CNetNode* findInboundPeer(std::string addr){
        for (CNetNode& node : vNetNodes){
            for (CPeer& peer : node.vPeers)
                if(peer.addrBind == addr) return &node;
        }

        return nullptr;
    }    

    CNetNode* findPeer(std::string addr, bool fInbound){
        if(fInbound)
            return findInboundPeer(addr);
        else 
            return getNode(addr);
    }


    bool removeNode(std::string a){
        std::vector<CNetNode>::iterator it = std::find_if(vNetNodes.begin(), vNetNodes.end(), [&](CNetNode n) {return n.addr==a;});

        if ( it != vNetNodes.end() ){
            vNetNodes.erase(it);
            return true;
        }

        return false;
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
