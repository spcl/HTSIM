// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "config.h"
#include "network.h"
#include "randomqueue.h"
#include <iostream>
#include <list>
#include <math.h>
#include <sstream>
#include <string.h>
//#include "subflow_control.h"
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
//#include "vl2_topology.h"

#include "fat_tree_topology.h"
//#include "oversubscribed_fat_tree_topology.h"
//#include "multihomed_fat_tree_topology.h"
//#include "star_topology.h"
//#include "bcube_topology.h"
#include "dragonfly_topology.h"
#include "dragonfly_topology.cpp"
#include "dragonfly_switch.h"
#include "dragonfly_switch.cpp"
#include "slimfly_topology.h"
#include "slimfly_topology.cpp"
#include "slimfly_switch.h"
#include "slimfly_switch.cpp"
#include "hammingmesh_topology.h"
#include "hammingmesh_topology.cpp"
#include "hammingmesh_switch.h"
#include "hammingmesh_switch.cpp"
#include "b-cube_topology.h"
#include "b-cube_topology.cpp"
#include "b-cube_switch.h"
#include "b-cube_switch.cpp"
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

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
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
    uint64_t interdc_delay = 0;
    //uint64_t max_queue_size = 0;
    
    queue_type queue_choice = COMPOSITE;
    TopologyCase topology = FAT_TREE_CASE;
    uint32_t p = 0;
    uint32_t a = 0;
    uint32_t h = 0;
    DragonflySwitch::routing_strategy df_routing_strategy = DragonflySwitch::NIX;
    uint32_t q_base = 0;
    uint32_t q_exp = 0;
    SlimflySwitch::routing_strategy sf_routing_strategy = SlimflySwitch::NIX;
    uint32_t height = 0;
    uint32_t width = 0;
    uint32_t height_board = 0;
    uint32_t width_board = 0;
    HammingmeshSwitch::routing_strategy hm_routing_strategy = HammingmeshSwitch::NIX;
    uint32_t n_bcube = 0;
    uint32_t k_bcube = 0;
    BCubeSwitch::routing_strategy bc_routing_strategy = BCubeSwitch::NIX;
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

        } else if (!strcmp(argv[i], "-interdc_delay")) {
            interdc_delay = atoi(argv[i + 1]);
            interdc_delay *= 1000;
            i++;
        } /* else if (!strcmp(argv[i], "-max_queue_size")) {
            max_queue_size = atoi(argv[i + 1]);
            i++;
        } */ else if (!strcmp(argv[i], "-number_entropies")) {
            number_entropies = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-kmax")) {
            // kmin as percentage of queue size (0..100)
            kmax = atoi(argv[i + 1]);
            printf("KMax: %d\n", atoi(argv[i + 1]));
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
            PKT_SIZE_MODERN =
                    packet_size; // Saving this for UEC reference, Bytes
            i++;
        } else if (!strcmp(argv[i], "-switch_latency")) {
            switch_latency = timeFromNs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-hop_latency")) {
            hop_latency = timeFromNs(atof(argv[i + 1]));
            LINK_DELAY_MODERN = hop_latency /
                                1000; // Saving this for UEC reference, ps to ns
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
        }  else if (!strcmp(argv[i], "-topology")) {
            if (!strcmp(argv[i + 1], "fat_tree")) {
                topology = FAT_TREE_CASE;
            }
            else if (!strcmp(argv[i + 1], "fat_tree_dc")) {
                topology = FAT_TREE_DC_CASE;
            }
            else if (!strcmp(argv[i + 1], "dragonfly")) {
                topology = DRAGONFLY_CASE;
            }
            else if (!strcmp(argv[i + 1], "slimfly")) {
                topology = SLIMFLY_CASE;
            }
            else if (!strcmp(argv[i + 1], "hammingmesh")) {
                topology = HAMMINGMESH_CASE;
            }
            else if (!strcmp(argv[i + 1], "bcube")) {
                topology = BCUBE_CASE;
            }
            i++;
        } else if (!strcmp(argv[i], "-p")) {
            p = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-a")) {
            a = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-h")) {
            h = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-df_strategy")){
            if (!strcmp(argv[i + 1], "nix")) {
                df_routing_strategy = DragonflySwitch::NIX;
            } else if (!strcmp(argv[i + 1], "minimal")) {
                df_routing_strategy = DragonflySwitch::MINIMAL;
            } else if (!strcmp(argv[i + 1], "valiants")) {
                df_routing_strategy = DragonflySwitch::VALIANTS;
            } else {
                cerr << "Wrong strategy for dragonfly chosen.\n";
            }
            i++;
        } else if (!strcmp(argv[i], "-q_base")) {
            q_base = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-q_exp")) {
            q_exp = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-sf_strategy")){
            if (!strcmp(argv[i + 1], "nix")) {
                sf_routing_strategy = SlimflySwitch::NIX;
            } else if (!strcmp(argv[i + 1], "minimal")) {
                sf_routing_strategy = SlimflySwitch::MINIMAL;
            } else if (!strcmp(argv[i + 1], "valiants")) {
                sf_routing_strategy = SlimflySwitch::VALIANTS;
            } else {
                cerr << "Wrong strategy for slimfly chosen.\n";
            }
            i++;
        } else if (!strcmp(argv[i], "-height")) {
            height = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-width")) {
            width = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-height_board")) {
            height_board = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-width_board")) {
            width_board = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-hm_strategy")){
            if (!strcmp(argv[i + 1], "nix")) {
                hm_routing_strategy = HammingmeshSwitch::NIX;
            } else if (!strcmp(argv[i + 1], "minimal")) {
                hm_routing_strategy = HammingmeshSwitch::MINIMAL;
            } else {
                cerr << "Wrong strategy for hammingmesh chosen.\n";
            }
            i++;
        } else if (!strcmp(argv[i], "-n_bcube")) {
            n_bcube = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-k_bcube")) {
            k_bcube = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-bc_strategy")){
            if (!strcmp(argv[i + 1], "nix")) {
                bc_routing_strategy = BCubeSwitch::NIX;
            } else if (!strcmp(argv[i + 1], "minimal")) {
                bc_routing_strategy = BCubeSwitch::MINIMAL;
            } else {
                cerr << "Wrong strategy for bcube chosen.\n";
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
    FatTreeTopology::set_tiers(3);
    FatTreeTopology::set_os_stage_2(fat_tree_k);
    FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
    if (kmin != -1 && kmax != -1) {
        FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
    }
    /* FatTreeTopology *top = new FatTreeTopology(
            no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff, COMPOSITE,
            hop_latency, switch_latency); */
#endif

#ifdef OV_FAT_TREE
    OversubscribedFatTreeTopology *top =
            new OversubscribedFatTreeTopology(&logfile, &eventlist, ff);
#endif

#ifdef MH_FAT_TREE
    MultihomedFatTreeTopology *top =
            new MultihomedFatTreeTopology(&logfile, &eventlist, ff);
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

    vector<const Route *> ***net_paths;
    net_paths = new vector<const Route *> **[no_of_nodes];

    int *is_dest = new int[no_of_nodes];

    for (int i = 0; i < no_of_nodes; i++) {
        is_dest[i] = 0;
        net_paths[i] = new vector<const Route *> *[no_of_nodes];
        for (int j = 0; j < no_of_nodes; j++)
            net_paths[i][j] = NULL;
    }

#ifdef USE_FIRST_FIT
    if (ff)
        ff->net_paths = net_paths;
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

    printf("Starting LGS Interface");
    LogSimInterface *lgs;

    switch (topology) {
        case (FAT_TREE_CASE): {
            printf("Fat-Tree Topology\n");
            FatTreeTopology::set_tiers(3);
            FatTreeTopology::set_os_stage_2(fat_tree_k);
            FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            FatTreeTopology *top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff,
                                                    queue_choice, hop_latency, switch_latency);
            lgs = new LogSimInterface(NULL, &traffic_logger, eventlist, top, NULL);
            break;
        } 
        case (FAT_TREE_DC_CASE): {
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
            FatTreeInterDCTopology *top = new FatTreeInterDCTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff,
                                                    queue_choice, hop_latency, switch_latency);
            lgs = new LogSimInterface(NULL, &traffic_logger, eventlist, top, NULL);
            break;
        }
        case (DRAGONFLY_CASE): {
            // Hier Code einfügen.
            // DragonflyTopology(uint32_t p, uint32_t a, uint32_t h, mem_b queuesize, EventList *ev, queue_type q);
            DragonflyTopology *top_df = new DragonflyTopology(p, a, h, queuesize, &eventlist, queue_choice);
            break;
        }
        case (SLIMFLY_CASE): {
            // Hier Code einfügen.
            // SlimflyTopology(uint32_t p, uint32_t q_base, uint32_t q_exp, mem_b queuesize, EventList *ev, queue_type q);
            SlimflyTopology *top_sf = new SlimflyTopology(p, q_base, q_exp, queuesize, &eventlist, queue_choice);
            break;
        }
        case (HAMMINGMESH_CASE): {
            HammingmeshTopology *top_hm = new HammingmeshTopology(height, width, height_board, width_board, queuesize, &eventlist, queue_choice);
            break;
        }
        case (BCUBE_CASE): {
            BCubeTopology *top_bc = new BCubeTopology(n_bcube, k_bcube, queuesize, &eventlist, queue_choice);
            break;
        }
    }

    lgs->set_protocol(UEC_PROTOCOL);
    lgs->set_cwd(cwnd);
    lgs->set_queue_size(queuesize);
    lgs->setNumberPaths(number_entropies);
    start_lgs(goal_filename, *lgs);

    cout << "Mean number of subflows " << ntoa((double)tot_subs / cnt_con)
         << endl;

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC * CORE_TO_HOST) +
                  " pkt/sec");
    printf("Host Link Rate %d - Core Link Rate %d - Pkt Size %d", HOST_NIC,
           CORE_TO_HOST, pktsize);
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    while (eventlist.doNextEvent()) {
    }

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
                cout << q->nodename() << " id=" << q->id << " "
                     << q->num_packets() << "pkts " << q->num_headers()
                     << "hdrs " << q->num_acks() << "acks " << q->num_nacks()
                     << "nacks " << q->num_stripped() << "stripped" << endl;
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
