// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "fat_tree_interdc_switch.h"
#include "callback_pipe.h"
#include "fat_tree_interdc_topology.h"
#include "fat_tree_switch.h"
#include "fat_tree_topology.h"
#include "queue_lossless.h"
#include "queue_lossless_output.h"
#include "routetable.h"

int FatTreeInterDCSwitch::precision_ts = 1;

FatTreeInterDCSwitch::FatTreeInterDCSwitch(EventList &eventlist, string s, switch_type t, uint32_t id,
                                           simtime_picosec delay, FatTreeInterDCTopology *ft, int dc)
        : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _uproutes = NULL;
    _ft = ft;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
    dc_id = dc;
}

void FatTreeInterDCSwitch::receivePacket(Packet &pkt) {

    if (pkt.type() == ETH_PAUSE) {
        EthPausePacket *p = (EthPausePacket *)&pkt;
        // I must be in lossless mode!
        // find the egress queue that should process this, and pass it over for
        // processing.
        printf("Received a Pause Packet\n");
        for (size_t i = 0; i < _ports.size(); i++) {
            LosslessQueue *q = (LosslessQueue *)_ports.at(i);
            if (q->getRemoteEndpoint() && ((Switch *)q->getRemoteEndpoint())->getID() == p->senderID()) {
                q->receivePacket(pkt);
                break;
            }
        }

        return;
    }

    /*  */
    pkt.previous_switch_id = _id;

    /* if (nodename() == "Switch_LowerPod_7" && pkt.size() > 100) {
        printf("Node %s - Pkt %d %d - Packet Received at %lu\n", nodename().c_str(), pkt.from, pkt.id(),
    GLOBAL_TIME/1000); } else if (pkt.size() > 100) { printf("Node %s - Pkt %d %d - Packet Received at %lu\n",
    nodename().c_str(), pkt.from, pkt.id(), GLOBAL_TIME/1000);
    } */

    // printf("Packet Destination is 3 %d\n", pkt.dst());

    if (_packets.find(&pkt) == _packets.end()) {
        // ingress pipeline processing.

        _packets[&pkt] = true;

        const Route *nh = getNextHop(pkt, NULL);
        // set next hop which is peer switch.
        pkt.set_route(*nh);

        // emulate the switching latency between ingress and packet arriving at
        // the egress queue.
        _pipe->receivePacket(pkt);
    } else {
        _packets.erase(&pkt);

        // egress queue processing.
        // cout << "Switch type " << _type <<  " id " << _id << " pkt dst " <<
        // pkt.dst() << " dir " << pkt.get_direction() << endl;

        pkt.hop_count++;

        // printf("From %d - At %s - Hop %d - Time %lu - Typr %d\n", pkt.from,
        //        nodename().c_str(), pkt.hop_count, GLOBAL_TIME, pkt.type());

        if ((pkt.hop_count == 1 && pkt.size() > 100) &&
            (pkt.type() == UEC || pkt.type() == NDP || pkt.type() == SWIFTTRIMMING || pkt.type() == UEC_DROP ||
             pkt.type() == BBR)) {

            simtime_picosec my_time =
                    (GLOBAL_TIME - (4160 * 8 / LINK_SPEED_MODERN * 1000) - (LINK_DELAY_MODERN * 1000));

            pkt.set_ts(my_time);

            if (COLLECT_DATA) {
                // Sent
                std::string file_name = PROJECT_ROOT_PATH / ("sim/output/sent/s" + std::to_string(pkt.from) + ".txt ");
                std::ofstream MyFile(file_name, std::ios_base::app);

                MyFile << my_time / 1000 << "," << 1 << "," << pkt.id() << std::endl;

                MyFile.close();

                // printf("Pkt %d %d - Time %lu %lu - Size %d\n", pkt.from,
                //        pkt.id(), eventlist().now() / 1000, my_time,
                //        pkt.size());
            }
        }

        pkt.sendOn();
    }
};

void FatTreeInterDCSwitch::addHostPort(int addr, int flowid, PacketSink *transport) {
    Route *rt = new Route();
    rt->push_back(_ft->queues_nlp_ns[dc_id][_ft->HOST_POD_SWITCH(addr)][addr][0]);
    rt->push_back(_ft->pipes_nlp_ns[dc_id][_ft->HOST_POD_SWITCH(addr)][addr][0]);
    rt->push_back(transport);
    _fib->addHostRoute(addr, rt, flowid);
}

