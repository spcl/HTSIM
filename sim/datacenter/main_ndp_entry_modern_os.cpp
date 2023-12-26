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
    Packet::set_packet_size(2048);
    eventlist.setEndtime(timeFromSec(10));
    Clock c(timeFromSec(5 / 100.), eventlist);
    mem_b queuesize = memFromPkt(INFINITE_BUFFER_SIZE);
    int no_of_conns = 0, cwnd = 40, no_of_nodes = DEFAULT_NODES;
    stringstream filename(ios_base::out);
    RouteStrategy route_strategy = NOT_SET;
    std::string goal_filename;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    simtime_picosec hop_latency = timeFromNs((uint32_t)RTT);
    simtime_picosec switch_latency = timeFromNs((uint32_t)0);
    int packet_size = 2048;
    int seed = -1;
    bool collect_data = false;
    COLLECT_DATA = collect_data;
    bool oversubscribed_congestion_control = false;

    int i = 1;
    filename << "logout.dat";
    printf("Name Running: NDP\n");

    while (i < argc) {
        if (!strcmp(argv[i], "-o")) {
            filename.str(std::string());
            filename << argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-sub")) {
            subflow_count = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-oversubscribed_cc")) {
            oversubscribed_congestion_control = true;
            i++;
        } else if (!strcmp(argv[i], "-conns")) {
            no_of_conns = atoi(argv[i + 1]);
            cout << "no_of_conns " << no_of_conns << endl;
            cout << "!!currently hardcoded to 8, value will be ignored!!"
                 << endl;
            i++;
        } else if (!strcmp(argv[i], "-nodes")) {
            no_of_nodes = atoi(argv[i + 1]);
            cout << "no_of_nodes " << no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i], "-collect_data")) {
            collect_data = atoi(argv[i + 1]);
            COLLECT_DATA = collect_data;
            i++;
        } else if (!strcmp(argv[i], "-cwnd")) {
            cwnd = atoi(argv[i + 1]);
            cout << "cwnd " << cwnd << endl;
            i++;
        } else if (!strcmp(argv[i], "-q")) {
            queuesize = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-linkspeed")) {
            // linkspeed specified is in Mbps
            linkspeed = speedFromMbps(atof(argv[i + 1]));
            LINK_SPEED_MODERN = atoi(argv[i + 1]);
            printf("Speed is %lu\n", LINK_SPEED_MODERN);
            LINK_SPEED_MODERN = LINK_SPEED_MODERN / 1000;
            // Saving this for UEC reference, Gbps
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
        } else if (!strcmp(argv[i], "-goal")) {
            goal_filename = argv[i + 1];
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
            }
            i++;
        } else
            exit_error(argv[0]);

        i++;
    }

    // Initialize Seed, Logging and Other variables
    if (seed != -1) {
        srand(seed);
        srandom(seed);
    } else {
        srand(time(NULL));
        srandom(time(NULL));
    }
    if (COLLECT_DATA) {
        initializeLoggingFolders();
    }

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
    NdpSink::_oversubscribed_congestion_control =
            oversubscribed_congestion_control;

    if (oversubscribed_congestion_control)
        cout << "Using oversubscribed congestion control " << endl;

    // NdpSrc *ndpSrc;
    // NdpSink *ndpSnk;

    // Route *routeout, *routein;
    // double extrastarttime;

    // scanner interval must be less than min RTO
    NdpRtxTimerScanner ndpRtxScanner(timeFromUs((uint32_t)1000), eventlist);

    // int dest;

#if USE_FIRST_FIT
    if (subflow_count == 1) {
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL), eventlist);
    }
#endif

    FatTreeTopology::set_tiers(3);
    FatTreeTopology *top = new FatTreeTopology(
            no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff, COMPOSITE,
            hop_latency, switch_latency);

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

    // NdpPullPacer *pacer = new NdpPullPacer(eventlist,
    // speedFromMbps(static_cast<double>(HOST_NIC)), 1 /*pull at line rate*/);
    // NdpPullPacer* pacer = new NdpPullPacer(eventlist,
    // "/Users/localadmin/poli/new-datacenter-protocol/data/1500.recv.cdf.pretty");

    map<int, vector<int> *>::iterator it;

    // used just to print out stats data at the end
    list<const Route *> routes;
    // int connID = 0;

    printf("Starting LGS Interface");
    LogSimInterface *lgs = new LogSimInterface(NULL, &traffic_logger, eventlist,
                                               top, net_paths);

    lgs->set_protocol(NDP_PROTOCOL);
    lgs->set_cwd(cwnd);
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

    // GO!
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
