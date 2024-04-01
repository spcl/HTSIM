// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "config.h"
#include "network.h"
#include "randomqueue.h"
#include <iostream>
#include <list>
#include <math.h>
#include <sstream>
#include <string.h>
// #include "subflow_control.h"
#include "fat_tree_interdc_topology.h"

#include "clock.h"
#include "compositequeue.h"
#include "connection_matrix.h"
#include "eventlist.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
#include "logsim-interface.h"
#include "ndp.h"
#include "pipe.h"
#include "shortflows.h"
#include "topology.h"
// #include "vl2_topology.h"

#include "fat_tree_topology.h"
// #include "oversubscribed_fat_tree_topology.h"
// #include "multihomed_fat_tree_topology.h"
// #include "star_topology.h"
// #include "bcube_topology.h"
#include <list>

// Simulation params

#define PRINT_PATHS 1

#define PERIODIC 0
#include "main.h"

// int RTT = 10; // this is per link delay; identical RTT microseconds = 0.02 ms
uint32_t RTT = 400; // this is per link delay in us; identical RTT microseconds
                    // = 0.02 ms
int DEFAULT_NODES = 432;
#define DEFAULT_QUEUE_SIZE 8

FirstFit *ff = NULL;
unsigned int subflow_count = 1;

string ntoa(double n);
string itoa(uint64_t n);

// #define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

Logfile *lg;

void exit_error(char *progr) {
    cout << "Usage " << progr
         << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] "
            "[epsilon][COUPLED_SCALABLE_TCP"
         << endl;
    exit(1);
}

void print_path(std::ofstream &paths, const Route *rt) {
    for (unsigned int i = 0; i < rt->size(); i += 1) {
        RandomQueue *q = (RandomQueue *)rt->at(i);
        if (q != NULL)
            paths << q->str() << " ";
        else
            paths << "NULL ";
    }

    paths << endl;
}