uint32_t FatTreeInterDCSwitch::adaptive_route_p2c(vector<FibEntry *> *ecmp_set, int8_t (*cmp)(FibEntry *, FibEntry *)) {
    uint32_t choice = 0, min = UINT32_MAX;
    uint32_t start, i = 0;
    static const uint16_t nr_choices = 2;

    do {
        start = random() % ecmp_set->size();

        Route *r = (*ecmp_set)[start]->getEgressPort();
        assert(r && r->size() > 1);
        BaseQueue *q = (BaseQueue *)(r->at(0));
        assert(q);
        if (q->queuesize() < min) {
            choice = start;
            min = q->queuesize();
        }
        i++;
    } while (i < nr_choices);
    return choice;
}

uint32_t FatTreeInterDCSwitch::adaptive_route(vector<FibEntry *> *ecmp_set, int8_t (*cmp)(FibEntry *, FibEntry *)) {
    uint32_t choice = 0;

    uint32_t best_choices[256];
    uint32_t best_choices_count = 0;

    FibEntry *min = (*ecmp_set)[choice];
    best_choices[best_choices_count++] = choice;

    for (uint32_t i = 1; i < ecmp_set->size(); i++) {
        int8_t c = cmp(min, (*ecmp_set)[i]);

        if (c < 0) {
            choice = i;
            min = (*ecmp_set)[choice];
            best_choices_count = 0;
            best_choices[best_choices_count++] = choice;
        } else if (c == 0) {
            assert(best_choices_count < 256);
            best_choices[best_choices_count++] = i;
        }
    }

    assert(best_choices_count >= 1);
    choice = best_choices[random() % best_choices_count];
    return choice;
}

uint32_t FatTreeInterDCSwitch::replace_worst_choice(vector<FibEntry *> *ecmp_set, int8_t (*cmp)(FibEntry *, FibEntry *),
                                                    uint32_t my_choice) {
    uint32_t best_choice = 0;
    uint32_t worst_choice = 0;

    uint32_t best_choices[256];
    uint32_t best_choices_count = 0;

    FibEntry *min = (*ecmp_set)[best_choice];
    FibEntry *max = (*ecmp_set)[worst_choice];
    best_choices[best_choices_count++] = best_choice;

    for (uint32_t i = 1; i < ecmp_set->size(); i++) {
        int8_t c = cmp(min, (*ecmp_set)[i]);

        if (c < 0) {
            best_choice = i;
            min = (*ecmp_set)[best_choice];
            best_choices_count = 0;
            best_choices[best_choices_count++] = best_choice;
        } else if (c == 0) {
            assert(best_choices_count < 256);
            best_choices[best_choices_count++] = i;
        }

        if (cmp(max, (*ecmp_set)[i]) > 0) {
            worst_choice = i;
            max = (*ecmp_set)[worst_choice];
        }
    }

    // might need to play with different alternatives here, compare to worst
    // rather than just to worst index.
    int8_t r = cmp((*ecmp_set)[my_choice], (*ecmp_set)[worst_choice]);
    assert(r >= 0);

    if (r == 0) {
        assert(best_choices_count >= 1);
        return best_choices[random() % best_choices_count];
    } else
        return my_choice;
}

int8_t FatTreeInterDCSwitch::compare_pause(FibEntry *left, FibEntry *right) {
    Route *r1 = left->getEgressPort();
    assert(r1 && r1->size() > 1);
    LosslessOutputQueue *q1 = dynamic_cast<LosslessOutputQueue *>(r1->at(0));
    Route *r2 = right->getEgressPort();
    assert(r2 && r2->size() > 1);
    LosslessOutputQueue *q2 = dynamic_cast<LosslessOutputQueue *>(r2->at(0));

    if (!q1->is_paused() && q2->is_paused())
        return 1;
    else if (q1->is_paused() && !q2->is_paused())
        return -1;
    else
        return 0;
}

int8_t FatTreeInterDCSwitch::compare_queuesize(FibEntry *left, FibEntry *right) {
    Route *r1 = left->getEgressPort();
    assert(r1 && r1->size() > 1);
    BaseQueue *q1 = dynamic_cast<BaseQueue *>(r1->at(0));
    Route *r2 = right->getEgressPort();
    assert(r2 && r2->size() > 1);
    BaseQueue *q2 = dynamic_cast<BaseQueue *>(r2->at(0));

    if (q1->quantized_queuesize() < q2->quantized_queuesize())
        return 1;
    else if (q1->quantized_queuesize() > q2->quantized_queuesize())
        return -1;
    else
        return 0;
}

