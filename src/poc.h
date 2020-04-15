/*POC*/
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
static const unsigned int MIN_POC_UPDATE_INTERVAL = 1;
static const unsigned int MAX_POC_UPDATE_INTERVAL = 10;
static const int64_t MAX_VERIFICATION_TIMEOUT = 10000;

#define F_INBOUND true
#define F_OUTBOUND false

std::string getOurAddr(CNode *p);

/* CPoC */
class CPoC
{
public:
    int id;
    std::string monitor;
    std::string target;

    //Digital Signature?
    std::atomic<int64_t> timeout{0};
    std::atomic<int64_t> maxTimeout{MAX_VERIFICATION_TIMEOUT};
    bool fExpired {false};
//    std::string target_addr; //Target ID 

//    bool fVerified{false}; //We distinguish between the peer's verified status, and the poc status
                           //so  we can keep the peer's previous status unchanged while we wait for poc to complete

    CPoC(){
        id = 0;
        monitor = "";
        target = "";
//        timeout = GetTimeMicros()+maxTimeout;
    };

    CPoC(int i, const std::string& m, const std::string& t){
        id = i;
        monitor = m;
        target = t;
//        timeout = GetTimeMicros()+maxTimeout;
    };

    /* Updates PoC */
    void update(void){
        id = rand() % 100000;
        int64_t nNow = GetTimeMicros();
        timeout = nNow+maxTimeout;
        fExpired = false;
    }

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
    CNetNode *node;
    std::string addr;
    bool fInbound;

    CPoC *poc {NULL};
//    bool fVerified{false};

    CPeer(){
        addr = "";
        fInbound = false;
    };
    CPeer(std::string& a, CNetNode *n, bool i){
        node = n;
        addr = a;
        fInbound = i;
    };
    
    bool operator==(const CPeer &peer) const {
        return (addr == peer.addr) && (fInbound == peer.fInbound);
    }

    // bool isSymmetric(const CPeer &peer) {
    //     return (addr == peer.node->addr) && (peer.node->addr == peer.addr) && (fInbound == !peer.fInbound);
    // }

    // CPeer getSymmetric(){
    //     CPeer sym;

    //     sym.addr = addrBind;
    //     sym.addrBind = addr;
    //     sym.fInbound = !fInbound;

    //     return sym;
    // }

    template <typename Stream>
    void Serialize(Stream& s) const {
        s << addr
          << fInbound;
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        s >> addr
          >> fInbound;
    }

#undef X
#define X(name) peer.name = name
    void copyPeer(CPeer &peer)
    {
        X(addr);
        X(fInbound);
    }
#undef X
};

// /* CPoCAlert */
// class CPoCAlert
// {
// public:
//     std::string type;
//     std::string addr1;
//     std::string addr2;
//     //int pocId;

//     CPoCAlert(){
//         type = "";
//         addr1 = "";
//         addr2 = "";
//         //pocId = -1;
//     };
//     CPoCAlert(const std::string& t, const std::string& a1, const std::string& a2){
//         type = t;
//         addr1 = a1;
//         addr2 = a2;
//         //pocId = p;
//     };
    
//     template <typename Stream>
//     void Serialize(Stream& s) const {
//         s << type
//           << addr1
//           << addr2;
//           //<< pocId;
//     }

//     template <typename Stream>
//     void Unserialize(Stream& s) {
//         s >> type
//           >> addr1
//           >> addr2;
// //          >> pocId;
//     }
// };

/* CNetNode */
class CNetNode
{
private:
    CNode *cnode;
    CCriticalSection cs_peers;
    int changes {0};

public:
    std::string addr;
    std::vector<CPeer> vPeers GUARDED_BY(cs_peers);
    //TODO-POC: vInPeers -- or CPeer * and mix all
    //Peers//
    //TODO: CPeer *
    //? TODO: move CPeer info here and make vector<CNetNode> -- how to handle pocId?
//    std::vector<CPeer> vPeersToCheck GUARDED_BY(cs_peers);
    
    //PoC//
    CPoC *poc{NULL};
    unsigned int nextPoCRound = {AVG_POC_UPDATE_INTERVAL};

    //Methods//
    CNetNode(){}
    CNetNode(std::string a, CNode *n){
        addr = a;
        cnode = n;
    }

