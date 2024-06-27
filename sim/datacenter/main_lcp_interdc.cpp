// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "config.h"
#include "network.h"
#include "queue_lossless_input.h"
#include "randomqueue.h"
#include <iostream>
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
#include "pipe.h"
#include "shortflows.h"
#include "topology.h"
#include "uec.h"
#include <filesystem>
//#include "vl2_topology.h"

// Fat Tree topology was modified to work with this script, others won't work
// correctly
#include "fat_tree_interdc_topology.h"
//#include "oversubscribed_fat_tree_topology.h"
//#include "multihomed_fat_tree_topology.h"
//#include "star_topology.h"
//#include "bcube_topology.h"
#include <list>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

// int RTT = 10; // this is per link delay; identical RTT microseconds = 0.02 ms
uint32_t RTT = 400; // this is per link delay in ns; identical RTT microseconds
                    // = 0.02 ms
int DEFAULT_NODES = 128;
#define DEFAULT_QUEUE_SIZE                                                     \
    100000000 // ~100MB, just a large value so we can ignore queues
// int N=128;

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
    for (unsigned int i = 1; i < rt->size() - 1; i += 2) {
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
    UecSrc::set_fast_drop(false);
    bool do_jitter = false;
    bool do_exponential_gain = false;
    bool use_fast_increase = false;
    double gain_value_med_inc = 1;
    double jitter_value_med_inc = 1;
    double delay_gain_value_med_inc = 5;
    int target_rtt_percentage_over_base = 50;
    bool collect_data = false;
    int fat_tree_k = 4; // 1:1 default
    COLLECT_DATA = collect_data;
    bool use_super_fast_increase = false;
    double y_gain = 1;
    double x_gain = 0.15;
    double z_gain = 1;
    double w_gain = 1;
    double bonus_drop = 1;
    double drop_value_buffer = 1;
    double starting_cwnd_ratio = 0;
    uint64_t explicit_starting_cwnd = 0;
    uint64_t explicit_starting_buffer = 0;
    uint64_t explicit_base_rtt = 0;
    uint64_t explicit_target_rtt = 0;
    uint64_t explicit_bdp = 0;
    double queue_size_ratio = 0;
    bool disable_case_3 = false;
    bool disable_case_4 = false;
    simtime_picosec endtime = timeFromMs(1.2);
    char *tm_file = NULL;
    int ratio_os_stage_1 = 4;
    bool log_tor_downqueue = false;
    bool log_tor_upqueue = false;
    bool log_switches = false;
    bool log_queue_usage = false;
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
            UecSrc::set_os_ratio_stage_1(ratio_os_stage_1);
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
            UecSrc::set_disable_case_3(disable_case_3);
            printf("DisableCase3: %d\n", disable_case_3);
            i++;
        } else if (!strcmp(argv[i], "-disable_case_4")) {
            disable_case_4 = atoi(argv[i + 1]);
            UecSrc::set_disable_case_4(disable_case_4);
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
            LINK_DELAY_MODERN = hop_latency /
                                1000; // Saving this for UEC reference, ps to ns
            i++;
        } else if (!strcmp(argv[i], "-ignore_ecn_data")) {
            ignore_ecn_data = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-fast_drop")) {
            UecSrc::set_fast_drop(atoi(argv[i + 1]));
            printf("FastDrop: %d\n", atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-seed")) {
            seed = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-interdc_delay")) {
            interdc_delay = atoi(argv[i + 1]);
            interdc_delay *= 1000;
            i++;
        } else if (!strcmp(argv[i], "-collect_data")) {
            collect_data = atoi(argv[i + 1]);
            COLLECT_DATA = collect_data;
            i++;
        } else if (!strcmp(argv[i], "-do_jitter")) {
            do_jitter = atoi(argv[i + 1]);
            UecSrc::set_do_jitter(do_jitter);
            printf("DoJitter: %d\n", do_jitter);
            i++;
        } else if (!strcmp(argv[i], "-do_exponential_gain")) {
            do_exponential_gain = atoi(argv[i + 1]);
            UecSrc::set_do_exponential_gain(do_exponential_gain);
            printf("DoExpGain: %d\n", do_exponential_gain);
            i++;
        } else if (!strcmp(argv[i], "-use_fast_increase")) {
            use_fast_increase = atoi(argv[i + 1]);
            UecSrc::set_use_fast_increase(use_fast_increase);
            printf("FastIncrease: %d\n", use_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-use_super_fast_increase")) {
            use_super_fast_increase = atoi(argv[i + 1]);
            UecSrc::set_use_super_fast_increase(use_super_fast_increase);
            printf("FastIncreaseSuper: %d\n", use_super_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-gain_value_med_inc")) {
            gain_value_med_inc = std::stod(argv[i + 1]);
            // UecSrc::set_gain_value_med_inc(gain_value_med_inc);
            printf("GainValueMedIncrease: %f\n", gain_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-jitter_value_med_inc")) {
            jitter_value_med_inc = std::stod(argv[i + 1]);
            // UecSrc::set_jitter_value_med_inc(jitter_value_med_inc);
            printf("JitterValue: %f\n", jitter_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-delay_gain_value_med_inc")) {
            delay_gain_value_med_inc = std::stod(argv[i + 1]);
            // UecSrc::set_delay_gain_value_med_inc(delay_gain_value_med_inc);
            printf("DelayGainValue: %f\n", delay_gain_value_med_inc);
            i++;
        } else if (!strcmp(argv[i], "-target_rtt_percentage_over_base")) {
            target_rtt_percentage_over_base = atoi(argv[i + 1]);
            UecSrc::set_target_rtt_percentage_over_base(
                    target_rtt_percentage_over_base);
            printf("TargetRTT: %d\n", target_rtt_percentage_over_base);
            i++;
        } else if (!strcmp(argv[i], "-fast_drop_rtt")) {
            UecSrc::set_fast_drop_rtt(atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-y_gain")) {
            y_gain = std::stod(argv[i + 1]);
            UecSrc::set_y_gain(y_gain);
            printf("YGain: %f\n", y_gain);
            i++;
        } else if (!strcmp(argv[i], "-x_gain")) {
            x_gain = std::stod(argv[i + 1]);
            UecSrc::set_x_gain(x_gain);
            printf("XGain: %f\n", x_gain);
            i++;
        } else if (!strcmp(argv[i], "-z_gain")) {
            z_gain = std::stod(argv[i + 1]);
            UecSrc::set_z_gain(z_gain);
            printf("ZGain: %f\n", z_gain);
            i++;
        } else if (!strcmp(argv[i], "-w_gain")) {
            w_gain = std::stod(argv[i + 1]);
            UecSrc::set_w_gain(w_gain);
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
            UecSrc::set_bonus_drop(bonus_drop);
            printf("BonusDrop: %f\n", bonus_drop);
            i++;
        } else if (!strcmp(argv[i], "-phantom_in_series")) {
            CompositeQueue::set_use_phantom_in_series();
            printf("PhantomQueueInSeries: %d\n", 1);
            i++;
        } else if (!strcmp(argv[i], "-drop_value_buffer")) {
            drop_value_buffer = std::stod(argv[i + 1]);
            UecSrc::set_buffer_drop(drop_value_buffer);
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
        } else if (!strcmp(argv[i], "-queue_type")) {
            if (!strcmp(argv[i + 1], "composite")) {
                queue_choice = COMPOSITE;
                UecSrc::set_queue_type("composite");
            } else if (!strcmp(argv[i + 1], "composite_bts")) {
                queue_choice = COMPOSITE_BTS;
                UecSrc::set_queue_type("composite_bts");
                printf("Name Running: UEC BTS\n");
            }
            i++;
        } else if (!strcmp(argv[i], "-algorithm")) {
            if (!strcmp(argv[i + 1], "delayA")) {
                UecSrc::set_alogirthm("delayA");
                printf("Name Running: UEC Version A\n");
            } else if (!strcmp(argv[i + 1], "delayB")) {
                UecSrc::set_alogirthm("delayB");
                printf("Name Running: SMaRTT\n");
            } else if (!strcmp(argv[i + 1], "delayB_rtt")) {
                UecSrc::set_alogirthm("delayB_rtt");
                printf("Name Running: SMaRTT Per RTT\n");
            } else if (!strcmp(argv[i + 1], "delayC")) {
                UecSrc::set_alogirthm("delayC");
            } else if (!strcmp(argv[i + 1], "delayD")) {
                UecSrc::set_alogirthm("delayD");
                printf("Name Running: STrack\n");
            } else if (!strcmp(argv[i + 1], "standard_trimming")) {
                UecSrc::set_alogirthm("standard_trimming");
                printf("Name Running: UEC Version D\n");
            } else if (!strcmp(argv[i + 1], "rtt")) {
                UecSrc::set_alogirthm("rtt");
                printf("Name Running: SMaRTT RTT Only\n");
            } else if (!strcmp(argv[i + 1], "ecn")) {
                UecSrc::set_alogirthm("ecn");
                printf("Name Running: SMaRTT ECN Only Constant\n");
            } else if (!strcmp(argv[i + 1], "custom")) {
                UecSrc::set_alogirthm("custom");
                printf("Name Running: SMaRTT ECN Only Variable\n");
            } else if (!strcmp(argv[i + 1], "lcp")) {
                UecSrc::set_alogirthm("lcp");
                printf("Name Running: LCP\n");
            } else if (!strcmp(argv[i + 1], "lcp-gemini")) {
                UecSrc::set_alogirthm("lcp-gemini");
                printf("Name Running: LCP Gemini\n");
            } else {
                printf("Unknown algorithm exiting...\n");
                exit(-1);
            } 
            i++;
        } else if (!strcmp(argv[i], "-target-low-us")) {
            TARGET_RTT_LOW = timeFromUs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-target-high-us")) {
            TARGET_RTT_HIGH = timeFromUs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-baremetal-us")) {
            BAREMETAL_RTT = timeFromUs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-alpha")) {
            LCP_ALPHA = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-beta")) {
            LCP_BETA = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-gamma")) {
            LCP_GAMMA = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-delta")) {
            LCP_DELTA = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-k")) {
            LCP_K = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-pacing-bonus")) {
            LCP_PACING_BONUS = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-fast-increase-threshold")) {
            LCP_FAST_INCREASE_THRESHOLD = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-no-qa")) {
            LCP_USE_QUICK_ADAPT = false;
        } else if (!strcmp(argv[i], "-no-fi")) {
            LCP_USE_FAST_INCREASE = false;
        } else if (!strcmp(argv[i], "-no-pacing")) {
            LCP_USE_PACING = false;
        } else if (!strcmp(argv[i], "-use-min")) {
            LCP_USE_MIN_RTT = true;
        }  else if (!strcmp(argv[i], "-use-ad")) {
            LCP_USE_AGGRESSIVE_DECREASE = true;
        }  else {
            printf("Called with %s\n", argv[i]);
            exit_error(argv[0]);
        }

        i++;
    }

//     SINGLE_PKT_TRASMISSION_TIME_MODERN = packet_size * 8 / (LINK_SPEED_MODERN);
//     // exit(1);

//     // Initialize Seed, Logging and Other variables
//     if (seed != -1) {
//         srand(seed);
//         srandom(seed);
//     } else {
//         srand(time(NULL));
//         srandom(time(NULL));
//     }
//     Packet::set_packet_size(packet_size);
//     initializeLoggingFolders();

//     srand(seed);
//     srandom(seed);
//     cout << "Parsed args\n";

//     eventlist.setEndtime(timeFromUs((uint32_t)endtime));
//     queuesize = memFromPkt(queuesize);
//     // prepare the loggers

//     cout << "Logging to " << filename.str() << endl;
//     // Logfile
//     Logfile logfile(filename.str(), eventlist);

//     cout << "Linkspeed set to " << linkspeed / 1000000000 << "Gbps" << endl;
//     logfile.setStartTime(timeFromSec(0));

// #if PRINT_PATHS
//     filename << ".paths";
//     cout << "Logging path choices to " << filename.str() << endl;
//     std::ofstream paths(filename.str().c_str());
//     if (!paths) {
//         cout << "Can't open for writing paths file!" << endl;
//         exit(1);
//     }
// #endif

//     // Calculate Network Info
//     // TODO: Fix this is probably wrong. Need to determine what the hops are (probably plus one) and add the interdc_delay.
//     int hops = 6; // hardcoded for now
//     uint64_t base_rtt_max_hops =
//             (hops * LINK_DELAY_MODERN) +
//             (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * hops) +
//             (hops * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * hops) + 3898;

//     uint64_t bdp_local = base_rtt_max_hops * LINK_SPEED_MODERN / 8;
//     if (queue_size_ratio == 0) {
//         queuesize = bdp_local; // Equal to BDP if not other info
//     } else {
//         queuesize = bdp_local * queue_size_ratio;
//     }
//     queuesize = bdp_local * 0.2;

//     if (LCP_DELTA == 1) {
//         LCP_DELTA = bdp_local * 0.05;
//     }

//     BAREMETAL_RTT = base_rtt_max_hops * 1000;
//     TARGET_RTT_LOW = BAREMETAL_RTT * 1.05;
//     TARGET_RTT_HIGH = BAREMETAL_RTT * 1.1;

//     // LCP_GEMINI_BETA = 0.2;
//     // LCP_GEMINI_TARGET_QUEUEING_LATENCY = (LCP_GEMINI_BETA / (1.0 - LCP_GEMINI_BETA)) * BAREMETAL_RTT;

//     LCP_GEMINI_TARGET_QUEUEING_LATENCY = 0.1 * BAREMETAL_RTT;
//     LCP_GEMINI_BETA = (double)LCP_GEMINI_TARGET_QUEUEING_LATENCY / ((double) LCP_GEMINI_TARGET_QUEUEING_LATENCY + (double) BAREMETAL_RTT);

//     double H = 1.2 * pow(10, -7);
//     cout << "Double of H: " << H * (double) bdp_local << endl;
//     LCP_GEMINI_H = max(min((H * (double) bdp_local), 5.0), 0.1) * (double) PKT_SIZE_MODERN;
//     if (LCP_GEMINI_H == 0) {
//         cout << "H is 0, raw value is: " << H * (double) bdp_local << " exiting..." << endl;
//         exit(-1);
//     }

//     if (route_strategy == NOT_SET) {
//         fprintf(stderr, "Route Strategy not set.  Use the -strat param.  "
//                         "\nValid values are perm, rand, pull, rg and single\n");
//         exit(1);
//     }

    // cout << "==============================" << endl;
    // cout << "Link speed: " << LINK_SPEED_MODERN << " GBps" << endl;
    // cout << "Baremetal RTT: " << BAREMETAL_RTT / 1000000 << " us" << endl;
    // cout << "Target RTT Low: " << TARGET_RTT_LOW / 1000000 << " us" << endl;
    // cout << "Target RTT High: " << TARGET_RTT_HIGH / 1000000 << " us" << endl;
    // cout << "MSS: " << PKT_SIZE_MODERN << " Bytes" << endl;
    // cout << "BDP: " << bdp_local / 1000 << " KB" << endl;
    // cout << "Queue Size: " << queuesize << " Bytes" << endl;
    // cout << "Delta: " << LCP_DELTA << endl;
    // cout << "Beta: " << LCP_BETA << endl;
    // cout << "Alpha: " << LCP_ALPHA << endl;
    // cout << "Gamma: " << LCP_GAMMA << endl;
    // cout << "K: " << LCP_K << endl;
    // cout << "Fast Increase Threshold: " << LCP_FAST_INCREASE_THRESHOLD << endl;
    // cout << "Use Quick Adapt: " << LCP_USE_QUICK_ADAPT << endl;
    // cout << "Use Pacing: " << LCP_USE_PACING << endl;
    // cout << "Use Fast Increase: " << LCP_USE_FAST_INCREASE << endl;
    // cout << "Pacing Bonus: " << LCP_PACING_BONUS << endl;
    // cout << "Use Min RTT: " << LCP_USE_MIN_RTT << endl;
    // cout << "Use Aggressive Decrease: " << LCP_USE_AGGRESSIVE_DECREASE << endl;
    // cout << "Gemini Queueing Delay Threshold: " << LCP_GEMINI_TARGET_QUEUEING_LATENCY / 1000000 << " us" << endl;
    // cout << "Gemini Beta: " << LCP_GEMINI_BETA << endl;
    // cout << "Gemini H: " << LCP_GEMINI_H << endl;
    // cout << "==============================" << endl;


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

    // Routing
    // float ar_sticky_delta = 10;
    // FatTreeSwitch::sticky_choices ar_sticky = FatTreeSwitch::PER_PACKET;
    // atTreeSwitch::_ar_sticky = ar_sticky;
    // FatTreeSwitch::_sticky_delta = timeFromUs(ar_sticky_delta);

    if (route_strategy == NOT_SET) {
        fprintf(stderr, "Route Strategy not set.  Use the -strat param.  "
                        "\nValid values are perm, rand, pull, rg and single\n");
        exit(1);
    }

    // Calculate Network Info
    int hops = 6; // hardcoded for now
    uint64_t actual_starting_cwnd = 0;
    uint64_t base_rtt_max_hops =
            (hops * LINK_DELAY_MODERN) +
            (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * hops) +
            (hops * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * hops);
    uint64_t bdp_local = base_rtt_max_hops * LINK_SPEED_MODERN / 8;

    if (starting_cwnd_ratio == 0) {
        actual_starting_cwnd = bdp_local; // Equal to BDP if not other info
    } else {
        actual_starting_cwnd = bdp_local * starting_cwnd_ratio;
    }
    if (queue_size_ratio == 0) {
        queuesize = bdp_local; // Equal to BDP if not other info
    } else {
        queuesize = bdp_local * queue_size_ratio;
    }

    if (explicit_starting_buffer != 0) {
        queuesize = explicit_starting_buffer;
    }
    if (explicit_starting_cwnd != 0) {
        actual_starting_cwnd = explicit_starting_cwnd;
        UecSrc::set_explicit_bdp(explicit_bdp);
    }

    UecSrc::set_starting_cwnd(actual_starting_cwnd);
    if (max_queue_size != 0) {
        queuesize = max_queue_size;
        UecSrc::set_switch_queue_size(max_queue_size);
    }

    printf("Using BDP of %lu - Queue is %lld - Starting Window is %lu - RTT "
           "%lu - Bandwidth %lu\n",
           bdp_local, queuesize, actual_starting_cwnd, base_rtt_max_hops,
           LINK_SPEED_MODERN);

    cout << "Using subflow count " << subflow_count << endl;

    if (LCP_DELTA == 1) {
        LCP_DELTA = bdp_local * 0.05;
    }

    BAREMETAL_RTT = base_rtt_max_hops * 1000;
    TARGET_RTT_LOW = BAREMETAL_RTT * 1.05;
    TARGET_RTT_HIGH = BAREMETAL_RTT * 1.1;
    // LCP_GEMINI_BETA = 0.2;
    // LCP_GEMINI_TARGET_QUEUEING_LATENCY = (LCP_GEMINI_BETA / (1.0 - LCP_GEMINI_BETA)) * BAREMETAL_RTT;
    LCP_GEMINI_TARGET_QUEUEING_LATENCY = 0.1 * BAREMETAL_RTT;
    LCP_GEMINI_BETA = (double)LCP_GEMINI_TARGET_QUEUEING_LATENCY / ((double) LCP_GEMINI_TARGET_QUEUEING_LATENCY + (double) BAREMETAL_RTT);

    double H = 1.2 * pow(10, -7);
    LCP_GEMINI_H = max(min((H * (double) bdp_local), 5.0), 0.1) * (double) PKT_SIZE_MODERN;
    if (LCP_GEMINI_H == 0) {
        cout << "H is 0, raw value is: " << H * (double) bdp_local << " exiting..." << endl;
        exit(-1);
    }

    cout << "==============================" << endl;
    cout << "Link speed: " << LINK_SPEED_MODERN << " GBps" << endl;
    cout << "Baremetal RTT: " << BAREMETAL_RTT / 1000000 << " us" << endl;
    cout << "Target RTT Low: " << TARGET_RTT_LOW / 1000000 << " us" << endl;
    cout << "Target RTT High: " << TARGET_RTT_HIGH / 1000000 << " us" << endl;
    cout << "MSS: " << PKT_SIZE_MODERN << " Bytes" << endl;
    cout << "BDP: " << bdp_local / 1000 << " KB" << endl;
    cout << "Queue Size: " << queuesize << " Bytes" << endl;
    cout << "Delta: " << LCP_DELTA << endl;
    cout << "Beta: " << LCP_BETA << endl;
    cout << "Alpha: " << LCP_ALPHA << endl;
    cout << "Gamma: " << LCP_GAMMA << endl;
    cout << "K: " << LCP_K << endl;
    cout << "Fast Increase Threshold: " << LCP_FAST_INCREASE_THRESHOLD << endl;
    cout << "Use Quick Adapt: " << LCP_USE_QUICK_ADAPT << endl;
    cout << "Use Pacing: " << LCP_USE_PACING << endl;
    cout << "Use Fast Increase: " << LCP_USE_FAST_INCREASE << endl;
    cout << "Pacing Bonus: " << LCP_PACING_BONUS << endl;
    cout << "Use Min RTT: " << LCP_USE_MIN_RTT << endl;
    cout << "Use Aggressive Decrease: " << LCP_USE_AGGRESSIVE_DECREASE << endl;
    cout << "Gemini Queueing Delay Threshold: " << LCP_GEMINI_TARGET_QUEUEING_LATENCY / 1000000 << " us" << endl;
    cout << "Gemini Beta: " << LCP_GEMINI_BETA << endl;
    cout << "Gemini H: " << LCP_GEMINI_H << endl;
    cout << "==============================" << endl;

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

    lg = &logfile;

    logfile.setStartTime(timeFromSec(0));

    // UecLoggerSimple uecLogger;
    // logfile.addLogger(uecLogger);
    TrafficLoggerSimple traffic_logger = TrafficLoggerSimple();
    logfile.addLogger(traffic_logger);

    // UecSrc *uecSrc;
    // UecSink *uecSink;

    UecSrc::setRouteStrategy(route_strategy);
    UecSink::setRouteStrategy(route_strategy);

    // Route *routeout, *routein;
    // double extrastarttime;

    int dest;

#if USE_FIRST_FIT
    if (subflow_count == 1) {
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL), eventlist);
    }
#endif

#ifdef FAT_TREE
#endif

#ifdef FAT_TREE_INTERDC_TOPOLOGY_H

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

#if USE_FIRST_FIT
    if (ff)
        ff->net_paths = net_paths;
#endif

    map<int, vector<int> *>::iterator it;

    // used just to print out stats data at the end
    list<const Route *> routes;

    int connID = 0;
    dest = 1;
    // int receiving_node = 127;
    vector<int> subflows_chosen;

    printf("Starting LGS Interface");
    LogSimInterface *lgs;
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
    FatTreeInterDCTopology::set_ecn_thresholds_as_queue_percentage(kmin,
                                                                    kmax);
    FatTreeInterDCTopology::set_bts_threshold(bts_threshold);
    FatTreeInterDCTopology::set_ignore_data_ecn(ignore_ecn_data);
    FatTreeInterDCTopology *top = new FatTreeInterDCTopology(
            no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff,
            queue_choice, hop_latency, switch_latency);
    lgs = new LogSimInterface(NULL, &traffic_logger, eventlist, top, NULL);

    lgs->set_protocol(UEC_PROTOCOL);
    lgs->set_cwd(cwnd);
    lgs->set_queue_size(queuesize);
    // lgs->setNumberEntropies(number_entropies);
    lgs->setIgnoreEcnData(ignore_ecn_data);
    lgs->setNumberPaths(number_entropies);
    start_lgs(goal_filename, *lgs);

    /*for (int src = 0; src < dest; ++src) {
        connID++;
        if (!net_paths[src][dest]) {
            vector<const Route *> *paths = top->get_paths(src, dest);
            net_paths[src][dest] = paths;
            for (unsigned int i = 0; i < paths->size(); i++) {
                routes.push_back((*paths)[i]);
            }
        }
        if (!net_paths[dest][src]) {
            vector<const Route *> *paths = top->get_paths(dest, src);
            net_paths[dest][src] = paths;
        }
    }*/

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC * CORE_TO_HOST) +
                  " pkt/sec");
    // logfile.write("# buffer = " + ntoa((double)
    // (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
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
        // print_route(*r);
        cout << "Path:" << endl;
        hop = 0;
        for (std::size_t i = 0; i < r->size(); i++) {
            PacketSink *ps = r->at(i);
            CompositeQueue *q = dynamic_cast<CompositeQueue *>(ps);
            if (q == 0) {
                cout << ps->nodename() << endl;
            } else {
                cout << q->nodename() << " id=" << 0 /*q->id*/ << " "
                     << q->num_packets() << "pkts " << q->num_headers()
                     << "hdrs " << q->num_acks() << "acks " << q->num_nacks()
                     << "nacks " << q->num_stripped() << "stripped"
                     << endl; // TODO(tommaso): compositequeues don't have id.
                              // Need to add that or find an alternative way.
                              // Verify also that compositequeue is the right
                              // queue to use here.
                counts[hop] += q->num_stripped();
                hop++;
            }
        }
        cout << endl;
    }
    for (int i = 0; i < 10; i++)
        cout << "Hop " << i << " Count " << counts[i] << endl;
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