int8_t FatTreeInterDCSwitch::compare_bandwidth(FibEntry *left, FibEntry *right) {
    Route *r1 = left->getEgressPort();
    assert(r1 && r1->size() > 1);
    BaseQueue *q1 = dynamic_cast<BaseQueue *>(r1->at(0));
    Route *r2 = right->getEgressPort();
    assert(r2 && r2->size() > 1);
    BaseQueue *q2 = dynamic_cast<BaseQueue *>(r2->at(0));

    if (q1->quantized_utilization() < q2->quantized_utilization())
        return 1;
    else if (q1->quantized_utilization() > q2->quantized_utilization())
        return -1;
    else
        return 0;
}

int8_t FatTreeInterDCSwitch::compare_pqb(FibEntry *left, FibEntry *right) {
    // compare pause, queuesize, bandwidth.
    int8_t p = compare_pause(left, right);

    if (p != 0)
        return p;

    p = compare_queuesize(left, right);

    if (p != 0)
        return p;

    return compare_bandwidth(left, right);
}

int8_t FatTreeInterDCSwitch::compare_pq(FibEntry *left, FibEntry *right) {
    // compare pause, queuesize, bandwidth.
    int8_t p = compare_pause(left, right);

    if (p != 0)
        return p;

    return compare_queuesize(left, right);
}

int8_t FatTreeInterDCSwitch::compare_qb(FibEntry *left, FibEntry *right) {
    // compare pause, queuesize, bandwidth.
    int8_t p = compare_queuesize(left, right);

    if (p != 0)
        return p;

    return compare_bandwidth(left, right);
}

int8_t FatTreeInterDCSwitch::compare_pb(FibEntry *left, FibEntry *right) {
    // compare pause, queuesize, bandwidth.
    int8_t p = compare_pause(left, right);

    if (p != 0)
        return p;

    return compare_bandwidth(left, right);
}

void FatTreeInterDCSwitch::permute_paths(vector<FibEntry *> *uproutes) {
    int len = uproutes->size();
    for (int i = 0; i < len; i++) {
        int ix = random() % (len - i);
        FibEntry *tmppath = (*uproutes)[ix];
        (*uproutes)[ix] = (*uproutes)[len - 1 - i];
        (*uproutes)[len - 1 - i] = tmppath;
    }
}

FatTreeInterDCSwitch::routing_strategy FatTreeInterDCSwitch::_strategy = FatTreeInterDCSwitch::NIX;
uint16_t FatTreeInterDCSwitch::_ar_fraction = 0;
uint16_t FatTreeInterDCSwitch::_ar_sticky = FatTreeInterDCSwitch::PER_PACKET;
simtime_picosec FatTreeInterDCSwitch::_sticky_delta = timeFromUs((uint32_t)10);
double FatTreeInterDCSwitch::_ecn_threshold_fraction = 1.0;
int8_t (*FatTreeInterDCSwitch::fn)(FibEntry *, FibEntry *) = &FatTreeInterDCSwitch::compare_queuesize;

