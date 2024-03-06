// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "fat_tree_interdc_topology.h"
#include "string.h"
#include <sstream>
#include <vector>

#include "compositequeue.h"
#include "compositequeuebts.h"
#include "ecnqueue.h"
#include "fat_tree_interdc_switch.h"
#include "main.h"
#include "prioqueue.h"
#include "queue.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"
#include "swift_scheduler.h"
#include <iostream>

string ntoa(double n);
string itoa(uint64_t n);

// default to 3-tier topology.  Change this with set_tiers() before calling the
// constructor.
uint32_t FatTreeInterDCTopology::_tiers = 3;
uint32_t FatTreeInterDCTopology::_os = 1;
uint32_t FatTreeInterDCTopology::_os_ratio_stage_1 = 1;
int FatTreeInterDCTopology::kmin = -1;
int FatTreeInterDCTopology::kmax = -1;
int FatTreeInterDCTopology::bts_trigger = -1;
bool FatTreeInterDCTopology::bts_ignore_data = true;
int FatTreeInterDCTopology::num_failing_links = -1;
//  extern int N;

FatTreeInterDCTopology::FatTreeInterDCTopology(
        uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
        QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *fit,
        queue_type q, simtime_picosec latency, simtime_picosec switch_latency,
        queue_type snd) {
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _logger_factory = logger_factory;
    _eventlist = ev;
    ff = fit;
    _qt = q;
    _sender_qt = snd;
    failed_links = 0;
    _hop_latency = latency;
    _switch_latency = switch_latency;

    cout << "Fat Tree topology with " << timeAsUs(_hop_latency)
         << "us links and " << timeAsUs(_switch_latency)
         << "us switching latency." << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeInterDCTopology::FatTreeInterDCTopology(
        uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
        QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *fit,
        queue_type q) {
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _logger_factory = logger_factory;
    _eventlist = ev;
    ff = fit;
    _qt = q;
    _sender_qt = FAIR_PRIO;
    failed_links = 0;
    _hop_latency = timeFromUs((uint32_t)1);
    _switch_latency = timeFromUs((uint32_t)0);

    cout << "Fat tree topology (1) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeInterDCTopology::FatTreeInterDCTopology(
        uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
        QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *fit,
        queue_type q, uint32_t num_failed) {
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _hop_latency = timeFromUs((uint32_t)1);
    _switch_latency = timeFromUs((uint32_t)0);
    _logger_factory = logger_factory;
    _qt = q;
    _sender_qt = FAIR_PRIO;

    _eventlist = ev;
    ff = fit;

    failed_links = num_failed;

    cout << "Fat tree topology (2) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeInterDCTopology::FatTreeInterDCTopology(
        uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
        QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *fit,
        queue_type qtype, queue_type sender_qtype, uint32_t num_failed) {
    _linkspeed = linkspeed;
    _queuesize = queuesize;
    _hop_latency = timeFromUs((uint32_t)1);
    _switch_latency = timeFromUs((uint32_t)0);
    _logger_factory = logger_factory;
    _qt = qtype;
    _sender_qt = sender_qtype;

    _eventlist = ev;
    ff = fit;

    failed_links = num_failed;

    cout << "Fat tree topology (3) with " << no_of_nodes << " nodes" << endl;
    set_params(no_of_nodes);

    init_network();
}

void FatTreeInterDCTopology::set_params(uint32_t no_of_nodes) {
    cout << "Set params " << no_of_nodes << endl;
    cout << "QueueSize " << _queuesize << endl;
    _no_of_nodes = 0;
    K = 0;
    if (_tiers == 3) {
        while (_no_of_nodes < no_of_nodes) {
            K++;
            _no_of_nodes = K * K * K / 4;
        }
        if (_no_of_nodes > no_of_nodes) {
            cerr << "Topology Error: can't have a 3-Tier FatTree with "
                 << no_of_nodes << " nodes, the closet is " << _no_of_nodes
                 << "nodes with K=" << K << "\n";
            exit(1);
        }

        // OverSub Checks
        if (_os_ratio_stage_1 > (K * K / (2 * K))) {
            printf("OverSub on Tor to Aggregation too aggressive, select a "
                   "lower ratio\n");
            exit(1);
        }
        if (_os > (K * K / 4) / 2) {
            printf("OverSub on Aggregation to Core too aggressive, select a "
                   "lower ratio\n");
            exit(1);
        }

        int NK = (K * K / 2);
        NSRV = (K * K * K / 4);
        NTOR = NK;
        NAGG = NK;
        NPOD = K;
        NCORE = (K * K / 4) / (_os);
    } else if (_tiers == 2) {
        // We want a leaf-spine topology
        while (_no_of_nodes < no_of_nodes) {
            K++;
            _no_of_nodes = K * K / 2;
        }
        if (_no_of_nodes > no_of_nodes) {
            cerr << "Topology Error: can't have a 2-Tier FatTree with "
                 << no_of_nodes << " nodes, the closet is " << _no_of_nodes
                 << "nodes with K=" << K << "\n";
            exit(1);
        }
        int NK = K;
        NSRV = K * K / 2;
        NTOR = NK;
        NAGG = (NK / 2) / (_os);
        NPOD = 1;
        NCORE = 0;
    } else {
        cerr << "Topology Error: " << _tiers << " tier FatTree not supported\n";
        exit(1);
    }

    cout << "_no_of_nodes " << _no_of_nodes << endl;
    cout << "K " << K << endl;
    cout << "Queue type " << _qt << endl;

    switches_lp.resize(number_datacenters, vector<Switch *>(NTOR));
    switches_up.resize(number_datacenters, vector<Switch *>(NAGG));
    switches_c.resize(number_datacenters, vector<Switch *>(NCORE));
    switches_border.resize(number_datacenters,
                           vector<Switch *>(number_border_switches));

    // These vectors are sparse - we won't use all the entries
    if (_tiers == 3) {
        pipes_nc_nup.resize(
                number_datacenters,
                vector<vector<Pipe *>>(NCORE, vector<Pipe *>(NAGG)));
        queues_nc_nup.resize(
                number_datacenters,
                vector<vector<BaseQueue *>>(NCORE, vector<BaseQueue *>(NAGG)));
    }

    pipes_nup_nlp.resize(number_datacenters,
                         vector<vector<Pipe *>>(NAGG, vector<Pipe *>(NTOR)));
    queues_nup_nlp.resize(
            number_datacenters,
            vector<vector<BaseQueue *>>(NAGG, vector<BaseQueue *>(NTOR)));

    pipes_nlp_ns.resize(number_datacenters,
                        vector<vector<Pipe *>>(NTOR, vector<Pipe *>(NSRV)));
    queues_nlp_ns.resize(
            number_datacenters,
            vector<vector<BaseQueue *>>(NTOR, vector<BaseQueue *>(NSRV)));

    if (_tiers == 3) {
        pipes_nup_nc.resize(
                number_datacenters,
                vector<vector<Pipe *>>(NAGG, vector<Pipe *>(NCORE)));
        queues_nup_nc.resize(
                number_datacenters,
                vector<vector<BaseQueue *>>(NAGG, vector<BaseQueue *>(NCORE)));
    }

    pipes_nlp_nup.resize(number_datacenters,
                         vector<vector<Pipe *>>(NTOR, vector<Pipe *>(NAGG)));
    pipes_ns_nlp.resize(number_datacenters,
                        vector<vector<Pipe *>>(NSRV, vector<Pipe *>(NTOR)));
    queues_nlp_nup.resize(
            number_datacenters,
            vector<vector<BaseQueue *>>(NTOR, vector<BaseQueue *>(NAGG)));
    queues_ns_nlp.resize(
            number_datacenters,
            vector<vector<BaseQueue *>>(NSRV, vector<BaseQueue *>(NTOR)));

    // Double Check Later
    pipes_nborder_nc.resize(number_datacenters,
                            vector<vector<Pipe *>>(number_border_switches,
                                                   vector<Pipe *>(NCORE)));
    queues_nborder_nc.resize(
            number_datacenters,
            vector<vector<BaseQueue *>>(number_border_switches,
                                        vector<BaseQueue *>(NCORE)));

    pipes_nc_nborder.resize(
            number_datacenters,
            vector<vector<Pipe *>>(NCORE,
                                   vector<Pipe *>(number_border_switches)));
    queues_nc_nborder.resize(
            number_datacenters,
            vector<vector<BaseQueue *>>(
                    NCORE, vector<BaseQueue *>(number_border_switches)));
}

BaseQueue *FatTreeInterDCTopology::alloc_src_queue(QueueLogger *queueLogger) {
    switch (_sender_qt) {
    case SWIFT_SCHEDULER:
        return new FairScheduler(_linkspeed * 1, *_eventlist, queueLogger);
    case PRIORITY:
        return new PriorityQueue(_linkspeed * 1, memFromPkt(FEEDER_BUFFER),
                                 *_eventlist, queueLogger);
    case FAIR_PRIO:
        return new FairPriorityQueue(_linkspeed * 1, memFromPkt(FEEDER_BUFFER),
                                     *_eventlist, queueLogger);
    default:
        abort();
    }
}

BaseQueue *FatTreeInterDCTopology::alloc_queue(QueueLogger *queueLogger,
                                               mem_b queuesize,
                                               link_direction dir,
                                               bool tor = false) {
    return alloc_queue(queueLogger, _linkspeed, queuesize, dir, tor);
}

BaseQueue *FatTreeInterDCTopology::alloc_queue(QueueLogger *queueLogger,
                                               linkspeed_bps speed,
                                               mem_b queuesize,
                                               link_direction dir, bool tor,
                                               bool is_failing) {
    switch (_qt) {
    case RANDOM:
        return new RandomQueue(speed, queuesize, *_eventlist, queueLogger,
                               memFromPkt(RANDOM_BUFFER));
    case COMPOSITE: {
        CompositeQueue *q =
                new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);

        if (kmin != -1) {
            q->set_ecn_thresholds((kmin / 100.0) * queuesize,
                                  (kmax / 100.0) * queuesize);
        }
        q->failed_link = is_failing;
        return q;
    }
    case COMPOSITE_BTS: {
        CompositeQueueBts *q = new CompositeQueueBts(speed, queuesize,
                                                     *_eventlist, queueLogger);

        if (kmin != -1) {
            q->set_ecn_thresholds((kmin / 100.0) * queuesize,
                                  (kmax / 100.0) * queuesize);
        }
        if (bts_trigger != -1) {
            q->set_bts_threshold((bts_trigger / 100.0) * queuesize);
        }
        q->set_ignore_ecn_data(bts_ignore_data);
        return q;
    }
    case CTRL_PRIO:
        return new CtrlPrioQueue(speed, queuesize, *_eventlist, queueLogger);
    case ECN:
        return new ECNQueue(speed, queuesize, *_eventlist, queueLogger,
                            memFromPkt(15));
    case LOSSLESS:
        return new LosslessQueue(speed, queuesize, *_eventlist, queueLogger,
                                 NULL);
    case LOSSLESS_INPUT: {
        LosslessOutputQueue *q = new LosslessOutputQueue(
                speed, queuesize, *_eventlist, queueLogger);
        if (kmin != -1) {
            q->set_ecn_thresholds((kmin / 100.0) * queuesize,
                                  (kmax / 100.0) * queuesize);
        }
        printf("Creaing Lossless queue");
        return q;
    }
    case LOSSLESS_INPUT_ECN:
        return new LosslessOutputQueue(speed, memFromPkt(10000), *_eventlist,
                                       queueLogger, 1, memFromPkt(16));
    case COMPOSITE_ECN:
        if (tor)
            return new CompositeQueue(speed, queuesize, *_eventlist,
                                      queueLogger);
        else
            return new ECNQueue(speed, memFromPkt(2 * SWITCH_BUFFER),
                                *_eventlist, queueLogger, memFromPkt(15));
    case COMPOSITE_ECN_LB: {
        CompositeQueue *q =
                new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
        if (!tor || dir == UPLINK) {
            // don't use ECN on ToR downlinks
            q->set_ecn_threshold(FatTreeInterDCSwitch::_ecn_threshold_fraction *
                                 queuesize);
        }
        return q;
    }
    default:
        abort();
    }
}

