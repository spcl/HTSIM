// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "config.h"
#include <sstream>

#include "clock.h"
#include "compositequeue.h"
#include "connection_matrix.h"
#include "fat_tree_interdc_topology.h"

#include "eventlist.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
#include "ndp.h"
#include "network.h"
#include "pipe.h"
#include "queue_lossless_input.h"
#include "randomqueue.h"
#include "shortflows.h"
#include "topology.h"
#include <iostream>
#include <math.h>
#include <string.h>
#include <unistd.h>

#include "fat_tree_switch.h"
#include "fat_tree_topology.h"

#include <list>

// Simulation params

#define PRINT_PATHS 1

#define PERIODIC 0
#include "main.h"

uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.02 ms
int DEFAULT_NODES = 432;
#define DEFAULT_QUEUE_SIZE 15

string ntoa(double n);
string itoa(uint64_t n);

// #define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

void exit_error(char *progr) {
    cout << "Usage " << progr
         << " [-nodes N]\n\t[-conns C]\n\t[-cwnd cwnd_size]\n\t[-q "
            "queue_size]\n\t[-oversubscribed_cc] Use receiver-driven AIMD to "
            "reduce total window when trims are not last hop\n\t[-queue_type "
            "composite|random|lossless|lossless_input|]\n\t[-tm "
            "traffic_matrix_file]\n\t[-strat route_strategy "
            "(single,rand,perm,pull,ecmp,\n\tecmp_host "
            "path_count,ecmp_ar,ecmp_rr,\n\tecmp_host_ar ar_thresh)]\n\t[-log "
            "log_level]\n\t[-seed random_seed]\n\t[-end "
            "end_time_in_usec]\n\t[-mtu MTU]\n\t[-hop_latency x] per hop wire "
            "latency in us,default 1\n\t[-switch_latency x] switching latency "
            "in us, default 0\n\t[-host_queue_type  swift|prio|fair_prio]"
         << endl;
    exit(1);
}

void print_path(std::ofstream &paths, const Route *rt) {
    for (size_t i = 1; i < rt->size() - 1; i++) {
        BaseQueue *q = dynamic_cast<BaseQueue *>(rt->at(i));
        if (q != NULL)
            paths << "Q:" << q->str() << " ";
        else
            paths << "- ";
    }

    paths << endl;
}

void filter_paths(uint32_t src_id, vector<const Route *> &paths, FatTreeTopology *top) {
    uint32_t num_servers = top->no_of_servers();
    uint32_t num_cores = top->no_of_cores();
    uint32_t num_pods = top->no_of_pods();
    uint32_t pod_switches = top->no_of_switches_per_pod();

    uint32_t path_classes = pod_switches / 2;
    cout << "srv: " << num_servers << " cores: " << num_cores << " pods: " << num_pods << " pod_sw: " << pod_switches
         << " classes: " << path_classes << endl;
    uint32_t pclass = src_id % path_classes;
    cout << "src: " << src_id << " class: " << pclass << endl;

    for (uint32_t r = 0; r < paths.size(); r++) {
        const Route *rt = paths.at(r);
        if (rt->size() == 12) {
            BaseQueue *q = dynamic_cast<BaseQueue *>(rt->at(6));
            cout << "Q:" << atoi(q->str().c_str() + 2) << " " << q->str() << endl;
            uint32_t core = atoi(q->str().c_str() + 2);
            if (core % path_classes != pclass) {
                paths[r] = NULL;
            }
        }
    }
}