int main(int argc, char **argv) {
    Packet::set_packet_size(PKT_SIZE_MODERN);
    eventlist.setEndtime(timeFromSec(10));
    Clock c(timeFromSec(5 / 100.), eventlist);
    mem_b queuesize = INFINITE_BUFFER_SIZE;
    int no_of_conns = 0, cwnd = 40, no_of_nodes = DEFAULT_NODES;
    stringstream filename(ios_base::out);
    RouteStrategy route_strategy = ECMP_FIB;
    std::string goal_filename;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    simtime_picosec hop_latency = timeFromNs((uint32_t)RTT);
    simtime_picosec switch_latency = timeFromNs((uint32_t)0);
    int packet_size = 2048;
    int seed = -1;
    int number_entropies = 256;
    int fat_tree_k = 1; // 1:1 default
    bool collect_data = false;
    COLLECT_DATA = collect_data;
    int kmin = -1;
    int kmax = -1;
    int ratio_os_stage_1 = 1;
    int flowsize = -1;
    bool topology_normal = true;
    char *topo_file = NULL;
    char *tm_file = NULL;
    uint64_t interdc_delay = 0;
    uint64_t max_queue_size = 0;

    int i = 1;
    filename << "logout.dat";

    while (i < argc) {
        if (!strcmp(argv[i], "-o")) {
            filename.str(std::string());
            filename << argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-sub")) {
            subflow_count = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-conns")) {
            no_of_conns = atoi(argv[i + 1]);
            cout << "no_of_conns " << no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i], "-nodes")) {
            no_of_nodes = atoi(argv[i + 1]);
            cout << "no_of_nodes " << no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i], "-goal")) {
            goal_filename = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-os_border")) {
            int os_b = atoi(argv[i + 1]);
            FatTreeInterDCTopology::set_os_ratio_border(os_b);
            i++;
        } else if (!strcmp(argv[i], "-interdc_delay")) {
            interdc_delay = atoi(argv[i + 1]);
            interdc_delay *= 1000;
            i++;
        } else if (!strcmp(argv[i], "-max_queue_size")) {
            max_queue_size = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-number_entropies")) {
            number_entropies = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-kmax")) {
            // kmin as percentage of queue size (0..100)
            kmax = atoi(argv[i + 1]);
            printf("KMax: %d\n", atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-topology")) {
            if (!strcmp(argv[i + 1], "normal")) {
                topology_normal = true;
            } else if (!strcmp(argv[i + 1], "interdc")) {
                topology_normal = false;
            }
            i++;
        } else if (!strcmp(argv[i], "-kmin")) {
            // kmin as percentage of queue size (0..100)
            kmin = atoi(argv[i + 1]);
            printf("KMin: %d\n", atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-cwnd")) {
            cwnd = atoi(argv[i + 1]);
            cout << "cwnd " << cwnd << endl;
            i++;
        } else if (!strcmp(argv[i], "-flowsize")) {
            flowsize = atoi(argv[i + 1]);
            cout << "flowsize " << flowsize << endl;
            i++;
        } else if (!strcmp(argv[i], "-topo")) {
            topo_file = argv[i + 1];
            cout << "FatTree topology input file: " << topo_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-ratio_os_stage_1")) {
            ratio_os_stage_1 = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-linkspeed")) {
            // linkspeed specified is in Mbps
            linkspeed = speedFromMbps(atof(argv[i + 1]));
            LINK_SPEED_MODERN = atoi(argv[i + 1]);
            printf("Speed is %lu\n", LINK_SPEED_MODERN);
            LINK_SPEED_MODERN = LINK_SPEED_MODERN / 1000;
            // Saving this for UEC reference, Gbps
            i++;
        } else if (!strcmp(argv[i], "-k")) {
            fat_tree_k = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-collect_data")) {
            collect_data = atoi(argv[i + 1]);
            COLLECT_DATA = collect_data;
            i++;
        } else if (!strcmp(argv[i], "-mtu")) {
            packet_size = atoi(argv[i + 1]);
            PKT_SIZE_MODERN = packet_size; // Saving this for UEC reference, Bytes
            i++;
        } else if (!strcmp(argv[i], "-switch_latency")) {
            switch_latency = timeFromNs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-hop_latency")) {
            hop_latency = timeFromNs(atof(argv[i + 1]));
            LINK_DELAY_MODERN = hop_latency / 1000; // Saving this for UEC reference, ps to ns
            i++;
        } else if (!strcmp(argv[i], "-tm")) {
            tm_file = argv[i + 1];
            cout << "traffic matrix input file: " << tm_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-seed")) {
            seed = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-q")) {
            queuesize = (atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-strat")) {
            if (!strcmp(argv[i + 1], "perm")) {
                route_strategy = SCATTER_PERMUTE;
            } else if (!strcmp(argv[i + 1], "rand")) {
                route_strategy = SCATTER_RANDOM;
            } else if (!strcmp(argv[i + 1], "pull")) {
                route_strategy = PULL_BASED;
            } else if (!strcmp(argv[i + 1], "single")) {
                route_strategy = SINGLE_PATH;
            } else if (!strcmp(argv[i + 1], "ecmp_host")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "ecmp_host_random_ecn")) {
                route_strategy = ECMP_RANDOM_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "ecmp_host_random2_ecn")) {
                route_strategy = ECMP_RANDOM2_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            }
            i++;
        } else
            exit_error(argv[0]);

        i++;
    }

    Packet::set_packet_size(packet_size);
    if (seed != -1) {
        srand(seed);
        srandom(seed);
    } else {
        srand(time(NULL));
        srandom(time(NULL));
    }
    initializeLoggingFolders();

    if (route_strategy == NOT_SET) {
        fprintf(stderr, "Route Strategy not set.  Use the -strat param.  "
                        "\nValid values are perm, rand, pull, rg and single\n");
        exit(1);
    }
    cout << "Using subflow count " << subflow_count << endl;

    // prepare the loggers

    cout << "Logging to " << filename.str() << endl;
    // Logfile
    Logfile logfile(filename.str(), eventlist);

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths) {
        cout << "Can't open for writing paths file!" << endl;
        exit(1);
    }