BaseQueue *FatTreeInterDCTopology::alloc_queue(QueueLogger *queueLogger,
                                               linkspeed_bps speed,
                                               mem_b queuesize,
                                               link_direction dir, bool tor) {
    switch (_qt) {
    case RANDOM:
        return new RandomQueue(speed, queuesize, *_eventlist, queueLogger,
                               memFromPkt(RANDOM_BUFFER));
    case COMPOSITE: {
        CompositeQueue *q =
                new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);

        if (kmin != -1) {
            q->set_ecn_thresholds((kmin / 100.0) * queuesize,
                                  (kmax / 100.0) * queuesize);
        }
        return q;
    }
    case COMPOSITE_BTS: {
        CompositeQueueBts *q = new CompositeQueueBts(speed, queuesize,
                                                     *_eventlist, queueLogger);

        if (kmin != -1) {
            q->set_ecn_thresholds((kmin / 100.0) * queuesize,
                                  (kmax / 100.0) * queuesize);
        }
        if (bts_trigger != -1) {
            q->set_bts_threshold((bts_trigger / 100.0) * queuesize);
        }
        q->set_ignore_ecn_data(bts_ignore_data);
        return q;
    }
    case CTRL_PRIO:
        return new CtrlPrioQueue(speed, queuesize, *_eventlist, queueLogger);
    case ECN:
        return new ECNQueue(speed, queuesize, *_eventlist, queueLogger,
                            memFromPkt(15));
    case LOSSLESS:
        return new LosslessQueue(speed, queuesize, *_eventlist, queueLogger,
                                 NULL);
    case LOSSLESS_INPUT: {
        LosslessOutputQueue *q = new LosslessOutputQueue(
                speed, queuesize, *_eventlist, queueLogger);
        if (kmin != -1) {
            q->set_ecn_thresholds((kmin / 100.0) * queuesize,
                                  (kmax / 100.0) * queuesize);
        }
        printf("Creaing Lossless queue");
        return q;
    }
    case LOSSLESS_INPUT_ECN:
        return new LosslessOutputQueue(speed, memFromPkt(10000), *_eventlist,
                                       queueLogger, 1, memFromPkt(16));
    case COMPOSITE_ECN:
        if (tor)
            return new CompositeQueue(speed, queuesize, *_eventlist,
                                      queueLogger);
        else
            return new ECNQueue(speed, memFromPkt(2 * SWITCH_BUFFER),
                                *_eventlist, queueLogger, memFromPkt(15));
    case COMPOSITE_ECN_LB: {
        CompositeQueue *q =
                new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);
        if (!tor || dir == UPLINK) {
            // don't use ECN on ToR downlinks
            q->set_ecn_threshold(FatTreeInterDCSwitch::_ecn_threshold_fraction *
                                 queuesize);
        }
        return q;
    }
    default:
        abort();
    }
}

