#ifndef BITCOIN_POC_H
#define BITCOIN_POC_H

#include <memory>
#include <string>
#include <vector>
#include <logging.h>
#include <algorithm> //remove()
#include <time.h>

#include <net.h>

class CNode;
class CNetNode;
class CPeer;

static const unsigned int AVG_POC_UPDATE_INTERVAL = 5;
static constexpr int64_t MAX_VERIFICATION_TIMEOUT = 5;

/* CPoC */
class CPoC
{
public:
    int id;
    std::string monitor;
    std::string target;
    //Digital Signature?
    std::atomic<int64_t> timeout{0};
    std::string target_addr;
    bool fExpired{false};
    bool fVerified{false}; //We distinguish between the peer's verified status, and the poc status
                           //so  we can keep the peer's previous status unchanged while we wait for poc to complete

    CPoC(){
        id = 0;
        monitor = "";
        target = "";
    };

    CPoC(int i, const std::string& m, const std::string& t, const std::string& ta){
        id = i;
        monitor = m;
        target = t;
        target_addr = ta;
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
    CPoC *poc;
    bool fVerified;
    CNetNode *node;
    int64_t timeout{0};

    CPeer(){
        addr = "";
        addrBind = "";
        fInbound = false;
        poc = NULL;
        fVerified = false;
    };
    CPeer(std::string& a, std::string& aB, bool i){
        addr = a;
        addrBind = aB;
        fInbound = i;
        poc = NULL;
        fVerified = false;
        node = NULL;
    };
    
    bool operator==(const CPeer &peer) const {
        return (addr == peer.addr) && (addrBind == peer.addrBind) && (fInbound == peer.fInbound);
    }

    bool isEqual(const CPeer &peer) {
        return (addr == peer.addrBind) && (addrBind == peer.addr) && (fInbound == !peer.fInbound);
    }

    CPeer getSymmetric(){
        CPeer sym;

        sym.addr = addrBind;
        sym.addrBind = addr;
        sym.fInbound = fInbound;

        return sym;
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
    //int pocId;

    CPoCAlert(){
        type = "";
        addr1 = "";
        addr2 = "";
        //pocId = -1;
    };
    CPoCAlert(const std::string& t, const std::string& a1, const std::string& a2){
        type = t;
        addr1 = a1;
        addr2 = a2;
        //pocId = p;
    };
    
    template <typename Stream>
    void Serialize(Stream& s) const {
        s << type
          << addr1
          << addr2;
          //<< pocId;
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        s >> type
          >> addr1
          >> addr2;
//          >> pocId;
    }
};

/*POC: CNetwork*/
class CNetNode
{
private:
    CNode *cnode;
    CCriticalSection cs_peers;

public:
    std::string addr;
    std::vector<CPeer> vPeers GUARDED_BY(cs_peers);
    //TODO: CPeer *
    //TODO move CPeer info here and make vector<CNetNode> -- how to handle pocId?

    std::vector<CPeer> vPeersToCheck GUARDED_BY(cs_peers);

    CNetNode(){}
    CNetNode(std::string a, CNode *n){
        cnode = n;
        addr = a;
    }

    CNode *getCNode(){
        return cnode;
    }

    void addPeer(CPeer p){
        //TODO: check for duplicates
        vPeers.push_back(p);
    }

    void replacePeers(std::vector<CPeer> newvPeers){
        //Keep old peer.fVerified state
        for (CPeer& newpeer : newvPeers){
            CPeer *peer = findPeer(newpeer);
            if(peer)
                newpeer.fVerified = peer->fVerified;
        }

        //? delete vPeers;
        vPeers = newvPeers;
    }

    bool operator==(const CNetNode &node) const {
        return this->addr == node.addr;
    }


    CPeer* findPeer(CPeer p){
        for (CPeer& peer : vPeers){
            if(peer == p || peer.isEqual(p)) return &peer;
        }
        return nullptr;
    }

    CPeer* findPeer2(CPeer p){
        for (CPeer& peer : vPeers){
            if(peer.isEqual(p)) return &peer;
        }
        return nullptr;
    }

    CPeer* getPeer(std::string a){ //TODO: This should per per (addr,addrBind)
        for (CPeer& peer : vPeers){
            if(peer.addr == a) return &peer;
        }
        return nullptr;
    }

    CPeer* getPeer(int pocId){
        for (CPeer& peer : vPeers){
            if(peer.poc && peer.poc->id == pocId) 
                return &peer;
        }

        return nullptr;
    }

    bool removePeer(std::string a,std::string aB){
        std::vector<CPeer>::iterator it = std::find_if(vPeers.begin(), vPeers.end(), [&](CPeer p) {return p.addr==a && p.addrBind==aB;});

        if ( it != vPeers.end() ){
            vPeers.erase(it);
            return true;
        }

        return false;
    }

    bool removePeer(CPeer p){
LogPrint(BCLog::NET, "[POC] DEBUG: removing peer (%s,%s)\n",p.addr,p.addrBind);
        std::vector<CPeer>::iterator it = std::find_if(vPeers.begin(), vPeers.end(), [&](CPeer peer) {
            return peer==p;
        });

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

    void addPeerToCheck(CPeer p){
        //Avoid duplicates
        for (CPeer peer : vPeersToCheck){
            if(peer == p) return;
        }

        vPeersToCheck.push_back(p);
    }
};

/* CNetMon */
class CNetMon
{
private:
    CCriticalSection cs_netmon;
    std::vector<CNetNode*> vNetNodes GUARDED_BY(cs_netmon);

    void setMaxTimeout();

public:
    CNetMon(){
        setMaxTimeout();
    }

    CNetNode* addNode(std::string addr, CNode *cnode){
//LogPrint(BCLog::NET, "[POC] DEBUG: addNode (%s)\n", addr);
        for(auto& node : vNetNodes)
            if(node->addr == addr){
//LogPrint(BCLog::NET, "[POC] DEBUG: duplicate addNode (%s)\n", addr);
                return node;
            }

        CNetNode *node = new CNetNode(addr, cnode);
        vNetNodes.push_back(node);
        return node;
    }

    CNetNode* getNode(std::string addr){
        for (auto& node : vNetNodes){
            if(node->addr == addr) return node;
        }
        return nullptr;
    }

    CNetNode* findInboundPeer(std::string addr){
        for (auto& node : vNetNodes){
            for (CPeer& peer : node->vPeers)
                if(peer.addrBind == addr) return node;
        }

        return nullptr;
    }    

    CNetNode* findNodeByPeer(std::string addr, std::string addrBind){
        for (auto& node : vNetNodes){
            for (CPeer& peer : node->vPeers)
                if(peer.addr == addr && peer.addrBind == addrBind) return node;
        }

        return nullptr;
    }    

    CNetNode* findPeer(std::string addr, bool fInbound){
        if(fInbound)
            return findInboundPeer(addr);
        else 
            return getNode(addr);
    }

    CPeer* findPeer(CPeer *p){
        for (auto& node : vNetNodes){
            for (CPeer& peer : node->vPeers){
                if(peer == *p) 
                    return &peer;
            }
                
        }

        return nullptr;
    }

    CPeer* findPeer2(CPeer p){
        for (auto& node : vNetNodes){
            for (CPeer& peer : node->vPeers){
                if(peer.isEqual(p)) 
                    return &peer;
            }   
        }

        return nullptr;
    }

    bool removePeer(CPeer p){
        CPeer *p2 = findPeer2(p);
        if(p2){
            p2->node->removePeer(*p2);
        }

        return p.node->removePeer(p);;
    }

    bool removeNode(std::string a){ //TODO 151 remove(CNetNode)
        LogPrint(BCLog::NET, "[POC] Removing node: %s\n", a);
        std::vector<CNetNode*>::iterator it = std::find_if(vNetNodes.begin(), vNetNodes.end(), [&](CNetNode *n) {return n->addr==a;});

        std::vector<CPeer *> toDelete;
        if ( it != vNetNodes.end() ){
            for(auto& peer : (*it)->vPeers){
                toDelete.push_back(&(*it));
            }

            for(auto& peer : toDelete)
                removePeer(peer);

//            delete (*it);
            vNetNodes.erase(it);
            //? vNetNodes.shrink_to_fit();
            return true;
        }

        return false;
    }

    void GetNodes(std::vector<CNetNode*>& vnetnodes){
        vnetnodes.clear();
        //{
        //LOCK(cs_vNetNodes);
        vnetnodes.reserve(vNetNodes.size());
        for (auto pnetnode : vNetNodes) {
            // vnetnodes.emplace_back();
            // pnetnode.copyNode(vnetnodes.back());
            vnetnodes.push_back(pnetnode);
        }
        //}
    }

    void sendPoC(CNode *pto, CPoC *poc);
    void sendAlert(CPeer *peer, std::string type);
    CNode* connectNode(std::string addr);
    CNode* connectNode(CPeer *peer);
};
/**/

extern std::unique_ptr<CNetMon> g_netmon;

#endif // BITCOIN_POC_H