Route *FatTreeInterDCSwitch::getNextHop(Packet &pkt, BaseQueue *ingress_port) {

    /* if (pkt.is_bts_pkt && pkt.id() == 356350 && pkt.from == 1) {
        printf("Get Next Hop BTS - %s - DC %d - Switch ID %d\n", nodename().c_str(), dc_id, id);
    } */
    vector<FibEntry *> *available_hops = _fib->getRoutes(pkt.dst());

    if (available_hops) {
        // implement a form of ECMP hashing; might need to revisit based on
        // measured performance.
        uint32_t ecmp_choice = 0;

        if (available_hops->size() > 1)
            switch (_strategy) {
            case NIX:
                abort();
            case ECMP:
                // printf("Pkt Flow ID %d - Path Id %d\n", pkt.flow_id(),
                //        pkt.pathid());
                ecmp_choice = freeBSDHash(pkt.flow_id(), pkt.pathid(), _hash_salt) % available_hops->size();
                break;
            case ADAPTIVE_ROUTING:
                if (_ar_sticky == FatTreeInterDCSwitch::PER_PACKET) {
                    ecmp_choice = adaptive_route(available_hops, fn);
                } else if (_ar_sticky == FatTreeInterDCSwitch::PER_FLOWLET) {
                    if (_flowlet_maps.find(pkt.flow_id()) != _flowlet_maps.end()) {
                        FlowletInfoInterDC *f = _flowlet_maps[pkt.flow_id()];

                        // only reroute an existing flow if its inter packet
                        // time is larger than _sticky_delta and and 50% chance
                        // happens. and (commented out) if the switch has not
                        // taken any other placement decision that we've not
                        // seen the effects of.
                        if (eventlist().now() - f->_last > _sticky_delta &&
                            /*eventlist().now() - _last_choice > _pipe->delay()
                               + BaseQueue::_update_period  &&*/
                            random() % 2 == 0) {
                            uint32_t new_route = adaptive_route(available_hops, fn);
                            if (fn(available_hops->at(f->_egress), available_hops->at(new_route)) < 0) {
                                f->_egress = new_route;
                                _last_choice = eventlist().now();
                                // cout << "Switch " << _type << ":" << _id << "
                                // choosing new path "<<  f->_egress << " for "
                                // << pkt.flow_id() << " at " <<
                                // timeAsUs(eventlist().now()) << " last is " <<
                                // timeAsUs(f->_last) << endl;
                            }
                        }
                        ecmp_choice = f->_egress;

                        f->_last = eventlist().now();
                    } else {
                        ecmp_choice = adaptive_route(available_hops, fn);
                        // cout << "Switch " << _type << ":" << getID() << "
                        // choosing first path "<<  ecmp_choice << " for " <<
                        // pkt.flow_id() << " at " <<
                        // timeAsUs(eventlist().now()) << endl;
                        _last_choice = eventlist().now();

                        _flowlet_maps[pkt.flow_id()] = new FlowletInfoInterDC(ecmp_choice, eventlist().now());
                    }
                }

                break;
            case ECMP_ADAPTIVE:
                ecmp_choice = freeBSDHash(pkt.flow_id(), pkt.pathid(), _hash_salt) % available_hops->size();
                if (random() % 100 < 50)
                    ecmp_choice = replace_worst_choice(available_hops, fn, ecmp_choice);
                break;
            case RR:
                if (_crt_route >= 5 * available_hops->size()) {
                    _crt_route = 0;
                    permute_paths(available_hops);
                }
                ecmp_choice = _crt_route % available_hops->size();
                _crt_route++;
                break;
            case RR_ECMP:
                if (_type == TOR) {
                    if (_crt_route >= 5 * available_hops->size()) {
                        _crt_route = 0;
                        permute_paths(available_hops);
                    }
                    ecmp_choice = _crt_route % available_hops->size();
                    _crt_route++;
                } else
                    ecmp_choice = freeBSDHash(pkt.flow_id(), pkt.pathid(), _hash_salt) % available_hops->size();

                break;
            }

        bool force_routing = false;
        if (force_routing && pkt.size() > 1000 && nodename() == "Switch_LowerPod_0") {
            ecmp_choice = pkt.from;
        }

        FibEntry *e = (*available_hops)[ecmp_choice];

        /*printf("Here2 %d %d@%d - Time %lu - Hops Size %d - IdxID %d - Inc ID "
               "%d - "
               "Direction"
               "%d - EgressPort %d - ECMP Choice %d -"
               "MyId "
               "%d %s\n",
               pkt.size(), pkt.from, pkt.dst(), GLOBAL_TIME / 1000,
               available_hops->size(), pkt.my_idx, pkt.inc_id,
               e->getDirection(), e->getEgressPort()->path_id(), ecmp_choice,
               id, nodename().c_str());
        printf("FROM %d / CHOICE %d / HOPS %d \n", pkt.from, ecmp_choice,
               available_hops->size());
        fflush(stdout);*/
        pkt.set_direction(e->getDirection());

        return e->getEgressPort();
    }

    // no route table entries for this destination. Add them to FIB or fail.
    if (_type == TOR) {
        if (_ft->HOST_POD_SWITCH(pkt.dst() % _ft->no_of_nodes()) == _id && dc_id == _ft->get_dc_id(pkt.dst())) {
            /* printf("TOR - PACKET %d@%d@%d DOWN - DC ID %d - TOT DC %d\n",
                   pkt.id(), pkt.from, pkt.size(), dc_id,
                   _ft->no_of_border_switches()); */
            // this host is directly connected and we are in the same DC
            HostFibEntry *fe = _fib->getHostRoute(pkt.dst() % _ft->no_of_nodes(), pkt.flow_id());
            assert(fe);
            pkt.set_direction(DOWN);
            return fe->getEgressPort();
        } else {
            // route packet up!
            /* printf("TOR - PACKET %d@%d@%d UP - DC ID %d - TOT DC %d\n",
                   pkt.id(), pkt.from, pkt.size(), dc_id,
                   _ft->no_of_border_switches());
            fflush(stdout); */
            if (_uproutes)
                _fib->setRoutes(pkt.dst(), _uproutes);
            else {
                uint32_t podid, agg_min, agg_max;

                if (_ft->get_tiers() == 3) {
                    podid = _id / _ft->tor_switches_per_pod();
                    agg_min = _ft->MIN_POD_AGG_SWITCH(podid);
                    agg_max = _ft->MAX_POD_AGG_SWITCH(podid);
                } else {
                    agg_min = 0;
                    agg_max = _ft->getNAGG() - 1;
                }

                for (uint32_t k = agg_min; k <= agg_max; k++) {
                    for (uint32_t b = 0; b < _ft->bundlesize(AGG_TIER); b++) {
                        Route *r = new Route();
                        r->push_back(_ft->queues_nlp_nup[dc_id][_id][k][b]);
                        assert(((BaseQueue *)r->at(0))->getSwitch() == this);

                        r->push_back(_ft->pipes_nlp_nup[dc_id][_id][k][b]);
                        r->push_back(_ft->queues_nlp_nup[dc_id][_id][k][b]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, UP);
                    }
                }
                _uproutes = _fib->getRoutes(pkt.dst());
                permute_paths(_uproutes);
            }
        }
    } else if (_type == AGG) {
        if (_ft->get_tiers() == 2 || (_ft->HOST_POD(pkt.dst() % _ft->no_of_nodes()) == _ft->AGG_SWITCH_POD_ID(_id) &&
                                      dc_id == _ft->get_dc_id(pkt.dst()))) {
            /* printf("AGG - PACKET %d@%d@%d DOWN - DC ID %d - TOT DC %d\n",
                   pkt.id(), pkt.from, pkt.size(), dc_id,
                   _ft->no_of_border_switches()); */
            // must go down!
            // target NLP id is 2 * pkt.dst()/K
            uint32_t target_tor = _ft->HOST_POD_SWITCH(pkt.dst() % _ft->no_of_nodes());
            for (uint32_t b = 0; b < _ft->bundlesize(AGG_TIER); b++) {
                Route *r = new Route();
                r->push_back(_ft->queues_nup_nlp[dc_id][_id][target_tor][b]);
                assert(((BaseQueue *)r->at(0))->getSwitch() == this);

                r->push_back(_ft->pipes_nup_nlp[dc_id][_id][target_tor][b]);
                r->push_back(_ft->queues_nup_nlp[dc_id][_id][target_tor][b]->getRemoteEndpoint());

                _fib->addRoute(pkt.dst(), r, 1, DOWN);
            }
        } else {
            /* printf("AGG - PACKET %d@%d@%d UP - DC ID %d - TOT DC %d\n",
                   pkt.id(), pkt.from, pkt.size(), dc_id,
                   _ft->no_of_border_switches());
            fflush(stdout); */
            // go up!
            if (_uproutes)
                _fib->setRoutes(pkt.dst() % _ft->no_of_nodes(), _uproutes);
            else {
                uint32_t podpos = _id % _ft->agg_switches_per_pod();
                uint32_t uplink_bundles = _ft->radix_up(AGG_TIER) / _ft->bundlesize(CORE_TIER);

                for (uint32_t l = 0; l < uplink_bundles; l++) {
                    uint32_t core = l * _ft->agg_switches_per_pod() + podpos;
                    for (uint32_t b = 0; b < _ft->bundlesize(CORE_TIER); b++) {
                        Route *r = new Route();
                        r->push_back(_ft->queues_nup_nc[dc_id][_id][core][b]);
                        assert(((BaseQueue *)r->at(0))->getSwitch() == this);

                        r->push_back(_ft->pipes_nup_nc[dc_id][_id][core][b]);
                        r->push_back(_ft->queues_nup_nc[dc_id][_id][core][b]->getRemoteEndpoint());

                        _fib->addRoute(pkt.dst(), r, 1, UP);
                    }
                }
                permute_paths(_fib->getRoutes(pkt.dst()));
            }
        }
    } else if (_type == CORE) {
        if (dc_id == _ft->get_dc_id(pkt.dst())) {
            uint32_t nup = _ft->MIN_POD_AGG_SWITCH(_ft->HOST_POD(pkt.dst() % _ft->no_of_nodes())) +
                           (_id % _ft->agg_switches_per_pod());
            for (uint32_t b = 0; b < _ft->bundlesize(CORE_TIER); b++) {
                Route *r = new Route();

                /* printf("Queue params are %d %d %d %d\n", dc_id, _id, nup, b);
                fflush(stdout); */

                assert(_ft->queues_nc_nup[dc_id][_id][nup][b]);
                r->push_back(_ft->queues_nc_nup[dc_id][_id][nup][b]);
                assert(((BaseQueue *)r->at(0))->getSwitch() == this);

                assert(_ft->pipes_nc_nup[dc_id][_id][nup][b]);
                r->push_back(_ft->pipes_nc_nup[dc_id][_id][nup][b]);

                r->push_back(_ft->queues_nc_nup[dc_id][_id][nup][b]->getRemoteEndpoint());
                _fib->addRoute(pkt.dst(), r, 1, DOWN);
            }
        } else {

            // route packet up!
            if (_uproutes)
                _fib->setRoutes(pkt.dst(), _uproutes);
            else {
                uint32_t podid, agg_min, agg_max;

                for (uint32_t k = 0; k < _ft->no_of_border_switches(); k++) {
                    for (int link_id = 0; link_id < _ft->no_of_links_core_to_same_border(); link_id++) {
                        Route *r = new Route();
                        r->push_back(_ft->queues_nc_nborder[dc_id][_id][k][link_id]);
                        r->push_back(_ft->pipes_nc_nborder[dc_id][_id][k][link_id]);
                        r->push_back(_ft->queues_nc_nborder[dc_id][_id][k][link_id]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, UP);
                    }
                }
                _uproutes = _fib->getRoutes(pkt.dst());
                permute_paths(_uproutes);
            }
        }
    } else if (_type == BORDER) {
        if (dc_id == _ft->get_dc_id(pkt.dst())) {
            // We are already at the right border switch
            /* printf("BORDER - PACKET %d@%d@%d (%d) DOWN - DC ID %d - TOT DC "
                   "%d\n",
                   pkt.id(), pkt.from, pkt.to, pkt.dst(), dc_id,
                   _ft->no_of_border_switches()); */

            for (int nup = 0; nup < _ft->no_of_cores(); nup++) {
                for (int link_id = 0; link_id < _ft->no_of_links_core_to_same_border(); link_id++) {
                    Route *r = new Route();
                    r->push_back(_ft->queues_nborder_nc[dc_id][_id][nup][link_id]);
                    r->push_back(_ft->pipes_nborder_nc[dc_id][_id][nup][link_id]);
                    r->push_back(_ft->queues_nborder_nc[dc_id][_id][nup][link_id]->getRemoteEndpoint());
                    _fib->addRoute(pkt.dst(), r, 1, DOWN);
                }
            }
        } else {
            // We need to cross the DC
            /* printf("BORDER - PACKET %d@%d@%d UP - DC ID %d - TOT DC %d\n",
                   pkt.id(), pkt.from, pkt.size(), dc_id,
                   _ft->no_of_border_switches());
            fflush(stdout); */
            if (_uproutes)
                _fib->setRoutes(pkt.dst(), _uproutes);
            else {
                for (uint32_t k = 0; k < _ft->no_of_border_switches(); k++) {
                    for (int link_id = 0; link_id < _ft->no_of_links_between_border(); link_id++) {
                        Route *r = new Route();
                        if (dc_id == 0) {
                            r->push_back(_ft->queues_nborderl_nborderu[_id][k][link_id]);
                            r->push_back(_ft->pipes_nborderl_nborderu[_id][k][link_id]);
                            r->push_back(_ft->queues_nborderl_nborderu[_id][k][link_id]->getRemoteEndpoint());
                        } else if (dc_id == 1) {
                            r->push_back(_ft->queues_nborderu_nborderl[_id][k][link_id]);
                            r->push_back(_ft->pipes_nborderu_nborderl[_id][k][link_id]);
                            r->push_back(_ft->queues_nborderu_nborderl[_id][k][link_id]->getRemoteEndpoint());
                        }

                        _fib->addRoute(pkt.dst(), r, 1, UP);
                    }
                }
                _uproutes = _fib->getRoutes(pkt.dst());
                permute_paths(_uproutes);
            }
        }
    } else {
        cerr << "Route lookup on switch with no proper type: " << _type << endl;
        abort();
    }
    assert(_fib->getRoutes(pkt.dst()));

    // FIB has been filled in; return choice.
    return getNextHop(pkt, ingress_port);
};