void FatTreeInterDCTopology::init_network() {
    QueueLogger *queueLogger;

    if (_tiers == 3) {
        for (int i = 0; i < number_datacenters; i++) {
            for (uint32_t j = 0; j < NCORE; j++) {
                for (uint32_t k = 0; k < NAGG; k++) {
                    queues_nc_nup[i][j][k] = NULL;
                    pipes_nc_nup[i][j][k] = NULL;
                    queues_nup_nc[i][k][j] = NULL;
                    pipes_nup_nc[i][k][j] = NULL;
                }
            }
        }
    }

    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t j = 0; j < number_border_switches; j++) {
            for (uint32_t k = 0; k < NCORE; k++) {
                queues_nborder_nc[i][j][k] = NULL;
                pipes_nborder_nc[i][j][k] = NULL;
                queues_nc_nborder[i][k][j] = NULL;
                pipes_nc_nborder[i][k][j] = NULL;
            }
        }
    }

    for (uint32_t j = 0; j < number_border_switches; j++) {
        for (uint32_t k = 0; k < number_border_switches; k++) {
            queues_nborderl_nborderu[j][k] = NULL;
            pipes_nborderl_nborderu[j][k] = NULL;
            queues_nborderu_nborderl[k][j] = NULL;
            pipes_nborderu_nborderl[k][j] = NULL;
        }
    }

    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t j = 0; j < NAGG; j++) {
            for (uint32_t k = 0; k < NTOR; k++) {
                queues_nup_nlp[i][j][k] = NULL;
                pipes_nup_nlp[i][j][k] = NULL;
                queues_nlp_nup[i][k][j] = NULL;
                pipes_nlp_nup[i][k][j] = NULL;
            }
        }
    }

    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t j = 0; j < NTOR; j++) {
            for (uint32_t k = 0; k < NSRV; k++) {
                queues_nlp_ns[i][j][k] = NULL;
                pipes_nlp_ns[i][j][k] = NULL;
                queues_ns_nlp[i][k][j] = NULL;
                pipes_ns_nlp[i][k][j] = NULL;
            }
        }
    }

    // create switches if we have lossless operation
    // if (_qt==LOSSLESS)
    //  changed to always create switches
    for (int i = 0; i < number_datacenters; i++) {
        cout << "total switches: ToR " << NTOR << " NAGG " << NAGG << " NCORE "
             << NCORE << " srv_per_tor " << K / 2 << endl;
        for (uint32_t j = 0; j < NTOR; j++) {
            switches_lp[i][j] = new FatTreeInterDCSwitch(
                    *_eventlist, "Switch_LowerPod_" + ntoa(j),
                    FatTreeInterDCSwitch::TOR, j, _switch_latency, this, i);
        }
        for (uint32_t j = 0; j < NAGG; j++) {
            switches_up[i][j] = new FatTreeInterDCSwitch(
                    *_eventlist, "Switch_UpperPod_" + ntoa(j),
                    FatTreeInterDCSwitch::AGG, j, _switch_latency, this, i);
        }
        for (uint32_t j = 0; j < NCORE; j++) {
            switches_c[i][j] = new FatTreeInterDCSwitch(
                    *_eventlist, "Switch_Core_" + ntoa(j),
                    FatTreeInterDCSwitch::CORE, j, _switch_latency, this, i);
        }
        for (uint32_t j = 0; j < number_border_switches; j++) {
            switches_border[i][j] = new FatTreeInterDCSwitch(
                    *_eventlist, "Switch_Border_" + ntoa(j),
                    FatTreeInterDCSwitch::BORDER, j, _switch_latency, this, i);
        }
    }

    // links from lower layer pod switch to server
    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t tor = 0; tor < NTOR; tor++) {
            for (uint32_t l = 0; l < K / 2; l++) {
                uint32_t srv = tor * K / 2 + l;
                // Downlink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }

                queues_nlp_ns[i][tor][srv] =
                        alloc_queue(queueLogger, _queuesize, DOWNLINK, true);
                queues_nlp_ns[i][tor][srv]->setName("DC" + ntoa(i) + "-LS" +
                                                    ntoa(tor) + "->DST" +
                                                    ntoa(srv));
                // if (logfile) logfile->writeName(*(queues_nlp_ns[tor][srv]));

                // pipes_nlp_ns[tor][srv] = new Pipe(70000000, *_eventlist);
                pipes_nlp_ns[i][tor][srv] = new Pipe(_hop_latency, *_eventlist);

                pipes_nlp_ns[i][tor][srv]->setName("DC" + ntoa(i) + "-Pipe-LS" +
                                                   ntoa(tor) + "->DST" +
                                                   ntoa(srv));
                // if (logfile) logfile->writeName(*(pipes_nlp_ns[tor][srv]));

                // Uplink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
                queues_ns_nlp[i][srv][tor] = alloc_src_queue(queueLogger);
                queues_ns_nlp[i][srv][tor]->setName("DC" + ntoa(i) + "-SRC" +
                                                    ntoa(srv) + "->LS" +
                                                    ntoa(tor));
                // cout << queues_ns_nlp[srv][tor]->str() << endl;
                //  if (logfile) logfile->writeName(*(queues_ns_nlp[srv][tor]));

                queues_ns_nlp[i][srv][tor]->setRemoteEndpoint(
                        switches_lp[i][tor]);

                switches_lp[i][tor]->addPort(queues_nlp_ns[i][tor][srv]);

                /*if (_qt==LOSSLESS){
                  ((LosslessQueue*)queues_nlp_ns[tor][srv])->setRemoteEndpoint(queues_ns_nlp[srv][tor]);
                  }else */
                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                    // no virtual queue needed at server
                    new LosslessInputQueue(*_eventlist,
                                           queues_ns_nlp[i][srv][tor],
                                           switches_lp[i][tor]);
                }

                pipes_ns_nlp[i][srv][tor] = new Pipe(_hop_latency, *_eventlist);
                // pipes_ns_nlp[srv][tor] = new Pipe(70000000, *_eventlist);

                pipes_ns_nlp[i][srv][tor]->setName("DC" + ntoa(i) +
                                                   "-Pipe-SRC" + ntoa(srv) +
                                                   "->LS" + ntoa(tor));
                // if (logfile) logfile->writeName(*(pipes_ns_nlp[srv][tor]));

                if (ff) {
                    ff->add_queue(queues_nlp_ns[i][tor][srv]);
                    ff->add_queue(queues_ns_nlp[i][srv][tor]);
                }
            }
        }
    }

    for (int i = 0; i < number_datacenters; i++) {
        // Lower layer in pod to upper layer in pod!
        for (uint32_t tor = 0; tor < NTOR; tor++) {
            uint32_t podid = 2 * tor / K;
            uint32_t agg_min, agg_max;
            if (_tiers == 3) {
                // Connect the lower layer switch to the upper layer switches in
                // the same pod
                agg_min = MIN_POD_ID(podid);
                agg_max = MAX_POD_ID(podid);
            } else {
                // Connect the lower layer switch to all upper layer switches
                assert(_tiers == 2);
                agg_min = 0;
                agg_max = NAGG - 1;
            }
            // uint32_t uplink_numbers_tor_to_agg = (K / 2) / _os;
            for (uint32_t agg = agg_min; agg <= agg_max; agg++) {
                // Downlink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }

                queues_nup_nlp[i][agg][tor] = alloc_queue(
                        queueLogger, _linkspeed / _os_ratio_stage_1,
                        _queuesize / _os_ratio_stage_1, DOWNLINK, false);

                queues_nup_nlp[i][agg][tor]->setName("DC" + ntoa(i) + "-US" +
                                                     ntoa(agg) + "->LS_" +
                                                     ntoa(tor));
                // if (logfile) logfile->writeName(*(queues_nup_nlp[agg][tor]));

                pipes_nup_nlp[i][agg][tor] =
                        new Pipe(_hop_latency, *_eventlist);
                pipes_nup_nlp[i][agg][tor]->setName("DC" + ntoa(i) +
                                                    "-Pipe-US" + ntoa(agg) +
                                                    "->LS" + ntoa(tor));
                // if (logfile) logfile->writeName(*(pipes_nup_nlp[agg][tor]));

                // Uplink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
                queues_nlp_nup[i][tor][agg] = alloc_queue(
                        queueLogger, _linkspeed / _os_ratio_stage_1,
                        _queuesize / _os_ratio_stage_1, UPLINK, true);
                queues_nlp_nup[i][tor][agg]->setName("DC" + ntoa(i) + "-LS" +
                                                     ntoa(tor) + "->US" +
                                                     ntoa(agg));
                // cout << queues_nlp_nup[tor][agg]->str() << endl;
                //  if (logfile)
                //  logfile->writeName(*(queues_nlp_nup[tor][agg]));

                switches_lp[i][tor]->addPort(queues_nlp_nup[i][tor][agg]);
                switches_up[i][agg]->addPort(queues_nup_nlp[i][agg][tor]);
                queues_nlp_nup[i][tor][agg]->setRemoteEndpoint(
                        switches_up[i][agg]);
                queues_nup_nlp[i][agg][tor]->setRemoteEndpoint(
                        switches_lp[i][tor]);

                /*if (_qt==LOSSLESS){
                  ((LosslessQueue*)queues_nlp_nup[tor][agg])->setRemoteEndpoint(queues_nup_nlp[agg][tor]);
                  ((LosslessQueue*)queues_nup_nlp[agg][tor])->setRemoteEndpoint(queues_nlp_nup[tor][agg]);
                  }else */
                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                    new LosslessInputQueue(*_eventlist,
                                           queues_nlp_nup[i][tor][agg],
                                           switches_up[i][agg]);
                    new LosslessInputQueue(*_eventlist,
                                           queues_nup_nlp[i][agg][tor],
                                           switches_lp[i][tor]);
                }

                pipes_nlp_nup[i][tor][agg] =
                        new Pipe(_hop_latency, *_eventlist);
                pipes_nlp_nup[i][tor][agg]->setName("DC" + ntoa(i) +
                                                    "-Pipe-LS" + ntoa(tor) +
                                                    "->US" + ntoa(agg));
                // if (logfile) logfile->writeName(*(pipes_nlp_nup[tor][agg]));

                if (ff) {
                    ff->add_queue(queues_nlp_nup[i][tor][agg]);
                    ff->add_queue(queues_nup_nlp[i][agg][tor]);
                }
            }
        }
    }

    // Upper layer in pod to core!
    if (_tiers == 3) {
        for (int i = 0; i < number_datacenters; i++) {
            for (uint32_t agg = 0; agg < NAGG; agg++) {
                uint32_t podpos = agg % (K / 2);
                uint32_t uplink_numbers = max((unsigned int)1, (K / 2) / _os);
                for (uint32_t l = 0; l < uplink_numbers; l++) {
                    uint32_t core = podpos * uplink_numbers + l;
                    // Downlink
                    if (_logger_factory) {
                        queueLogger = _logger_factory->createQueueLogger();
                    } else {
                        queueLogger = NULL;
                    }

                    /*queues_nup_nc[agg][core] =
                            alloc_queue(queueLogger, _queuesize, UPLINK);*/

                    if (curr_failed_link < num_failing_links) {
                        queues_nup_nc[i][agg][core] = alloc_queue(
                                queueLogger, _linkspeed / _os_ratio_stage_1,
                                _queuesize / _os_ratio_stage_1, UPLINK, false,
                                true);
                        curr_failed_link++;
                    } else {
                        queues_nup_nc[i][agg][core] = alloc_queue(
                                queueLogger, _linkspeed / _os_ratio_stage_1,
                                _queuesize / _os_ratio_stage_1, UPLINK, false,
                                false);
                    }

                    queues_nup_nc[i][agg][core]->setName("DC" + ntoa(i) +
                                                         "-US" + ntoa(agg) +
                                                         "->CS" + ntoa(core));
                    // cout << queues_nup_nc[agg][core]->str() << endl;
                    //  if (logfile)
                    //  logfile->writeName(*(queues_nup_nc[agg][core]));

                    pipes_nup_nc[i][agg][core] =
                            new Pipe(_hop_latency, *_eventlist);

                    pipes_nup_nc[i][agg][core]->setName("DC" + ntoa(i) +
                                                        "-Pipe-US" + ntoa(agg) +
                                                        "->CS" + ntoa(core));
                    // if (logfile)
                    // logfile->writeName(*(pipes_nup_nc[agg][core]));

                    // Uplink
                    if (_logger_factory) {
                        queueLogger = _logger_factory->createQueueLogger();
                    } else {
                        queueLogger = NULL;
                    }

                    if ((l + agg * K / 2) < failed_links) {
                        queues_nc_nup[i][core][agg] = alloc_queue(
                                queueLogger, 0, _queuesize, //_linkspeed/10
                                DOWNLINK, false);
                        cout << "Adding link failure for agg_sw " << ntoa(agg)
                             << " l " << ntoa(l) << endl;
                    } else {

                        queues_nc_nup[i][core][agg] = alloc_queue(
                                queueLogger, _linkspeed / _os_ratio_stage_1,
                                _queuesize / _os_ratio_stage_1, DOWNLINK,
                                false);
                    }

                    queues_nc_nup[i][core][agg]->setName("DC" + ntoa(i) +
                                                         "-CS" + ntoa(core) +
                                                         "->US" + ntoa(agg));

                    switches_up[i][agg]->addPort(queues_nup_nc[i][agg][core]);
                    switches_c[i][core]->addPort(queues_nc_nup[i][core][agg]);
                    queues_nup_nc[i][agg][core]->setRemoteEndpoint(
                            switches_c[i][core]);
                    queues_nc_nup[i][core][agg]->setRemoteEndpoint(
                            switches_up[i][agg]);

                    /*if (_qt==LOSSLESS){
                      ((LosslessQueue*)queues_nup_nc[agg][core])->setRemoteEndpoint(queues_nc_nup[core][agg]);
                      ((LosslessQueue*)queues_nc_nup[core][agg])->setRemoteEndpoint(queues_nup_nc[agg][core]);
                      }
                      else*/
                    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                        new LosslessInputQueue(*_eventlist,
                                               queues_nup_nc[i][agg][core],
                                               switches_c[i][core]);
                        new LosslessInputQueue(*_eventlist,
                                               queues_nc_nup[i][core][agg],
                                               switches_up[i][agg]);
                    }
                    // if (logfile)
                    // logfile->writeName(*(queues_nc_nup[core][agg]));

                    pipes_nc_nup[i][core][agg] =
                            new Pipe(_hop_latency, *_eventlist);

                    /*printf("Core %d - Agg %d - Latency %lu\n", core, agg,
                           _hop_latency);*/

                    pipes_nc_nup[i][core][agg]->setName(
                            "DC" + ntoa(i) + "-Pipe-CS" + ntoa(core) + "->US" +
                            ntoa(agg));
                    // if (logfile)
                    // logfile->writeName(*(pipes_nc_nup[core][agg]));

                    if (ff) {
                        ff->add_queue(queues_nup_nc[i][agg][core]);
                        ff->add_queue(queues_nc_nup[i][core][agg]);
                    }
                }
            }
        }
    }

    // Core to Border
    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t core = 0; core < NCORE; core++) {
            uint32_t uplink_numbers = number_border_switches;
            for (uint32_t border_sw = 0; border_sw < uplink_numbers;
                 border_sw++) {

                // UpLinks Queues and Pipes
                queues_nc_nborder[i][core][border_sw] = alloc_queue(
                        queueLogger, _linkspeed / _os_ratio_stage_1,
                        _queuesize / _os_ratio_stage_1, UPLINK, false, false);

                queues_nc_nborder[i][core][border_sw]->setName(
                        "DC" + ntoa(i) + "-CS" + ntoa(core) + "->BORDER" +
                        ntoa(border_sw));

                pipes_nc_nborder[i][core][border_sw] =
                        new Pipe(_hop_latency, *_eventlist);

                pipes_nc_nborder[i][core][border_sw]->setName(
                        "DC" + ntoa(i) + "-Pipe-CS" + ntoa(core) + "->BORDER" +
                        ntoa(border_sw));

                // DownLinks Queues and Pipes
                queues_nborder_nc[i][border_sw][core] = alloc_queue(
                        queueLogger, _linkspeed / _os_ratio_stage_1,
                        _queuesize / _os_ratio_stage_1, DOWNLINK, false);

                queues_nborder_nc[i][border_sw][core]->setName(
                        "DC" + ntoa(i) + "-BORDER" + ntoa(border_sw) + "->CS" +
                        ntoa(core));

                pipes_nborder_nc[i][border_sw][core] =
                        new Pipe(_hop_latency, *_eventlist);

                pipes_nborder_nc[i][border_sw][core]->setName(
                        "DC" + ntoa(i) + "-Pipe-BORDER" + ntoa(border_sw) +
                        "->CS" + ntoa(core));

                // Add Ports to switches
                switches_c[i][core]->addPort(
                        queues_nc_nborder[i][core][border_sw]);
                switches_border[i][border_sw]->addPort(
                        queues_nborder_nc[i][border_sw][core]);

                // Add Remote Endpoints
                queues_nc_nborder[i][core][border_sw]->setRemoteEndpoint(
                        switches_border[i][border_sw]);
                queues_nborder_nc[i][border_sw][core]->setRemoteEndpoint(
                        switches_c[i][core]);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                    printf("Not Supported Yet!\n");
                    exit(0);
                    /*
                    new LosslessInputQueue(*_eventlist,
                    queues_nup_nc[agg][core], switches_c[core]); new
                    LosslessInputQueue(*_eventlist, queues_nc_nup[core][agg],
                                           switches_up[agg]);
                    */
                }

                if (ff) {
                    ff->add_queue(queues_nc_nborder[i][core][border_sw]);
                    ff->add_queue(queues_nborder_nc[i][border_sw][core]);
                }
            }
        }
    }

    // Between border switches
    for (uint32_t border_l = 0; border_l < number_border_switches; border_l++) {
        for (uint32_t border_u = 0; border_u < number_border_switches;
             border_u++) {

            // UpLinks Queues and Pipes
            queues_nborderl_nborderu[border_l][border_u] = alloc_queue(
                    queueLogger, _linkspeed / _os_ratio_stage_1,
                    _queuesize / _os_ratio_stage_1, UPLINK, false, false);

            queues_nborderl_nborderu[border_l][border_u]->setName(
                    "DC" + ntoa(0) + "-BORDER" + ntoa(border_l) + "->BORDER" +
                    ntoa(border_u));

            pipes_nborderl_nborderu[border_l][border_u] =
                    new Pipe(_hop_latency, *_eventlist);

            pipes_nborderl_nborderu[border_l][border_u]->setName(
                    "DC" + ntoa(0) + "-Pipe-BORDER" + ntoa(border_l) +
                    "->BORDER" + ntoa(border_u));

            // DownLinks Queues and Pipes
            queues_nborderu_nborderl[border_u][border_l] = alloc_queue(
                    queueLogger, _linkspeed / _os_ratio_stage_1,
                    _queuesize / _os_ratio_stage_1, DOWNLINK, false);

            queues_nborderu_nborderl[border_u][border_l]->setName(
                    "DC" + ntoa(0) + "-BORDER" + ntoa(border_u) + "->BORDER" +
                    ntoa(border_l));

            pipes_nborderu_nborderl[border_u][border_l] =
                    new Pipe(_hop_latency, *_eventlist);

            pipes_nborderu_nborderl[border_u][border_l]->setName(
                    "DC" + ntoa(0) + "-Pipe-BORDER" + ntoa(border_u) +
                    "->BORDER" + ntoa(border_l));

            // Add Ports to switches
            switches_border[0][border_l]->addPort(
                    queues_nborderl_nborderu[border_l][border_u]);
            switches_border[1][border_u]->addPort(
                    queues_nborderu_nborderl[border_u][border_l]);

            // Add Remote Endpoints
            queues_nborderl_nborderu[border_l][border_u]->setRemoteEndpoint(
                    switches_border[1][border_u]);
            queues_nborderu_nborderl[border_u][border_l]->setRemoteEndpoint(
                    switches_border[0][border_l]);

            if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                printf("Not Supported Yet!\n");
                exit(0);
            }

            if (ff) {
                ff->add_queue(queues_nborderl_nborderu[border_l][border_u]);
                ff->add_queue(queues_nborderu_nborderl[border_u][border_l]);
            }
        }
    }
}

