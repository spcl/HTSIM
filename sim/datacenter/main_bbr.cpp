// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "config.h"
#include <sstream>

#include "bbr.h"
#include "clock.h"
#include "compositequeue.h"
#include "connection_matrix.h"
#include "eventlist.h"
#include "fat_tree_interdc_topology.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
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
FirstFit *ff = NULL;

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
    int packet_size = 4096;
    int kmin = -1;
    int kmax = -1;
    int bts_threshold = -1;
    int seed = -1;
    int number_entropies = 256;
    queue_type queue_choice = COMPOSITE;
    bool ignore_ecn_data = true;
    BBRSrc::set_fast_drop(false);
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
    bool disable_case_4 = false;
    simtime_picosec endtime = timeFromMs(1);
    char *tm_file = NULL;
    int ratio_os_stage_1 = 1;
    bool log_tor_downqueue = false;
    bool log_tor_upqueue = false;
    bool log_switches = false;
    bool log_queue_usage = false;
    double explicit_starting_cwnd = 0;
    bool use_exp_avg_rtt = false;
    bool use_exp_avg_ecn = false;
    bool use_reps = false;
    bool ecn = false;
    mem_b ecn_low = 0, ecn_high = 0;
    char *topo_file = NULL;
    int end_time = 1000; // in microseconds
    bool topology_normal = true;
    uint64_t interdc_delay = 0;
    uint64_t max_queue_size = 0;

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
            cout << "!!currently hardcoded to 8, value will be ignored!!" << endl;
            i++;
        } else if (!strcmp(argv[i], "-nodes")) {
            no_of_nodes = atoi(argv[i + 1]);
            cout << "no_of_nodes " << no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i], "-ecn")) {
            // fraction of queuesize, between 0 and 1
            ecn = true;
            ecn_low = atoi(argv[i + 1]);
            ecn_high = atoi(argv[i + 2]);
            i += 2;
        } else if (!strcmp(argv[i], "-os_border")) {
            int os_b = atoi(argv[i + 1]);
            FatTreeInterDCTopology::set_os_ratio_border(os_b);
            i++;
        } else if (!strcmp(argv[i], "-topo")) {
            topo_file = argv[i + 1];
            cout << "FatTree topology input file: " << topo_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-cwnd")) {
            cwnd = atoi(argv[i + 1]);
            cout << "cwnd " << cwnd << endl;
            i++;
        } else if (!strcmp(argv[i], "-q")) {
            queuesize = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-use_exp_avg_ecn")) {
            use_exp_avg_ecn = atoi(argv[i + 1]);
            printf("UseExpAvgEcn: %d\n", use_exp_avg_ecn);
            i++;
        } else if (!strcmp(argv[i], "-explicit_starting_cwnd_bonus")) {
            explicit_starting_cwnd = std::stod(argv[i + 1]);
            printf("StartingWindowForcedBonus: %d\n", explicit_starting_cwnd);
            i++;
        } else if (!strcmp(argv[i], "-end")) {
            end_time = atoi(argv[i + 1]);
            cout << "endtime(us) " << end_time << endl;
            i++;
        } else if (!strcmp(argv[i], "-use_exp_avg_rtt")) {
            use_exp_avg_rtt = atoi(argv[i + 1]);
            printf("UseExpAvgRtt: %d\n", use_exp_avg_rtt);
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
            BBRSrc::set_os_ratio_stage_1(ratio_os_stage_1);
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
            PKT_SIZE_MODERN = packet_size; // Saving this for UEC reference, Bytes
            i++;
        } else if (!strcmp(argv[i], "-disable_case_3")) {
            disable_case_3 = atoi(argv[i + 1]);
            BBRSrc::set_disable_case_3(disable_case_3);
            printf("DisableCase3: %d\n", disable_case_3);
            i++;
        } else if (!strcmp(argv[i], "-disable_case_4")) {
            disable_case_4 = atoi(argv[i + 1]);
            BBRSrc::set_disable_case_4(disable_case_4);
            printf("DisableCase4: %d\n", disable_case_4);
            i++;
        } else if (!strcmp(argv[i], "-number_entropies")) {
            number_entropies = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-switch_latency")) {
            switch_latency = timeFromNs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-hop_latency")) {
            hop_latency = timeFromNs(atof(argv[i + 1]));
            LINK_DELAY_MODERN = hop_latency / 1000; // Saving this for UEC reference, ps to ns
            i++;
        } else if (!strcmp(argv[i], "-ignore_ecn_data")) {
            ignore_ecn_data = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-fast_drop")) {
            BBRSrc::set_fast_drop(atoi(argv[i + 1]));
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
            BBRSrc::set_do_jitter(do_jitter);
            printf("DoJitter: %d\n", do_jitter);
            i++;
        } else if (!strcmp(argv[i], "-do_exponential_gain")) {
            do_exponential_gain = atoi(argv[i + 1]);
            BBRSrc::set_do_exponential_gain(do_exponential_gain);
            printf("DoExpGain: %d\n", do_exponential_gain);
            i++;
        } else if (!strcmp(argv[i], "-use_fast_increase")) {
            use_fast_increase = atoi(argv[i + 1]);
            BBRSrc::set_use_fast_increase(use_fast_increase);
            printf("FastIncrease: %d\n", use_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-use_super_fast_increase")) {
            use_super_fast_increase = atoi(argv[i + 1]);
            BBRSrc::set_use_super_fast_increase(use_super_fast_increase);
            printf("FastIncreaseSuper: %d\n", use_super_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-gain_value_med_inc")) {
            gain_value_med_inc = std::stod(argv[i + 1]);
            BBRSrc::set_gain_value_med_inc(gain_value_med_inc);
            printf("GainValueMedIncrease: %f\n", gain_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-jitter_value_med_inc")) {
            jitter_value_med_inc = std::stod(argv[i + 1]);
            BBRSrc::set_jitter_value_med_inc(jitter_value_med_inc);
            printf("JitterValue: %f\n", jitter_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-delay_gain_value_med_inc")) {
            delay_gain_value_med_inc = std::stod(argv[i + 1]);
            BBRSrc::set_delay_gain_value_med_inc(delay_gain_value_med_inc);
            printf("DelayGainValue: %f\n", delay_gain_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-target_rtt_percentage_over_base")) {
            target_rtt_percentage_over_base = atoi(argv[i + 1]);
            BBRSrc::set_target_rtt_percentage_over_base(target_rtt_percentage_over_base);
            printf("TargetRTT: %d\n", target_rtt_percentage_over_base);
            i++;
        } else if (!strcmp(argv[i], "-fast_drop_rtt")) {
            BBRSrc::set_fast_drop_rtt(atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-y_gain")) {
            y_gain = std::stod(argv[i + 1]);
            BBRSrc::set_y_gain(y_gain);
            printf("YGain: %f\n", y_gain);
            i++;
        } else if (!strcmp(argv[i], "-x_gain")) {
            x_gain = std::stod(argv[i + 1]);
            BBRSrc::set_x_gain(x_gain);
            printf("XGain: %f\n", x_gain);
            i++;
        } else if (!strcmp(argv[i], "-z_gain")) {
            z_gain = std::stod(argv[i + 1]);
            BBRSrc::set_z_gain(z_gain);
            printf("ZGain: %f\n", z_gain);
            i++;
        } else if (!strcmp(argv[i], "-w_gain")) {
            w_gain = std::stod(argv[i + 1]);
            BBRSrc::set_w_gain(w_gain);
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
            BBRSrc::set_bonus_drop(bonus_drop);
            printf("BonusDrop: %f\n", bonus_drop);
            i++;
        } else if (!strcmp(argv[i], "-drop_value_buffer")) {
            drop_value_buffer = std::stod(argv[i + 1]);
            BBRSrc::set_buffer_drop(drop_value_buffer);
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
        } else if (!strcmp(argv[i], "-interdc_delay")) {
            interdc_delay = atoi(argv[i + 1]);
            interdc_delay *= 1000;
            i++;
        } else if (!strcmp(argv[i], "-max_queue_size")) {
            max_queue_size = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-topology")) {
            if (!strcmp(argv[i + 1], "normal")) {
                topology_normal = true;
            } else if (!strcmp(argv[i + 1], "interdc")) {
                topology_normal = false;
            }
            i++;
        } else if (!strcmp(argv[i], "-queue_type")) {
            if (!strcmp(argv[i + 1], "composite")) {
                queue_choice = COMPOSITE;
                BBRSrc::set_queue_type("composite");
            }
            i++;
        } else if (!strcmp(argv[i], "-algorithm")) {
            if (!strcmp(argv[i + 1], "smartt")) {
                BBRSrc::set_alogirthm("smartt");
                printf("Name Running: UEC Version A\n");
            } else if (!strcmp(argv[i + 1], "intersmartt")) {
                BBRSrc::set_alogirthm("intersmartt");
                printf("Name Running: SMaRTT\n");
            } else if (!strcmp(argv[i + 1], "mprdma")) {
                BBRSrc::set_alogirthm("mprdma");
                printf("Name Running: SMaRTT Per RTT\n");
            } else if (!strcmp(argv[i + 1], "delayC")) {
                BBRSrc::set_alogirthm("delayC");
            } else if (!strcmp(argv[i + 1], "delayD")) {
                BBRSrc::set_alogirthm("delayD");
                printf("Name Running: STrack\n");
            } else if (!strcmp(argv[i + 1], "standard_trimming")) {
                BBRSrc::set_alogirthm("standard_trimming");
                printf("Name Running: UEC Version D\n");
            } else if (!strcmp(argv[i + 1], "rtt")) {
                BBRSrc::set_alogirthm("rtt");
                printf("Name Running: SMaRTT RTT Only\n");
            } else if (!strcmp(argv[i + 1], "ecn")) {
                BBRSrc::set_alogirthm("ecn");
                printf("Name Running: SMaRTT ECN Only Constant\n");
            } else if (!strcmp(argv[i + 1], "custom")) {
                BBRSrc::set_alogirthm("custom");
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
    eventlist.setEndtime(timeFromUs((uint32_t)end_time));

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
    uint64_t base_rtt_max_hops = (hops * LINK_DELAY_MODERN) + (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * hops) +
                                 (hops * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * hops);
    uint64_t bdp_local = base_rtt_max_hops * LINK_SPEED_MODERN / 8;
    if (queue_size_ratio == 0) {
        queuesize = bdp_local; // Equal to BDP if not other info
    } else {
        queuesize = bdp_local * queue_size_ratio;
    }

    BBRSrc::setRouteStrategy(route_strategy);
    BBRSink::setRouteStrategy(route_strategy);

    BBRSrc *BBR_Src;
    BBRSink *uecSnk;

    Route *routeout, *routein;

    QueueLoggerFactory *qlf = 0;
    if (log_tor_downqueue || log_tor_upqueue) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_SAMPLING, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    } else if (log_queue_usage) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }
#ifdef FAT_TREE

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

    if (conns->N != no_of_nodes && no_of_nodes != 0) {
        cout << "Connection matrix numbefr of nodes is " << conns->N << " while I am using " << no_of_nodes << endl;
        // exit(-1);
    }

    // no_of_nodes = conns->N;
    if (ecn) {
        ecn_low = memFromPkt(ecn_low);
        ecn_high = memFromPkt(ecn_high);
        // FatTreeTopology::set_ecn_parameters(true, true, ecn_low, ecn_high);
    }
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

    // used just to print out stats data at the end
    // list <const Route*> routes;

    vector<connection *> *all_conns = conns->getAllConnections();
    vector<BBRSrc *> uec_srcs;

    map<flowid_t, TriggerTarget *> flowmap;

    FatTreeInterDCTopology *top_dc = NULL;
    FatTreeTopology *top = NULL;

    if (max_queue_size != 0) {
        queuesize = max_queue_size;
    }

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
                BBRSrc::set_interdc_delay(interdc_delay);
            } else {
                FatTreeInterDCTopology::set_interdc_delay(hop_latency);
                BBRSrc::set_interdc_delay(hop_latency);
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
        vector<BBRSrc *> uec_srcs;
        BBRSrc *uecSrc;
        BBRSink *uecSnk;

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

            // uecSrc = new BBRSrc(NULL, NULL, eventlist);
            uecSrc = new BBRSrc(NULL, NULL, eventlist, rtt, bdp, 100, 6);
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
                uecSrc->setFlowSize(crt->size);
            }

            if (crt->trigger) {
                Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                trig->add_target(*uecSrc);
            }
            if (crt->send_done_trigger) {
                Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                uecSrc->set_end_trigger(*trig);
            }

            uecSnk = new BBRSink();

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
    }

    /* for (size_t c = 0; c < all_conns->size(); c++) {
        connection *crt = all_conns->at(c);
        int src = crt->src;
        int dest = crt->dst;
        // cout << "Connection " << crt->src << "->" <<crt->dst << " starting at
        // " << crt->start << " size " << crt->size << endl;
        uint64_t rtt = BASE_RTT_MODERN * 1000;
        uint64_t bdp = BDP_MODERN_UEC;
        printf("Reaching here1\n");
        fflush(stdout);
        uint64_t actual_starting_cwnd = 0;
        uint64_t base_rtt_max_hops = (hops * LINK_DELAY_MODERN) + (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * hops) +
                                     (hops * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * hops);
        uint64_t bdp_local = base_rtt_max_hops * LINK_SPEED_MODERN / 8;

        if (starting_cwnd_ratio == 0) {
            actual_starting_cwnd = bdp_local; // Equal to BDP if not other info
        } else {
            actual_starting_cwnd = bdp_local * starting_cwnd_ratio;
        }

        if (explicit_starting_cwnd != 0) {
            BBRSrc::set_starting_cwnd(actual_starting_cwnd * explicit_starting_cwnd);
        }

        printf("Using BDP of %lu - Queue is %lld - Starting Window is %lu\n", bdp_local, queuesize,
               actual_starting_cwnd);

        BBR_Src = new BBRSrc(NULL, NULL, eventlist, rtt, bdp, 100, 6);
        BBR_Src->setReuse(1);
        BBR_Src->setIgnoreEcnAck(1);
        BBR_Src->setIgnoreEcnData(1);
        BBR_Src->setNumberEntropies(256);
        uec_srcs.push_back(BBR_Src);
        BBR_Src->set_dst(dest);
        printf("Reaching here\n");
        fflush(stdout);
        if (crt->flowid) {
            BBR_Src->set_flowid(crt->flowid);
            assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
            flowmap[crt->flowid] = BBR_Src;
        }

        if (crt->size > 0) {
            BBR_Src->setFlowSize(crt->size);
        }

        if (crt->trigger) {
            Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*BBR_Src);
        }
        if (crt->send_done_trigger) {
            Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
            BBR_Src->set_end_trigger(*trig);
        }

        uecSnk = new BBRSink();

        BBR_Src->setName("uec_" + ntoa(src) + "_" + ntoa(dest));

        cout << "uec_" + ntoa(src) + "_" + ntoa(dest) << endl;
        logfile.writeName(*BBR_Src);

        uecSnk->set_src(src);

        uecSnk->setName("uec_sink_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*uecSnk);
        if (crt->recv_done_trigger) {
            Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
            uecSnk->set_end_trigger(*trig);
        }

        // uecRtxScanner->registerUec(*BBR_Src);

        switch (route_strategy) {
        case ECMP_FIB:
        case ECMP_RANDOM2_ECN:
        case ECMP_FIB_ECN:
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
                BBR_Src->src_dc = top_dc->get_dc_id(src);
                BBR_Src->dest_dc = top_dc->get_dc_id(dest);
                BBR_Src->updateParams();

                printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                         [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                         [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                 ->getRemoteEndpoint());

                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                         [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                         [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                 ->getRemoteEndpoint());
            }

            BBR_Src->from = src;
            BBR_Src->to = dest;
            BBR_Src->connect(srctotor, dsttotor, *uecSnk, crt->start);
            BBR_Src->set_paths(number_entropies);
            uecSnk->set_paths(number_entropies);
            // eqds_src->setPaths(path_entropy_size);
            // eqds_snk->setPaths(path_entropy_size);

            // register src and snk to receive packets from their respective
            // TORs.
            if (top != NULL) {
                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, BBR_Src->flow_id(), BBR_Src);
                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, BBR_Src->flow_id(), uecSnk);
            } else {
                int idx_dc = top_dc->get_dc_id(src);
                int idx_dc_to = top_dc->get_dc_id(dest);

                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                        src % top_dc->no_of_nodes(), BBR_Src->flow_id(), BBR_Src);
                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                        dest % top_dc->no_of_nodes(), BBR_Src->flow_id(), uecSnk);
            }
            break;
        }
        default:
            abort();
        }

        // set up the triggers
        // xxx
    } */

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
    for (size_t ix = 0; ix < uec_srcs.size(); ix++) {
        new_pkts += uec_srcs[ix]->_new_packets_sent;
        rtx_pkts += uec_srcs[ix]->_rtx_packets_sent;
        bounce_pkts += uec_srcs[ix]->_bounces_received;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << " Bounced: " << bounce_pkts << endl;

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