    CNode *getCNode(){
        return cnode;
    }

    void addPeer(CPeer p){
        //TODO: check for duplicates
        LOCK(cs_peers);
        vPeers.push_back(p);

        changes++;
    }

    void addPeer(std::string addr, bool i){
        CPeer p(addr, this, i);

        addPeer(p);
    }

    void updateFreq(void){
        if(changes > 0){
            nextPoCRound -= changes;

            if(nextPoCRound < MIN_POC_UPDATE_INTERVAL)
                nextPoCRound = MIN_POC_UPDATE_INTERVAL;
        }
        else if(nextPoCRound < MAX_POC_UPDATE_INTERVAL)
                nextPoCRound++;
    }



    //TODO: name updatePeers; maintain 'history' of connection 
    //(how long has a connection existed, does it appear/disappear?)
    //TODO: update instead of replacing (change vector<CPeer> to vector<CPeer*>)
    // void replacePeers(std::vector<CPeer> newvPeers){
    //     int changes = 0;

    //     LOCK(cs_peers);

    //     //TODO: check if there are changes in the peer list. If so, update Freq
    //     //This makes freq proportional to the number of connections
    //     //For every change increase frequency of 1 sec (i.e. decrease nextUpdate by 1, MIN=1)
    //     //If

    //     //Keep old peer.fVerified state so the peers keeps verified while waiting for the new poc to complete
    //     for (CPeer& newpeer : newvPeers){
    //         CPeer *peer = findPeer(newpeer); //TODO: use == or change findPeer. a-b is different from b-a
    //         if(peer)
    //             newpeer.fVerified = peer->fVerified;
    //         else changes++;
    //     }

    //     //Check deleted peers
    //     for (CPeer& peer : vPeers){
    //         bool found = false;
    //         for (CPeer& newpeer : newvPeers){
    //             if(newpeer == peer){
    //                 found = true;
    //                 break;
    //             }
    //         }
    //         if(!found) changes++;
    //     }

    //     /* Update frequency */
    //     if(changes>0){
    //         updateFreq -= changes;

    //         if(updateFreq < MIN_POC_UPDATE_INTERVAL)
    //             updateFreq = MIN_POC_UPDATE_INTERVAL;
    //     }
    //     else if(updateFreq < MAX_POC_UPDATE_INTERVAL)
    //             updateFreq++;
        

    //     vPeers.clear();
    //     vPeers = newvPeers;
    // }

    bool operator==(const CNetNode &node) const {
        return this->addr == node.addr;
    }

    CPeer* getPeer(std::string a){
        LOCK(cs_peers);
        for (CPeer& peer : vPeers){
            if(peer.addr == a) return &peer;
        }
        return nullptr;
    }

    //old findPeer
    CPeer* getPeer(CPeer p){
        LOCK(cs_peers);
        for (CPeer& peer : vPeers){
            if(peer == p) return &peer;
        }
        return nullptr;
    }

    // CPeer* findPeer2(CPeer p){ //TODO: rename findSymmetric
    //     LOCK(cs_peers);
    //     for (CPeer& peer : vPeers){
    //         if(peer.isSymmetric(p)) return &peer;
    //     }
    //     return nullptr;
    // }


    bool removePeer(std::string addr){
        LOCK(cs_peers);

        if(vPeers.empty()) return false;

        std::vector<CPeer>::iterator it = std::find_if(vPeers.begin(), vPeers.end(), [&](CPeer peer) {
            return peer.addr == addr;
        });

        if (it != vPeers.end() ){
LogPrint(BCLog::NET, "[POC] DEBUG: removing peer (%s)\n", addr);
            vPeers.erase(it);
            //vPeers.shrink_to_fit();

            changes++;
            return true;
        }

        return false;
    }

    void copyNode(CNetNode &node){
        LOCK(cs_peers);
        node.addr = addr;

        node.vPeers.clear();
        node.vPeers.reserve(vPeers.size());
        for (CPeer& ppeer : vPeers) {
            node.vPeers.emplace_back();
            ppeer.copyPeer(node.vPeers.back());
        }
    }

    // void addPeerToCheck(CPeer p){
    //     LOCK(cs_peers);
    //     //Avoid duplicates
    //     for (CPeer peer : vPeersToCheck){
    //         if(peer == p) return;
    //     }