#endif

    int tot_subs = 0;
    int cnt_con = 0;

    lg = &logfile;

    logfile.setStartTime(timeFromSec(0));

    // NdpSinkLoggerSampling sinkLogger = NdpSinkLoggerSampling(timeFromMs(0.1),
    // eventlist); logfile.addLogger(sinkLogger);
    NdpTrafficLogger traffic_logger = NdpTrafficLogger();
    logfile.addLogger(traffic_logger);
    NdpSrc::setMinRTO(1000); // increase RTO to avoid spurious retransmits
    NdpSrc::setRouteStrategy(route_strategy);
    NdpSink::setRouteStrategy(route_strategy);

    if (max_queue_size != 0) {
        queuesize = max_queue_size;
    }

    // NdpSrc *ndpSrc;
    // NdpSink *ndpSnk;

    // Route *routeout, *routein;
    // double extrastarttime;

    // scanner interval must be less than min RTO
    NdpRtxTimerScanner ndpRtxScanner(timeFromUs((uint32_t)1000), eventlist);

    // int dest;
    printf("Name Running: NDP\n");
    fflush(stdout);
#if USE_FIRST_FIT
    if (subflow_count == 1) {
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL), eventlist);
    }
#endif

#ifdef FAT_TREE
#endif

#ifdef OV_FAT_TREE
    OversubscribedFatTreeTopology *top = new OversubscribedFatTreeTopology(&logfile, &eventlist, ff);
#endif

#ifdef MH_FAT_TREE
    MultihomedFatTreeTopology *top = new MultihomedFatTreeTopology(&logfile, &eventlist, ff);
#endif

#ifdef STAR
    StarTopology *top = new StarTopology(&logfile, &eventlist, ff);
#endif

#ifdef BCUBE
    BCubeTopology *top = new BCubeTopology(&logfile, &eventlist, ff);
    cout << "BCUBE " << K << endl;
#endif

#ifdef VL2
    VL2Topology *top = new VL2Topology(&logfile, &eventlist, ff);
#endif