int main(int argc, char **argv) {
    Packet::set_packet_size(PKT_SIZE_MODERN);
    eventlist.setEndtime(timeFromSec(1));
    Clock c(timeFromSec(5 / 100.), eventlist);
    mem_b queuesize = INFINITE_BUFFER_SIZE;
    int no_of_conns = 0, cwnd = 40, no_of_nodes = DEFAULT_NODES;
    stringstream filename(ios_base::out);
    bool topology_normal = true;
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
    char *topo_file = NULL;
    char *tm_file = NULL;
    bool rts = false;
    bool log_tor_downqueue = false;
    uint64_t interdc_delay = 0;
    bool log_tor_upqueue = false;
    bool log_switches = false;
    bool log_queue_usage = false;

    int i = 1;
    filename << "logout.dat";

    while (i < argc) {
        if (!strcmp(argv[i], "-o")) {
            filename.str(std::string());
            filename << argv[i + 1];
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

        } else if (!strcmp(argv[i], "-topo")) {
            topo_file = argv[i + 1];
            cout << "FatTree topology input file: " << topo_file << endl;
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
        } else if (!strcmp(argv[i], "-tm")) {
            tm_file = argv[i + 1];
            cout << "traffic matrix input file: " << tm_file << endl;
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
            } else if (!strcmp(argv[i + 1], "ecmp_host_random_ecn")) {
                route_strategy = ECMP_RANDOM_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "ecmp_host_random2_ecn")) {
                route_strategy = ECMP_RANDOM2_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
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

    // prepare the loggers

    cout << "Logging to " << filename.str() << endl;
    // Logfile
    Logfile logfile(filename.str(), eventlist);

    cout << "Linkspeed set to " << linkspeed / 1000000000 << "Gbps" << endl;
    logfile.setStartTime(timeFromSec(0));

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths) {
        cout << "Can't open for writing paths file!" << endl;
        exit(1);
    }
#endif

    printf("Name Running: NDP\n");

    NdpSrc::setMinRTO(50000); // increase RTO to avoid spurious retransmits
    NdpSrc::setPathEntropySize(number_entropies);
    NdpSrc::setRouteStrategy(route_strategy);
    NdpSink::setRouteStrategy(route_strategy);

    NdpSrc *ndpSrc;
    NdpSink *ndpSnk;

    Route *routeout, *routein;

    // scanner interval must be less than min RTO
    NdpRtxTimerScanner ndpRtxScanner(timeFromUs((uint32_t)9), eventlist);

    QueueLoggerFactory *qlf = 0;
    if (log_tor_downqueue || log_tor_upqueue) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    } else if (log_queue_usage) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }
#ifdef FAT_TREE
    FatTreeTopology::set_tiers(3);
    FatTreeTopology::set_os_stage_2(fat_tree_k);
    FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
    if (kmin != -1 && kmax != -1) {
        FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
    }
    FatTreeTopology *top = new FatTreeTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, NULL, COMPOSITE,
                                               hop_latency, switch_latency);
#endif

#ifdef OV_FAT_TREE
    OversubscribedFatTreeTopology *top = new OversubscribedFatTreeTopology(lf, &eventlist, ff);
#endif

#ifdef MH_FAT_TREE
    MultihomedFatTreeTopology *top = new MultihomedFatTreeTopology(lf, &eventlist, ff);
#endif

#ifdef STAR
    StarTopology *top = new StarTopology(lf, &eventlist, ff);
#endif

#ifdef BCUBE
    BCubeTopology *top = new BCubeTopology(lf, &eventlist, ff);
    cout << "BCUBE " << K << endl;
#endif

#ifdef VL2
    VL2Topology *top = new VL2Topology(lf, &eventlist, ff);