void FatTreeInterDCTopology::count_queue(Queue *queue) {
    if (_link_usage.find(queue) == _link_usage.end()) {
        _link_usage[queue] = 0;
    }

    _link_usage[queue] = _link_usage[queue] + 1;
}

int FatTreeInterDCTopology::get_dc_id(int node) {
    if (node < NSRV) {
        return 0;
    } else {
        return 1;
    }
}

vector<const Route *> *FatTreeInterDCTopology::get_bidir_paths(uint32_t src,
                                                               uint32_t dest,
                                                               bool reverse) {
    return NULL;
}

/*
vector<const Route *> *FatTreeInterDCTopology::get_bidir_paths(uint32_t src,
                                                               uint32_t dest,
                                                               bool reverse) {
    vector<const Route *> *paths = new vector<const Route *>();

    route_t *routeout, *routeback;

    if (HOST_POD_SWITCH(src) == HOST_POD_SWITCH(dest)) {

        // forward path
        routeout = new Route();
        // routeout->push_back(pqueue);
        routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
        routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

        if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
            routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]
                                        ->getRemoteEndpoint());

        routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
        routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

        if (reverse) {
            // reverse path for RTS packets
            routeback = new Route();
            routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]);
            routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)]);

            if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]
                                             ->getRemoteEndpoint());

            routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src]);
            routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src]);

            routeout->set_reverse(routeback);
            routeback->set_reverse(routeout);
        }

        // print_route(*routeout);
        paths->push_back(routeout);

        check_non_null(routeout);
        return paths;
    } else if (HOST_POD(src) == HOST_POD(dest)) {
        // don't go up the hierarchy, stay in the pod only.

        uint32_t pod = HOST_POD(src);
        // there are K/2 paths between the source and the destination
        if (_tiers == 2) {
            // xxx sanity check for debugging, remove later.
            assert(MIN_POD_ID(pod) == 0);
            assert(MAX_POD_ID(pod) == NAGG - 1);
        }
        for (uint32_t upper = MIN_POD_ID(pod); upper <= MAX_POD_ID(pod);
             upper++) {
            // upper is nup

            routeout = new Route();
            // routeout->push_back(pqueue);

            routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
            routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);

            if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]
                                            ->getRemoteEndpoint());

            routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);
            routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);

            if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_nlp_nup[HOST_POD_SWITCH(src)][upper]
                                            ->getRemoteEndpoint());

            routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)]);
            routeout->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(dest)]);

            if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                routeout->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(dest)]
                                            ->getRemoteEndpoint());

            routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
            routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);

            if (reverse) {
                // reverse path for RTS packets
                routeback = new Route();

                routeback->push_back(
                        queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]);
                routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)]);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                    routeback->push_back(
                            queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]
                                    ->getRemoteEndpoint());

                routeback->push_back(
                        queues_nlp_nup[HOST_POD_SWITCH(dest)][upper]);
                routeback->push_back(
                        pipes_nlp_nup[HOST_POD_SWITCH(dest)][upper]);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                    routeback->push_back(
                            queues_nlp_nup[HOST_POD_SWITCH(dest)][upper]
                                    ->getRemoteEndpoint());

                routeback->push_back(
                        queues_nup_nlp[upper][HOST_POD_SWITCH(src)]);
                routeback->push_back(
                        pipes_nup_nlp[upper][HOST_POD_SWITCH(src)]);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                    routeback->push_back(
                            queues_nup_nlp[upper][HOST_POD_SWITCH(src)]
                                    ->getRemoteEndpoint());

                routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src]);
                routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src]);

                routeout->set_reverse(routeback);
                routeback->set_reverse(routeout);
            }

            // print_route(*routeout);
            paths->push_back(routeout);
            check_non_null(routeout);
        }
        return paths;
    } else {
        assert(_tiers == 3);
        uint32_t pod = HOST_POD(src);

        for (uint32_t upper = MIN_POD_ID(pod); upper <= MAX_POD_ID(pod);
             upper++)
            for (uint32_t core = (upper % (K / 2)) * K / 2;
                 core < (((upper % (K / 2)) + 1) * K / 2) / _os; core++) {
                // upper is nup

                routeout = new Route();
                routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]);
                routeout->push_back(pipes_ns_nlp[src][HOST_POD_SWITCH(src)]);
                check_non_null(routeout);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                    routeout->push_back(queues_ns_nlp[src][HOST_POD_SWITCH(src)]
                                                ->getRemoteEndpoint());

                routeout->push_back(
                        queues_nlp_nup[HOST_POD_SWITCH(src)][upper]);
                routeout->push_back(pipes_nlp_nup[HOST_POD_SWITCH(src)][upper]);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                    routeout->push_back(
                            queues_nlp_nup[HOST_POD_SWITCH(src)][upper]
                                    ->getRemoteEndpoint());

                routeout->push_back(queues_nup_nc[upper][core]);
                routeout->push_back(pipes_nup_nc[upper][core]);
                check_non_null(routeout);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
                    routeout->push_back(
                            queues_nup_nc[upper][core]->getRemoteEndpoint());

                // now take the only link down to the destination server!

                uint32_t upper2 = (HOST_POD(dest) * K / 2 + 2 * core / K);


routeout->push_back(queues_nc_nup[core][upper2]);
routeout->push_back(pipes_nc_nup[core][upper2]);
check_non_null(routeout);

if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
    routeout->push_back(queues_nc_nup[core][upper2]->getRemoteEndpoint());

routeout->push_back(queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);
routeout->push_back(pipes_nup_nlp[upper2][HOST_POD_SWITCH(dest)]);
check_non_null(routeout);

if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
    routeout->push_back(
            queues_nup_nlp[upper2][HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

routeout->push_back(queues_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
routeout->push_back(pipes_nlp_ns[HOST_POD_SWITCH(dest)][dest]);
check_non_null(routeout);

if (reverse) {
    // reverse path for RTS packets
    routeback = new Route();

    routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]);
    routeback->push_back(pipes_ns_nlp[dest][HOST_POD_SWITCH(dest)]);

    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
        routeback->push_back(queues_ns_nlp[dest][HOST_POD_SWITCH(dest)]
                                     ->getRemoteEndpoint());

    routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper2]);
    routeback->push_back(pipes_nlp_nup[HOST_POD_SWITCH(dest)][upper2]);

    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
        routeback->push_back(queues_nlp_nup[HOST_POD_SWITCH(dest)][upper2]
                                     ->getRemoteEndpoint());

    routeback->push_back(queues_nup_nc[upper2][core]);
    routeback->push_back(pipes_nup_nc[upper2][core]);

    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
        routeback->push_back(queues_nup_nc[upper2][core]->getRemoteEndpoint());

    // now take the only link back down to the src server!

    routeback->push_back(queues_nc_nup[core][upper]);
    routeback->push_back(pipes_nc_nup[core][upper]);

    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
        routeback->push_back(queues_nc_nup[core][upper]->getRemoteEndpoint());

    routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)]);
    routeback->push_back(pipes_nup_nlp[upper][HOST_POD_SWITCH(src)]);

    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN)
        routeback->push_back(queues_nup_nlp[upper][HOST_POD_SWITCH(src)]
                                     ->getRemoteEndpoint());

    routeback->push_back(queues_nlp_ns[HOST_POD_SWITCH(src)][src]);
    routeback->push_back(pipes_nlp_ns[HOST_POD_SWITCH(src)][src]);

    routeout->set_reverse(routeback);
    routeback->set_reverse(routeout);
}

paths->push_back(routeout);
check_non_null(routeout);
}
return paths;
}
}

int64_t FatTreeInterDCTopology::find_lp_switch(Queue *queue) {
    // first check ns_nlp
    for (uint32_t srv = 0; srv < NSRV; srv++)
        for (uint32_t tor = 0; tor < NTOR; tor++)
            if (queues_ns_nlp[srv][tor] == queue)
                return tor;

    // only count nup to nlp
    count_queue(queue);

    for (uint32_t agg = 0; agg < NAGG; agg++)
        for (uint32_t tor = 0; tor < NTOR; tor++)
            if (queues_nup_nlp[agg][tor] == queue)
                return tor;

    return -1;
}

int64_t FatTreeInterDCTopology::find_up_switch(Queue *queue) {
    count_queue(queue);
    // first check nc_nup
    for (uint32_t core = 0; core < NCORE; core++)
        for (uint32_t agg = 0; agg < NAGG; agg++)
            if (queues_nc_nup[core][agg] == queue)
                return agg;

    // check nlp_nup
    for (uint32_t tor = 0; tor < NTOR; tor++)
        for (uint32_t agg = 0; agg < NAGG; agg++)
            if (queues_nlp_nup[tor][agg] == queue)
                return agg;

    return -1;
}

int64_t FatTreeInterDCTopology::find_core_switch(Queue *queue) {
    count_queue(queue);
    // first check nup_nc
    for (uint32_t agg = 0; agg < NAGG; agg++)
        for (uint32_t core = 0; core < NCORE; core++)
            if (queues_nup_nc[agg][core] == queue)
                return core;

    return -1;
}

int64_t FatTreeInterDCTopology::find_destination(Queue *queue) {
    // first check nlp_ns
    for (uint32_t tor = 0; tor < NTOR; tor++)
        for (uint32_t srv = 0; srv < NSRV; srv++)
            if (queues_nlp_ns[tor][srv] == queue)
                return srv;

    return -1;
}

void FatTreeInterDCTopology::print_path(std::ofstream &paths, uint32_t src,
                                        const Route *route) {
    paths << "SRC_" << src << " ";

    if (route->size() / 2 == 2) {
        paths << "LS_" << find_lp_switch((Queue *)route->at(1)) << " ";
        paths << "DST_" << find_destination((Queue *)route->at(3)) << " ";
    } else if (route->size() / 2 == 4) {
        paths << "LS_" << find_lp_switch((Queue *)route->at(1)) << " ";
        paths << "US_" << find_up_switch((Queue *)route->at(3)) << " ";
        paths << "LS_" << find_lp_switch((Queue *)route->at(5)) << " ";
        paths << "DST_" << find_destination((Queue *)route->at(7)) << " ";
    } else if (route->size() / 2 == 6) {
        paths << "LS_" << find_lp_switch((Queue *)route->at(1)) << " ";
        paths << "US_" << find_up_switch((Queue *)route->at(3)) << " ";
        paths << "CS_" << find_core_switch((Queue *)route->at(5)) << " ";
        paths << "US_" << find_up_switch((Queue *)route->at(7)) << " ";
        paths << "LS_" << find_lp_switch((Queue *)route->at(9)) << " ";
        paths << "DST_" << find_destination((Queue *)route->at(11)) << " ";
    } else {
        paths << "Wrong hop count " << ntoa(route->size() / 2);
    }

    paths << endl;
}*/

void FatTreeInterDCTopology::add_switch_loggers(Logfile &log,
                                                simtime_picosec sample_period) {
    return;
}