    //     vPeersToCheck.push_back(p);
    // }
};

/* CNetMon */
class CNetMon
{
private:
    CCriticalSection cs_netnodes;
    std::vector<CNetNode*> vNetNodes GUARDED_BY(cs_netnodes);
    //TODO? List of connections (instead of a list for each Node)

    void setMaxTimeout();

public:
    CNetMon(){
        //setMaxTimeout();
    }

    /* Find node by addr */
    CNetNode* getNetNode(std::string addr){
        LOCK(cs_netnodes);
        for (auto& node : vNetNodes){
            if(node->addr == addr) return node;
        }
        return nullptr;
    }

    /* Add new node to the topology */
    CNetNode* addNetNode(std::string addr, CNode *cnode){
        if(addr.empty() || !cnode) return NULL;

LogPrint(BCLog::NET, "[POC] DEBUG: addNode (%s)\n", addr);
        LOCK(cs_netnodes);
        //Add new NetNode//
        CNetNode *node = new CNetNode(addr, cnode);
        vNetNodes.push_back(node);

        return node;
    }

    CNetNode* findInboundPeer(std::string addr){
        LOCK(cs_netnodes);
        for (auto& node : vNetNodes){
            for (CPeer& peer : node->vPeers)
                if(peer.node->addr == addr) return node;
        }

        return nullptr;
    }    

    CNetNode* findNodeByPeer(std::string addr){
        LOCK(cs_netnodes);
        for (auto& node : vNetNodes){
            for (CPeer& peer : node->vPeers)
                if(peer.addr == addr) return node;
        }

        return nullptr;
    }    

    CNetNode* findPeer(std::string addr, bool fInbound){
        if(fInbound)
            return findInboundPeer(addr);
        else 
            return getNetNode(addr);
    }

    CPeer* findPeer(CPeer *p){
        LOCK(cs_netnodes);
        for (auto& node : vNetNodes){
            for (CPeer& peer : node->vPeers){
                if(peer == *p) 
                    return &peer;
            }
                
        }

        return nullptr;
    }

    // CPeer* findPeer2(CPeer p){
    //     LOCK(cs_netnodes);
    //     for (auto& node : vNetNodes){
    //         for (CPeer& peer : node->vPeers){
    //             if(peer.isSymmetric(p)) 
    //                 return &peer;
    //         }   
    //     }

    //     return nullptr;
    // }

    bool removeConnection(CPeer p){
        //Delete symmetric, if present
        // CPeer *p2 = findPeer2(p);
        // if(p2){
        //     p2->node->removePeer(*p2);
        // }

        return p.node->removePeer(p.addr);
    }

    bool removeNode(std::string a){ //TODO 151 remove(CNetNode)
        LOCK(cs_netnodes);
        LogPrint(BCLog::NET, "[POC] Removing node: %s\n", a);
        std::vector<CNetNode*>::iterator it = std::find_if(vNetNodes.begin(), vNetNodes.end(), [&](CNetNode *n) {return n->addr==a;});

        std::vector<CPeer> toDelete;
        if ( it != vNetNodes.end() ){
            for(CPeer peer : (*it)->vPeers){
                toDelete.push_back(peer);
            }

            //Delete connections
            for(auto peer : toDelete)
                removeConnection(peer);

            //Delete object and shrink vector
            delete (*it);
            vNetNodes.erase(it);
            vNetNodes.shrink_to_fit();

            return true;
        }

        return false;
    }

    /* Returns a copy of the node */
    void GetNodes(std::vector<CNetNode*>& vnetnodes){
        LOCK(cs_netnodes);
        vnetnodes.clear();

        vnetnodes.reserve(vNetNodes.size());
        for (auto pnetnode : vNetNodes) {
            vnetnodes.push_back(pnetnode);
        }
    }

    void sendMon(CNode *pnode);
    void startPoCRound(CNode *pto);
    void endPocRound(CNode *pnode);
    //void sendVerified(CNode *pto);
    CNode* connectNode(std::string addr);
    CNode* connectNode(CPeer *peer);
};
/**/

extern std::unique_ptr<CNetMon> g_netmon;

#endif // BITCOIN_POC_H