#ifdef USE_FIRST_FIT
#endif

    // vector<int> *destinations;

    // Permutation connections
    // conns->setStride(no_of_conns);
    // conns->setStaggeredPermutation(top,(double)no_of_conns/100.0);
    // conns->setStaggeredRandom(top,512,1);
    // conns->setHotspot(no_of_conns,512/no_of_conns);
    // conns->setManytoMany(128);

    // conns->setVL2();

    // conns->setRandom(no_of_conns);

    // NdpPullPacer* pacer = new NdpPullPacer(eventlist,
    // "/Users/localadmin/poli/new-datacenter-protocol/data/1500.recv.cdf.pretty");

    map<int, vector<int> *>::iterator it;

    // used just to print out stats data at the end
    list<const Route *> routes;
    // int connID = 0;

    ConnectionMatrix *conns = NULL;
    LogSimInterface *lgs = NULL;

    if (tm_file != NULL) {

        eventlist.setEndtime(timeFromSec(1));

        FatTreeInterDCTopology *top_dc = NULL;
        FatTreeTopology *top = NULL;

        if (topology_normal) {
            printf("Normal Topology\n");
            FatTreeTopology::set_tiers(3);
            FatTreeTopology::set_os_stage_2(fat_tree_k);
            FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff, COMPOSITE, hop_latency,
                                      switch_latency);
        } else {
            if (interdc_delay != 0) {
                FatTreeInterDCTopology::set_interdc_delay(interdc_delay);
                NdpSrc::set_interdc_delay(interdc_delay);
            } else {
                FatTreeInterDCTopology::set_interdc_delay(hop_latency);
                NdpSrc::set_interdc_delay(hop_latency);
            }
            FatTreeInterDCTopology::set_tiers(3);
            FatTreeInterDCTopology::set_os_stage_2(fat_tree_k);
            FatTreeInterDCTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeInterDCTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            if (topo_file) {
                top_dc = FatTreeInterDCTopology::load(topo_file, NULL, eventlist, queuesize, COMPOSITE, FAIR_PRIO);
            } else {
                FatTreeInterDCTopology::set_tiers(3);
                top_dc = new FatTreeInterDCTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, NULL,
                                                    COMPOSITE, hop_latency, switch_latency, FAIR_PRIO);
            }
        }

        conns = new ConnectionMatrix(no_of_nodes);

        if (tm_file) {
            cout << "Loading connection matrix from  " << tm_file << endl;

            if (!conns->load(tm_file)) {
                cout << "Failed to load connection matrix " << tm_file << endl;
                exit(-1);
            }
        } else {
            cout << "Loading connection matrix from  standard input" << endl;
            conns->load(cin);
        }

        map<flowid_t, TriggerTarget *> flowmap;
        vector<connection *> *all_conns = conns->getAllConnections();
        vector<NdpSrc *> uec_srcs;
        NdpSrc *uecSrc;
        NdpSink *uecSnk;
        vector<NdpPullPacer *> pacers;
        for (int ix = 0; ix <= no_of_nodes * 2; ix++)
            pacers.push_back(new NdpPullPacer(eventlist, linkspeed, 0.99));

        for (size_t c = 0; c < all_conns->size(); c++) {
            connection *crt = all_conns->at(c);
            int src = crt->src;
            int dest = crt->dst;
            uint64_t rtt = BASE_RTT_MODERN * 1000;
            uint64_t bdp = BDP_MODERN_UEC;
            printf("Reaching here1\n");
            fflush(stdout);

            /* Route *myin = new Route(*top->get_paths(src, dest)->at(0));
            int hops = myin->hop_count(); // hardcoded for now */
            int hops = 6;

            uint64_t actual_starting_cwnd = 0;
            uint64_t base_rtt_max_hops = (hops * LINK_DELAY_MODERN) + (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * hops) +
                                         (hops * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * hops);
            uint64_t bdp_local = base_rtt_max_hops * LINK_SPEED_MODERN / 8;

            actual_starting_cwnd = bdp_local; // Equal to BDP if not other info

            printf("Setting CWND to %lu\n", cwnd);

            printf("Using BDP of %lu - Queue is %lld - Starting Window is %lu\n", bdp_local, queuesize, cwnd);

            uecSrc = new NdpSrc(NULL, NULL, eventlist);
            uecSrc->setCwnd(cwnd);

            // uecSrc->setNumberEntropies(256);
            uec_srcs.push_back(uecSrc);
            uecSrc->set_dst(dest);
            printf("Reaching here\n");
            if (crt->flowid) {
                uecSrc->set_flowid(crt->flowid);
                assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                flowmap[crt->flowid] = uecSrc;
            }

            if (crt->size > 0) {
                uecSrc->set_flowsize(crt->size);
            }

            if (crt->trigger) {
                Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                trig->add_target(*uecSrc);
            }
            if (crt->send_done_trigger) {
                Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                uecSrc->set_end_trigger(*trig);
            }

            uecSnk = new NdpSink(pacers[dest]);

            uecSrc->setName("uec_" + ntoa(src) + "_" + ntoa(dest));

            cout << "uec_" + ntoa(src) + "_" + ntoa(dest) << endl;
            logfile.writeName(*uecSrc);

            uecSnk->set_src(src);

            uecSnk->setName("uec_sink_" + ntoa(src) + "_" + ntoa(dest));
            logfile.writeName(*uecSnk);
            if (crt->recv_done_trigger) {
                Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                uecSnk->set_end_trigger(*trig);
            }

            // uecRtxScanner->registerUec(*uecSrc);

            switch (route_strategy) {
            case ECMP_FIB:
            case ECMP_FIB_ECN:
            case ECMP_RANDOM2_ECN:
            case REACTIVE_ECN: {
                Route *srctotor = new Route();
                Route *dsttotor = new Route();

                if (top != NULL) {
                    srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                    srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                    srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                    dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                    dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                    dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                } else if (top_dc != NULL) {
                    int idx_dc = top_dc->get_dc_id(src);
                    int idx_dc_to = top_dc->get_dc_id(dest);
                    uecSrc->src_dc = top_dc->get_dc_id(src);
                    uecSrc->dest_dc = top_dc->get_dc_id(dest);
                    uecSrc->updateParams();

                    printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                    srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                             [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                    srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                    srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                             [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                     ->getRemoteEndpoint());

                    dsttotor->push_back(
                            top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                 [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                    dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                    dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                             [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                     ->getRemoteEndpoint());
                }

                uecSrc->from = src;
                uecSrc->to = dest;
                uecSnk->to = dest;
                uecSrc->connect(srctotor, dsttotor, *uecSnk, crt->start);
                uecSrc->set_paths(number_entropies);
                uecSnk->set_paths(number_entropies);

                // register src and snk to receive packets src their respective
                // TORs.
                if (top != NULL) {
                    top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, uecSrc->flow_id(), uecSrc);
                    top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, uecSrc->flow_id(), uecSnk);
                } else {
                    int idx_dc = top_dc->get_dc_id(src);
                    int idx_dc_to = top_dc->get_dc_id(dest);

                    top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                            src % top_dc->no_of_nodes(), uecSrc->flow_id(), uecSrc);
                    top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                            dest % top_dc->no_of_nodes(), uecSrc->flow_id(), uecSnk);
                }
                break;
            }
            case NOT_SET: {
                abort();
                break;
            }
            default: {
                abort();
                break;
            }
            }
        }

        while (eventlist.doNextEvent()) {
        }

        for (std::size_t i = 0; i < uec_srcs.size(); ++i) {
            delete uec_srcs[i];
        }

    } else if (goal_filename.size() > 0) {
        printf("Starting LGS Interface");

        if (topology_normal) {
            printf("Normal Topology\n");
            FatTreeTopology::set_tiers(3);
            FatTreeTopology::set_os_stage_2(fat_tree_k);
            FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            FatTreeTopology *top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff,
                                                       COMPOSITE, hop_latency, switch_latency);
            lgs = new LogSimInterface(NULL, &traffic_logger, eventlist, top, NULL);
        } else {
            if (interdc_delay != 0) {
                FatTreeInterDCTopology::set_interdc_delay(interdc_delay);
                UecSrc::set_interdc_delay(interdc_delay);
            } else {
                FatTreeInterDCTopology::set_interdc_delay(hop_latency);
                UecSrc::set_interdc_delay(hop_latency);
            }
            FatTreeInterDCTopology::set_tiers(3);
            FatTreeInterDCTopology::set_os_stage_2(fat_tree_k);
            FatTreeInterDCTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeInterDCTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            FatTreeInterDCTopology *top = new FatTreeInterDCTopology(
                    no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff, COMPOSITE, hop_latency, switch_latency);
            lgs = new LogSimInterface(NULL, &traffic_logger, eventlist, top, NULL);
        }

        lgs->set_protocol(NDP_PROTOCOL);
        lgs->set_cwd(cwnd);
        lgs->set_queue_size(queuesize);
        lgs->setNumberPaths(number_entropies);
        start_lgs(goal_filename, *lgs);
    }

    cout << "Mean number of subflows " << ntoa((double)tot_subs / cnt_con) << endl;

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC * CORE_TO_HOST) + " pkt/sec");
    printf("Host Link Rate %d - Core Link Rate %d - Pkt Size %d", HOST_NIC, CORE_TO_HOST, pktsize);
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    cout << "Done" << endl;
    list<const Route *>::iterator rt_i;
    int counts[10];
    int hop;
    for (int i = 0; i < 10; i++)
        counts[i] = 0;
    for (rt_i = routes.begin(); rt_i != routes.end(); rt_i++) {
        const Route *r = (*rt_i);
#ifdef PRINTPATHS
        cout << "Path:" << endl;
#endif
        hop = 0;
        for (std::size_t i = 0; i < r->size(); i++) {
            PacketSink *ps = r->at(i);
            CompositeQueue *q = dynamic_cast<CompositeQueue *>(ps);
            if (q == 0) {
#ifdef PRINTPATHS
                cout << ps->nodename() << endl;
#endif
            } else {
#ifdef PRINTPATHS
                cout << q->nodename() << " id=" << q->id << " " << q->num_packets() << "pkts " << q->num_headers()
                     << "hdrs " << q->num_acks() << "acks " << q->num_nacks() << "nacks " << q->num_stripped()
                     << "stripped" << endl;
#endif
                counts[hop] += q->num_stripped();
                hop++;
            }
        }
#ifdef PRINTPATHS
        cout << endl;
#endif
    }
}

string ntoa(double n) {
    stringstream s;
    s << n;
    return s.str();
}

string itoa(uint64_t n) {
    stringstream s;
    s << n;
    return s.str();
}
