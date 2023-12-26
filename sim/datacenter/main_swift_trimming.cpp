// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "config.h"
#include <sstream>

#include "clock.h"
#include "compositequeue.h"
#include "connection_matrix.h"
#include "eventlist.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
#include "network.h"
#include "pipe.h"
#include "queue_lossless_input.h"
#include "randomqueue.h"
#include "shortflows.h"
#include "swifttrimming.h"
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

uint32_t RTT =
        1; // this is per link delay in us; identical RTT microseconds = 0.02 ms
int DEFAULT_NODES = 432;
#define DEFAULT_QUEUE_SIZE 15

string ntoa(double n);
string itoa(uint64_t n);

//#define SWITCH_BUFFER (SERVICE * RTT / 1000)
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

void filter_paths(uint32_t src_id, vector<const Route *> &paths,
                  FatTreeTopology *top) {
    uint32_t num_servers = top->no_of_servers();
    uint32_t num_cores = top->no_of_cores();
    uint32_t num_pods = top->no_of_pods();
    uint32_t pod_switches = top->no_of_switches_per_pod();

    uint32_t path_classes = pod_switches / 2;
    cout << "srv: " << num_servers << " cores: " << num_cores
         << " pods: " << num_pods << " pod_sw: " << pod_switches
         << " classes: " << path_classes << endl;
    uint32_t pclass = src_id % path_classes;
    cout << "src: " << src_id << " class: " << pclass << endl;

    for (uint32_t r = 0; r < paths.size(); r++) {
        const Route *rt = paths.at(r);
        if (rt->size() == 12) {
            BaseQueue *q = dynamic_cast<BaseQueue *>(rt->at(6));
            cout << "Q:" << atoi(q->str().c_str() + 2) << " " << q->str()
                 << endl;
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
    int no_of_conns = 0, cwnd = MAX_CWD_MODERN_UEC, no_of_nodes = DEFAULT_NODES;
    stringstream filename(ios_base::out);
    RouteStrategy route_strategy = NOT_SET;
    std::string goal_filename;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    simtime_picosec hop_latency = timeFromNs((uint32_t)RTT);
    simtime_picosec switch_latency = timeFromNs((uint32_t)0);
    int packet_size = 2048;
    int kmin = -1;
    int kmax = -1;
    int bts_threshold = -1;
    int seed = -1;
    int number_entropies = 256;
    queue_type queue_choice = COMPOSITE;
    bool ignore_ecn_data = true;
    SwiftTrimmingSrc::set_fast_drop(false);
    bool do_jitter = false;
    bool do_exponential_gain = false;
    bool use_fast_increase = false;
    double gain_value_med_inc = 1;
    double jitter_value_med_inc = 1;
    double delay_gain_value_med_inc = 5;
    int target_rtt_percentage_over_base = 50;
    bool collect_data = false;
    int fat_tree_k = 1; // 1:1 default
    COLLECT_DATA = collect_data;
    bool use_super_fast_increase = false;
    double y_gain = 1;
    double x_gain = 0.15;
    double z_gain = 1;
    double w_gain = 1;
    double bonus_drop = 1;
    double drop_value_buffer = 1;
    double starting_cwnd_ratio = 0;
    double queue_size_ratio = 0;
    bool disable_case_3 = false;
    simtime_picosec endtime = timeFromMs(1.2);
    char *tm_file = NULL;
    int ratio_os_stage_1 = 1;
    bool log_tor_downqueue = false;
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
            cout << "!!currently hardcoded to 8, value will be ignored!!"
                 << endl;
            i++;
        } else if (!strcmp(argv[i], "-nodes")) {
            no_of_nodes = atoi(argv[i + 1]);
            cout << "no_of_nodes " << no_of_nodes << endl;
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
        } else if (!strcmp(argv[i], "-kmin")) {
            // kmin as percentage of queue size (0..100)
            kmin = atoi(argv[i + 1]);
            printf("KMin: %d\n", atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-k")) {
            fat_tree_k = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-ratio_os_stage_1")) {
            ratio_os_stage_1 = atoi(argv[i + 1]);
            SwiftTrimmingSrc::set_os_ratio_stage_1(ratio_os_stage_1);
            i++;
        } else if (!strcmp(argv[i], "-kmax")) {
            // kmin as percentage of queue size (0..100)
            kmax = atoi(argv[i + 1]);
            printf("KMax: %d\n", atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-bts_trigger")) {
            bts_threshold = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-mtu")) {
            packet_size = atoi(argv[i + 1]);
            PKT_SIZE_MODERN =
                    packet_size; // Saving this for UEC reference, Bytes
            i++;
        } else if (!strcmp(argv[i], "-disable_case_3")) {
            disable_case_3 = atoi(argv[i + 1]);
            SwiftTrimmingSrc::set_disable_case_3(disable_case_3);
            printf("DisableCase3: %d\n", disable_case_3);
            i++;
        } else if (!strcmp(argv[i], "-number_entropies")) {
            number_entropies = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-switch_latency")) {
            switch_latency = timeFromNs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-hop_latency")) {
            hop_latency = timeFromNs(atof(argv[i + 1]));
            LINK_DELAY_MODERN = hop_latency /
                                1000; // Saving this for UEC reference, ps to ns
            i++;
        } else if (!strcmp(argv[i], "-ignore_ecn_data")) {
            ignore_ecn_data = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-fast_drop")) {
            SwiftTrimmingSrc::set_fast_drop(atoi(argv[i + 1]));
            printf("FastDrop: %d\n", atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-seed")) {
            seed = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-collect_data")) {
            collect_data = atoi(argv[i + 1]);
            COLLECT_DATA = collect_data;
            i++;
        } else if (!strcmp(argv[i], "-tm")) {
            tm_file = argv[i + 1];
            cout << "traffic matrix input file: " << tm_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-do_jitter")) {
            do_jitter = atoi(argv[i + 1]);
            SwiftTrimmingSrc::set_do_jitter(do_jitter);
            printf("DoJitter: %d\n", do_jitter);
            i++;
        } else if (!strcmp(argv[i], "-do_exponential_gain")) {
            do_exponential_gain = atoi(argv[i + 1]);
            SwiftTrimmingSrc::set_do_exponential_gain(do_exponential_gain);
            printf("DoExpGain: %d\n", do_exponential_gain);
            i++;
        } else if (!strcmp(argv[i], "-use_fast_increase")) {
            use_fast_increase = atoi(argv[i + 1]);
            SwiftTrimmingSrc::set_use_fast_increase(use_fast_increase);
            printf("FastIncrease: %d\n", use_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-use_super_fast_increase")) {
            use_super_fast_increase = atoi(argv[i + 1]);
            SwiftTrimmingSrc::set_use_super_fast_increase(
                    use_super_fast_increase);
            printf("FastIncreaseSuper: %d\n", use_super_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-gain_value_med_inc")) {
            gain_value_med_inc = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_gain_value_med_inc(gain_value_med_inc);
            printf("GainValueMedIncrease: %f\n", gain_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-jitter_value_med_inc")) {
            jitter_value_med_inc = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_jitter_value_med_inc(jitter_value_med_inc);
            printf("JitterValue: %f\n", jitter_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-delay_gain_value_med_inc")) {
            delay_gain_value_med_inc = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_delay_gain_value_med_inc(
                    delay_gain_value_med_inc);
            printf("DelayGainValue: %f\n", delay_gain_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-target_rtt_percentage_over_base")) {
            target_rtt_percentage_over_base = atoi(argv[i + 1]);
            SwiftTrimmingSrc::set_target_rtt_percentage_over_base(
                    target_rtt_percentage_over_base);
            printf("TargetRTT: %d\n", target_rtt_percentage_over_base);
            i++;
        } else if (!strcmp(argv[i], "-fast_drop_rtt")) {
            SwiftTrimmingSrc::set_fast_drop_rtt(atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-y_gain")) {
            y_gain = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_y_gain(y_gain);
            printf("YGain: %f\n", y_gain);
            i++;
        } else if (!strcmp(argv[i], "-x_gain")) {
            x_gain = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_x_gain(x_gain);
            printf("XGain: %f\n", x_gain);
            i++;
        } else if (!strcmp(argv[i], "-z_gain")) {
            z_gain = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_z_gain(z_gain);
            printf("ZGain: %f\n", z_gain);
            i++;
        } else if (!strcmp(argv[i], "-w_gain")) {
            w_gain = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_w_gain(w_gain);
            printf("WGain: %f\n", w_gain);
            i++;
        } else if (!strcmp(argv[i], "-starting_cwnd_ratio")) {
            starting_cwnd_ratio = std::stod(argv[i + 1]);
            printf("StartingWindowRatio: %f\n", starting_cwnd_ratio);
            i++;
        } else if (!strcmp(argv[i], "-queue_size_ratio")) {
            queue_size_ratio = std::stod(argv[i + 1]);
            printf("QueueSizeRatio: %f\n", queue_size_ratio);
            i++;
        } else if (!strcmp(argv[i], "-bonus_drop")) {
            bonus_drop = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_bonus_drop(bonus_drop);
            printf("BonusDrop: %f\n", bonus_drop);
            i++;
        } else if (!strcmp(argv[i], "-drop_value_buffer")) {
            drop_value_buffer = std::stod(argv[i + 1]);
            SwiftTrimmingSrc::set_buffer_drop(drop_value_buffer);
            printf("BufferDrop: %f\n", drop_value_buffer);
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
            } else if (!strcmp(argv[i + 1], "ecmp_host_random_ecn")) {
                route_strategy = ECMP_RANDOM_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "ecmp_host_random2_ecn")) {
                route_strategy = ECMP_RANDOM2_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
            }
            i++;
        } else if (!strcmp(argv[i], "-queue_type")) {
            if (!strcmp(argv[i + 1], "composite")) {
                queue_choice = COMPOSITE;
                SwiftTrimmingSrc::set_queue_type("composite");
            } else if (!strcmp(argv[i + 1], "composite_bts")) {
                queue_choice = COMPOSITE_BTS;
                SwiftTrimmingSrc::set_queue_type("composite_bts");
                printf("Name Running: UEC BTS\n");
            }
            i++;
        } else if (!strcmp(argv[i], "-algorithm")) {
            if (!strcmp(argv[i + 1], "delayA")) {
                SwiftTrimmingSrc::set_alogirthm("delayA");
                printf("Name Running: UEC Version A\n");
            } else if (!strcmp(argv[i + 1], "delayB")) {
                SwiftTrimmingSrc::set_alogirthm("delayB");
                printf("Name Running: SMaRTT\n");
            } else if (!strcmp(argv[i + 1], "delayB_rtt")) {
                SwiftTrimmingSrc::set_alogirthm("delayB_rtt");
                printf("Name Running: SMaRTT Per RTT\n");
            } else if (!strcmp(argv[i + 1], "delayC")) {
                SwiftTrimmingSrc::set_alogirthm("delayC");
            } else if (!strcmp(argv[i + 1], "delayD")) {
                SwiftTrimmingSrc::set_alogirthm("delayD");
                printf("Name Running: STrack\n");
            } else if (!strcmp(argv[i + 1], "standard_trimming")) {
                SwiftTrimmingSrc::set_alogirthm("standard_trimming");
                printf("Name Running: UEC Version D\n");
            } else if (!strcmp(argv[i + 1], "rtt")) {
                SwiftTrimmingSrc::set_alogirthm("rtt");
                printf("Name Running: SMaRTT RTT Only\n");
            } else if (!strcmp(argv[i + 1], "ecn")) {
                SwiftTrimmingSrc::set_alogirthm("ecn");
                printf("Name Running: SMaRTT ECN Only Constant\n");
            } else if (!strcmp(argv[i + 1], "custom")) {
                SwiftTrimmingSrc::set_alogirthm("custom");
                printf("Name Running: SMaRTT ECN Only Variable\n");
            }
            i++;
        } else {
            printf("Called with %s\n", argv[i]);
            exit_error(argv[0]);
        }

        i++;
    }

    SINGLE_PKT_TRASMISSION_TIME_MODERN = packet_size * 8 / (LINK_SPEED_MODERN);

    // Initialize Seed, Logging and Other variables
    if (seed != -1) {
        srand(seed);
        srandom(seed);
    } else {
        srand(time(NULL));
        srandom(time(NULL));
    }
    Packet::set_packet_size(packet_size);
    initializeLoggingFolders();

    srand(seed);
    srandom(seed);
    cout << "Parsed args\n";

    eventlist.setEndtime(timeFromUs((uint32_t)endtime));
    queuesize = memFromPkt(queuesize);
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

    // Calculate Network Info
    int hops = 6; // hardcoded for now
    uint64_t base_rtt_max_hops =
            (hops * LINK_DELAY_MODERN) +
            (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * hops) +
            (hops * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * hops);
    uint64_t bdp_local = base_rtt_max_hops * LINK_SPEED_MODERN / 8;
    if (queue_size_ratio == 0) {
        queuesize = bdp_local; // Equal to BDP if not other info
    } else {
        queuesize = bdp_local * queue_size_ratio;
    }

    SwiftTrimmingSrc::setRouteStrategy(route_strategy);
    SwiftTrimmingSink::setRouteStrategy(route_strategy);

    SwiftTrimmingSrc *uecSrc;
    SwiftTrimmingSink *uecSnk;

    Route *routeout, *routein;

    QueueLoggerFactory *qlf = 0;
    if (log_tor_downqueue || log_tor_upqueue) {
        qlf = new QueueLoggerFactory(
                &logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    } else if (log_queue_usage) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY,
                                     eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }
#ifdef FAT_TREE
    FatTreeTopology::set_tiers(3);
    FatTreeTopology::set_os_stage_2(fat_tree_k);
    FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
    FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
    FatTreeTopology::set_bts_threshold(bts_threshold);
    FatTreeTopology::set_ignore_data_ecn(ignore_ecn_data);
    FatTreeTopology *top = new FatTreeTopology(
            no_of_nodes, linkspeed, queuesize, NULL, &eventlist, NULL,
            queue_choice, hop_latency, switch_latency);
#endif

#ifdef OV_FAT_TREE
    OversubscribedFatTreeTopology *top =
            new OversubscribedFatTreeTopology(lf, &eventlist, ff);
#endif

#ifdef MH_FAT_TREE
    MultihomedFatTreeTopology *top =
            new MultihomedFatTreeTopology(lf, &eventlist, ff);
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
        cout << "Connection matrix number of nodes is " << conns->N
             << " while I am using " << no_of_nodes << endl;
        exit(-1);
    }

    // handle link failures specified in the connection matrix.
    for (size_t c = 0; c < conns->failures.size(); c++) {
        failure *crt = conns->failures.at(c);

        cout << "Adding link failure switch type" << crt->switch_type
             << " Switch ID " << crt->switch_id << " link ID " << crt->link_id
             << endl;
        top->add_failed_link(crt->switch_type, crt->switch_id, crt->link_id);
    }

    // used just to print out stats data at the end
    // list <const Route*> routes;

    vector<connection *> *all_conns = conns->getAllConnections();
    vector<SwiftTrimmingSrc *> uec_srcs;

    for (size_t c = 0; c < all_conns->size(); c++) {
        connection *crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        path_refcounts[src][dest]++;
        path_refcounts[dest][src]++;

        if (!net_paths[src][dest] && route_strategy != ECMP_FIB &&
            route_strategy != ECMP_FIB_ECN && route_strategy != REACTIVE_ECN &&
            route_strategy != ECMP_RANDOM2_ECN) {
            vector<const Route *> *paths =
                    top->get_bidir_paths(src, dest, false);
            net_paths[src][dest] = paths;
            /*
              for (unsigned int i = 0; i < paths->size(); i++) {
              routes.push_back((*paths)[i]);
              }
            */
        }
        if (!net_paths[dest][src] && route_strategy != ECMP_FIB &&
            route_strategy != ECMP_FIB_ECN && route_strategy != REACTIVE_ECN &&
            route_strategy != ECMP_RANDOM2_ECN) {
            vector<const Route *> *paths =
                    top->get_bidir_paths(dest, src, false);
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
        uint64_t rtt = BASE_RTT_MODERN * 1000;
        uint64_t bdp = BDP_MODERN_UEC;
        printf("Reaching here1\n");
        fflush(stdout);
        Route *myin = new Route(*top->get_paths(src, dest)->at(0));

        int hops = myin->hop_count(); // hardcoded for now
        uint64_t actual_starting_cwnd = 0;
        uint64_t base_rtt_max_hops =
                (hops * LINK_DELAY_MODERN) +
                (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * hops) +
                (hops * LINK_DELAY_MODERN) +
                (64 * 8 / LINK_SPEED_MODERN * hops);
        uint64_t bdp_local = base_rtt_max_hops * LINK_SPEED_MODERN / 8;

        if (starting_cwnd_ratio == 0) {
            actual_starting_cwnd = bdp_local; // Equal to BDP if not other info
        } else {
            actual_starting_cwnd = bdp_local * starting_cwnd_ratio;
        }

        SwiftTrimmingSrc::set_starting_cwnd(actual_starting_cwnd);

        uecSrc = new SwiftTrimmingSrc(NULL, NULL, eventlist, rtt, bdp, 100,
                                      myin->hop_count());
        uecSrc->setReuse(1);
        uecSrc->setIgnoreEcnAck(1);
        uecSrc->setIgnoreEcnData(1);
        uecSrc->setNumberEntropies(256);
        uec_srcs.push_back(uecSrc);
        uecSrc->set_dst(dest);
        fflush(stdout);
        if (crt->flowid) {
            uecSrc->set_flowid(crt->flowid);
            assert(flowmap.find(crt->flowid) ==
                   flowmap.end()); // don't have dups
            flowmap[crt->flowid] = uecSrc;
        }

        if (crt->size > 0) {
            uecSrc->setFlowSize(crt->size);
        }

        if (crt->trigger) {
            Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*uecSrc);
        }
        if (crt->send_done_trigger) {
            Trigger *trig =
                    conns->getTrigger(crt->send_done_trigger, eventlist);
            uecSrc->set_end_trigger(*trig);
        }

        uecSnk = new SwiftTrimmingSink();

        uecSrc->setName("uec_" + ntoa(src) + "_" + ntoa(dest));

        cout << "uec_" + ntoa(src) + "_" + ntoa(dest) << endl;
        logfile.writeName(*uecSrc);

        uecSnk->set_src(src);

        uecSnk->setName("uec_sink_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*uecSnk);
        if (crt->recv_done_trigger) {
            Trigger *trig =
                    conns->getTrigger(crt->recv_done_trigger, eventlist);
            uecSnk->set_end_trigger(*trig);
        }

        // uecRtxScanner->registerSwiftTrimming(*uecSrc);

        switch (route_strategy) {
        case ECMP_FIB:
        case ECMP_FIB_ECN:
        case ECMP_RANDOM2_ECN:
        case REACTIVE_ECN: {
            Route *srctotor = new Route();
            srctotor->push_back(
                    top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
            srctotor->push_back(
                    top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
            srctotor->push_back(
                    top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]
                            ->getRemoteEndpoint());

            Route *dsttotor = new Route();
            dsttotor->push_back(
                    top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
            dsttotor->push_back(
                    top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
            dsttotor->push_back(
                    top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]
                            ->getRemoteEndpoint());

            uecSrc->connect(srctotor, dsttotor, *uecSnk, crt->start);
            uecSrc->set_paths(number_entropies);
            uecSnk->set_paths(number_entropies);

            // register src and snk to receive packets from their respective
            // TORs.
            assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
            assert(top->switches_lp[top->HOST_POD_SWITCH(src)]);
            top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(
                    src, uecSrc->flow_id(), uecSrc);
            top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(
                    dest, uecSrc->flow_id(), uecSnk);
            break;
        }
        case SINGLE_PATH: {
            assert(route_strategy == SINGLE_PATH);
            int choice = rand() % net_paths[src][dest]->size();
            routeout = new Route(*(net_paths[src][dest]->at(choice)));
            routeout->add_endpoints(uecSrc, uecSnk);

            routein = new Route(
                    *top->get_bidir_paths(dest, src, false)->at(choice));
            routein->add_endpoints(uecSnk, uecSrc);
            uecSrc->connect(routeout, routein, *uecSnk, crt->start);
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

        path_refcounts[src][dest]--;
        path_refcounts[dest][src]--;

        // set up the triggers
        // xxx

        // free up the routes if no other connection needs them
        if (path_refcounts[src][dest] == 0 && net_paths[src][dest]) {
            vector<const Route *>::iterator i;
            for (i = net_paths[src][dest]->begin();
                 i != net_paths[src][dest]->end(); i++) {
                if ((*i)->reverse())
                    delete (*i)->reverse();
                delete *i;
            }
            delete net_paths[src][dest];
        }
        if (path_refcounts[dest][src] == 0 && net_paths[dest][src]) {
            vector<const Route *>::iterator i;
            for (i = net_paths[dest][src]->begin();
                 i != net_paths[dest][src]->end(); i++) {
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
    printf("Name Running: Swift-like\n");
    Logged::dump_idmap();
    // Record the setup
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
    for (size_t ix = 0; ix < uec_srcs.size(); ix++) {
        new_pkts += uec_srcs[ix]->_new_packets_sent;
        rtx_pkts += uec_srcs[ix]->_rtx_packets_sent;
        bounce_pkts += uec_srcs[ix]->_bounces_received;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts
         << " Bounced: " << bounce_pkts << endl;

    for (std::size_t i = 0; i < uec_srcs.size(); ++i) {
        delete uec_srcs[i];
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