#endif

    if (log_switches) {
        top->add_switch_loggers(logfile, timeFromUs(20.0));
    }

    vector<const Route *> ***net_paths;
    net_paths = new vector<const Route *> **[no_of_nodes];

    int **path_refcounts;
    path_refcounts = new int *[no_of_nodes];

    int *is_dest = new int[no_of_nodes];

    for (int s = 0; s < no_of_nodes; s++) {
        is_dest[s] = 0;
        net_paths[s] = new vector<const Route *> *[no_of_nodes];
        path_refcounts[s] = new int[no_of_nodes];
        for (int d = 0; d < no_of_nodes; d++) {
            net_paths[s][d] = NULL;
            path_refcounts[s][d] = 0;
        }
    }

    /* FatTreeInterDCTopology *top_dc = NULL;
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
            if (top_dc->no_of_nodes() != no_of_nodes) {
                cerr << "Mismatch between connection matrix (" << no_of_nodes << " nodes) and topology ("
                     << top_dc->no_of_nodes() << " nodes)" << endl;
                exit(1);
            }
        } else {
            FatTreeInterDCTopology::set_tiers(3);
            top_dc = new FatTreeInterDCTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, NULL, COMPOSITE,
                                                hop_latency, switch_latency, FAIR_PRIO);
        }
    } */

    ConnectionMatrix *conns = new ConnectionMatrix(no_of_nodes);

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

    if ((int)conns->N != no_of_nodes) {
        cout << "Connection matrix number of nodes is " << conns->N << " while I am using " << no_of_nodes << endl;
        exit(-1);
    }

    // handle link failures specified in the connection matrix.
    for (size_t c = 0; c < conns->failures.size(); c++) {
        failure *crt = conns->failures.at(c);

        cout << "Adding link failure switch type" << crt->switch_type << " Switch ID " << crt->switch_id << " link ID "
             << crt->link_id << endl;
        top->add_failed_link(crt->switch_type, crt->switch_id, crt->link_id);
    }

    vector<NdpPullPacer *> pacers;

    for (int ix = 0; ix < no_of_nodes; ix++)
        pacers.push_back(new NdpPullPacer(eventlist, linkspeed, 0.99));

    // used just to print out stats data at the end
    // list <const Route*> routes;

    vector<connection *> *all_conns = conns->getAllConnections();
    vector<NdpSrc *> ndp_srcs;

    for (size_t c = 0; c < all_conns->size(); c++) {
        connection *crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        path_refcounts[src][dest]++;
        path_refcounts[dest][src]++;

        if (!net_paths[src][dest] && route_strategy != ECMP_FIB && route_strategy != ECMP_FIB_ECN &&
            route_strategy != REACTIVE_ECN && route_strategy != ECMP_RANDOM2_ECN) {
            vector<const Route *> *paths = top->get_bidir_paths(src, dest, false);
            net_paths[src][dest] = paths;
            /*
              for (unsigned int i = 0; i < paths->size(); i++) {
              routes.push_back((*paths)[i]);
              }
            */
        }
        if (!net_paths[dest][src] && route_strategy != ECMP_FIB && route_strategy != ECMP_FIB_ECN &&
            route_strategy != REACTIVE_ECN && route_strategy != ECMP_RANDOM2_ECN) {
            vector<const Route *> *paths = top->get_bidir_paths(dest, src, false);
            net_paths[dest][src] = paths;
        }
    }

    map<flowid_t, TriggerTarget *> flowmap;

    for (size_t c = 0; c < all_conns->size(); c++) {
        connection *crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        // cout << "Connection " << crt->src << "->" <<crt->dst << " starting at
        // " << crt->start << " size " << crt->size << endl;

        ndpSrc = new NdpSrc(NULL, NULL, eventlist, rts);
        ndpSrc->setCwnd(cwnd * Packet::data_packet_size());
        ndp_srcs.push_back(ndpSrc);
        ndpSrc->set_dst(dest);
        if (crt->flowid) {
            ndpSrc->set_flowid(crt->flowid);
            assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
            flowmap[crt->flowid] = ndpSrc;
        }

        if (crt->size > 0) {
            ndpSrc->set_flowsize(crt->size);
        }

        if (crt->trigger) {
            Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*ndpSrc);
        }
        if (crt->send_done_trigger) {
            Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
            ndpSrc->set_end_trigger(*trig);
        }

        ndpSnk = new NdpSink(pacers[dest]);

        ndpSrc->setName("ndp_" + ntoa(src) + "_" + ntoa(dest));

        cout << "ndp_" + ntoa(src) + "_" + ntoa(dest) << endl;
        logfile.writeName(*ndpSrc);

        ndpSnk->set_src(src);

        ndpSnk->setName("ndp_sink_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*ndpSnk);
        if (crt->recv_done_trigger) {
            Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
            ndpSnk->set_end_trigger(*trig);
        }

        ndpSnk->set_priority(crt->priority);

        ndpRtxScanner.registerNdp(*ndpSrc);

        switch (route_strategy) {
        case SCATTER_PERMUTE:
        case SCATTER_RANDOM:
        case SCATTER_ECMP:
        case PULL_BASED:
            ndpSrc->connect(NULL, NULL, *ndpSnk, crt->start);
            ndpSrc->set_paths(net_paths[src][dest]);
            ndpSnk->set_paths(net_paths[dest][src]);
            break;
        case ECMP_FIB:
        case ECMP_FIB_ECN:
        case ECMP_RANDOM2_ECN:
        case REACTIVE_ECN: {
            Route *srctotor = new Route();
            srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
            srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
            srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

            Route *dsttotor = new Route();
            dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
            dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
            dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

            ndpSrc->connect(srctotor, dsttotor, *ndpSnk, crt->start);
            ndpSrc->set_paths(number_entropies);
            ndpSnk->set_paths(number_entropies);

            // register src and snk to receive packets from their respective
            // TORs.
            assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
            assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
            top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, ndpSrc->flow_id(), ndpSrc);
            top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, ndpSrc->flow_id(), ndpSnk);
            break;
        }
        case SINGLE_PATH: {
            assert(route_strategy == SINGLE_PATH);
            int choice = rand() % net_paths[src][dest]->size();
            routeout = new Route(*(net_paths[src][dest]->at(choice)));
            routeout->add_endpoints(ndpSrc, ndpSnk);

            routein = new Route(*top->get_bidir_paths(dest, src, false)->at(choice));
            routein->add_endpoints(ndpSnk, ndpSrc);
            ndpSrc->connect(routeout, routein, *ndpSnk, crt->start);
            break;
        }
        case NOT_SET:
            abort();
        default:
            abort();
            break;
        }

        path_refcounts[src][dest]--;
        path_refcounts[dest][src]--;

        // set up the triggers
        // xxx

        // free up the routes if no other connection needs them
        if (path_refcounts[src][dest] == 0 && net_paths[src][dest]) {
            vector<const Route *>::iterator i;
            for (i = net_paths[src][dest]->begin(); i != net_paths[src][dest]->end(); i++) {
                if ((*i)->reverse())
                    delete (*i)->reverse();
                delete *i;
            }
            delete net_paths[src][dest];
        }
        if (path_refcounts[dest][src] == 0 && net_paths[dest][src]) {
            vector<const Route *>::iterator i;
            for (i = net_paths[dest][src]->begin(); i != net_paths[dest][src]->end(); i++) {
                if ((*i)->reverse())
                    delete (*i)->reverse();
                delete *i;
            }
            delete net_paths[dest][src];
        }
    }

    for (int ix = 0; ix < no_of_nodes; ix++) {
        delete path_refcounts[ix];
    }

    Logged::dump_idmap();
    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# hostnicrate = " + ntoa(linkspeed / 1000000) + " Mbps");
    // logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + "
    // pkt/sec"); logfile.write("# buffer = " + ntoa((double)
    // (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    // GO!
    cout << "Starting simulation" << endl;
    while (eventlist.doNextEvent()) {
    }

    cout << "Done" << endl;

    int new_pkts = 0, rtx_pkts = 0, bounce_pkts = 0;
    for (size_t ix = 0; ix < ndp_srcs.size(); ix++) {
        new_pkts += ndp_srcs[ix]->_new_packets_sent;
        rtx_pkts += ndp_srcs[ix]->_rtx_packets_sent;
        bounce_pkts += ndp_srcs[ix]->_bounces_received;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << " Bounced: " << bounce_pkts << endl;

    for (std::size_t i = 0; i < ndp_srcs.size(); ++i) {
        delete ndp_srcs[i];
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
