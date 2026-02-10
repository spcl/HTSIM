// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-  
#include <climits>
#include <unordered_set>
#include "routetable.h"
#include "network.h"
#include "queue.h"
#include "pipe.h"

RouteTable::~RouteTable() {
    // Track already-deleted pointers to handle shared vectors
    // (e.g. FatTreeSwitch::_uproutes is shared across destinations via setRoutes)
    std::unordered_set<void*> deleted;
    for (auto& [dest, vec] : _fib) {
        if (vec && deleted.insert(vec).second) {
            for (auto* entry : *vec) {
                delete entry;
            }
            delete vec;
        }
    }
    for (auto& [dest, map] : _hostfib) {
        if (map && deleted.insert(map).second) {
            for (auto& [fid, entry] : *map) {
                delete entry;
            }
            delete map;
        }
    }
}

void RouteTable::addRoute(int destination, Route* port, int cost, packet_direction direction){  
    auto it = _fib.find(destination);
    if (it == _fib.end()) {
        auto* vec = new vector<FibEntry*>();
        _fib[destination] = vec;
        it = _fib.find(destination);
    }
    assert(port!=NULL);
    it->second->push_back(new FibEntry(port,cost,direction));
}

void RouteTable::addHostRoute(int destination, Route* port, int flowid){  
    auto it = _hostfib.find(destination);
    if (it == _hostfib.end()) {
        auto* m = new unordered_map<int, HostFibEntry*>();
        _hostfib[destination] = m;
        it = _hostfib.find(destination);
    }
    assert(port!=NULL);
    (*it->second)[flowid] = new HostFibEntry(port,flowid);
}


vector<FibEntry*>* RouteTable::getRoutes(int destination){
    auto it = _fib.find(destination);
    return (it != _fib.end()) ? it->second : nullptr;
}

HostFibEntry* RouteTable::getHostRoute(int destination, int flowid){
    auto it = _hostfib.find(destination);
    if (it == _hostfib.end())
        return nullptr;
    auto it2 = it->second->find(flowid);
    return (it2 != it->second->end()) ? it2->second : nullptr;
}

void RouteTable::setRoutes(int destination, vector<FibEntry*>* routes){
    _fib[destination] = routes;
}