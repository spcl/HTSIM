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
extern void tokenize(string const &str, const char delim, vector<string> &out);

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
uint64_t FatTreeInterDCTopology::_interdc_delay = 0;
int FatTreeInterDCTopology::os_ratio_border = 1;
//  extern int N;
simtime_picosec FatTreeInterDCTopology::_link_latencies[] = {0, 0, 0};
simtime_picosec FatTreeInterDCTopology::_switch_latencies[] = {0, 0, 0};
uint32_t FatTreeInterDCTopology::_hosts_per_pod = 0;
uint32_t FatTreeInterDCTopology::_radix_up[] = {0, 0};
uint32_t FatTreeInterDCTopology::_radix_down[] = {0, 0, 0};
mem_b FatTreeInterDCTopology::_queue_up[] = {0, 0};
mem_b FatTreeInterDCTopology::_queue_down[] = {0, 0, 0};
uint32_t FatTreeInterDCTopology::_bundlesize[] = {1, 1, 1};
uint32_t FatTreeInterDCTopology::_oversub[] = {1, 1, 1};
linkspeed_bps FatTreeInterDCTopology::_downlink_speeds[] = {0, 0, 0};

void FatTreeInterDCTopology::set_tier_parameters(int tier, int radix_up, int radix_down, mem_b queue_up,
                                                 mem_b queue_down, int bundlesize, linkspeed_bps linkspeed,
                                                 int oversub) {
    // tier is 0 for ToR, 1 for agg switch, 2 for core switch
    if (tier < CORE_TIER) {
        // no uplinks from core switches
        _radix_up[tier] = radix_up;
        _queue_up[tier] = queue_up;
    }
    _radix_down[tier] = radix_down;
    _queue_down[tier] = queue_down;
    _bundlesize[tier] = bundlesize;
    _downlink_speeds[tier] = linkspeed; // this is the link going downwards from this tier.
                                        // up/down linkspeeds are symmetric.
    _oversub[tier] = oversub;
    // xxx what to do about queue sizes
}

// load a config file and use it to create a FatTreeInterDCTopology
FatTreeInterDCTopology *FatTreeInterDCTopology::load(const char *filename, QueueLoggerFactory *logger_factory,
                                                     EventList &eventlist, mem_b queuesize, queue_type q_type,
                                                     queue_type sender_q_type) {
    std::ifstream file(filename);
    if (file.is_open()) {
        FatTreeInterDCTopology *ft = load(file, logger_factory, eventlist, queuesize, q_type, sender_q_type);
        file.close();
        ft->_queuesize = queuesize;
        return ft;
    } else {
        cerr << "Failed to open FatTree config file " << filename << endl;
        exit(1);
    }
}

// in-place conversion to lower case
void to_lower(string &s) {
    string::iterator i;
    for (i = s.begin(); i != s.end(); i++) {
        *i = std::tolower(*i);
    }
    // std::transform(s.begin(), s.end(), s.begin(),
    //[](unsigned char c){ return std::tolower(c); });
}

FatTreeInterDCTopology *FatTreeInterDCTopology::load(istream &file, QueueLoggerFactory *logger_factory,
                                                     EventList &eventlist, mem_b queuesize, queue_type q_type,
                                                     queue_type sender_q_type) {
    // cout << "topo load start\n";
    std::string line;
    int linecount = 0;
    int no_of_nodes = 0;
    _tiers = 0;
    _hosts_per_pod = 0;
    for (int tier = 0; tier < 3; tier++) {
        _queue_down[tier] = queuesize;
        if (tier != 2)
            _queue_up[tier] = queuesize;
    }
    while (std::getline(file, line)) {
        linecount++;
        vector<string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() == 0)
            continue;
        if (tokens[0][0] == '#') {
            continue;
        }
        to_lower(tokens[0]);
        if (tokens[0] == "nodes") {
            no_of_nodes = stoi(tokens[1]);
        } else if (tokens[0] == "tiers") {
            _tiers = stoi(tokens[1]);
        } else if (tokens[0] == "podsize") {
            _hosts_per_pod = stoi(tokens[1]);
        } else if (tokens[0] == "tier") {
            // we're done with the header
            break;
        }
    }
    if (no_of_nodes == 0) {
        cerr << "Missing number of nodes in header" << endl;
        exit(1);
    }
    if (_tiers == 0) {
        cerr << "Missing number of tiers in header" << endl;
        exit(1);
    }
    if (_tiers < 2 || _tiers > 3) {
        cerr << "Invalid number of tiers: " << _tiers << endl;
        exit(1);
    }
    if (_hosts_per_pod == 0) {
        cerr << "Missing pod size in header" << endl;
        exit(1);
    }
    linecount--;
    bool tiers_done[3] = {false, false, false};
    int current_tier = -1;
    do {
        linecount++;
        vector<string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() < 1) {
            continue;
        }
        to_lower(tokens[0]);
        if (tokens.size() == 0 || tokens[0][0] == '#') {
            continue;
        } else if (tokens[0] == "tier") {
            current_tier = stoi(tokens[1]);
            if (current_tier < 0 || current_tier > 2) {
                cerr << "Invalid tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            tiers_done[current_tier] = true;
        } else if (tokens[0] == "downlink_speed_gbps") {
            if (_downlink_speeds[current_tier] != 0) {
                cerr << "Duplicate linkspeed setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _downlink_speeds[current_tier] = ((linkspeed_bps)stoi(tokens[1])) * 1000000000;
            printf("Downlink speed is %f\n", speedAsGbps(_downlink_speeds[current_tier]));
        } else if (tokens[0] == "radix_up") {
            if (_radix_up[current_tier] != 0) {
                cerr << "Duplicate radix_up setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            if (current_tier == 2) {
                cerr << "Can't specific radix_up for tier " << current_tier << " at line " << linecount
                     << " (no uplinks from top tier!)" << endl;
                exit(1);
            }
            _radix_up[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "radix_down") {
            if (_radix_down[current_tier] != 0) {
                cerr << "Duplicate radix_down setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _radix_down[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "queue_up") {
            if (_queue_up[current_tier] != 0) {
                cerr << "Duplicate queue_up setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            if (current_tier == 2) {
                cerr << "Can't specific queue_up for tier " << current_tier << " at line " << linecount
                     << " (no uplinks from top tier!)" << endl;
                exit(1);
            }
            _queue_up[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "queue_down") {
            if (_queue_down[current_tier] != 0) {
                cerr << "Duplicate queue_down setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _queue_down[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "oversubscribed") {
            if (_oversub[current_tier] != 1) {
                cerr << "Duplicate oversubscribed setting for tier " << current_tier << " at line " << linecount
                     << endl;
                exit(1);
            }
            _oversub[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "bundle") {
            if (_bundlesize[current_tier] != 1) {
                cerr << "Duplicate bundle size setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _bundlesize[current_tier] = stoi(tokens[1]);
        } else if (tokens[0] == "switch_latency_ns") {
            if (_switch_latencies[current_tier] != 0) {
                cerr << "Duplicate switch_latency setting for tier " << current_tier << " at line " << linecount
                     << endl;
                exit(1);
            }
            _switch_latencies[current_tier] = timeFromNs(stoi(tokens[1]));
        } else if (tokens[0] == "downlink_latency_ns") {
            if (_link_latencies[current_tier] != 0) {
                cerr << "Duplicate link latency setting for tier " << current_tier << " at line " << linecount << endl;
                exit(1);
            }
            _link_latencies[current_tier] = timeFromNs(stoi(tokens[1]));
        } else {
            cerr << "Error: Unknown attribute " << tokens[0] << " at line " << linecount << endl;
            cerr << "Allowed attributes are: tier, downlink_speed_gbps, "
                    "radix_up, radix_down, queue_up, queue_down, "
                    "oversubscribed, bundle, switch_latency_ns, "
                    "downlink_latency_ns"
                 << endl;
            exit(1);
        }
    } while (std::getline(file, line));
    for (uint32_t tier = 0; tier < _tiers; tier++) {
        if (tiers_done[tier] == false) {
            cerr << "No configuration found for tier " << tier << endl;
            exit(1);
        }
        if (_downlink_speeds[tier] == 0) {
            cerr << "Missing downlink_speed_gbps for tier " << tier << endl;
            exit(1);
        }
        if (_link_latencies[tier] == 0) {
            cerr << "Missing downlink_latency_ns for tier " << tier << endl;
            exit(1);
        }
        if (tier < (_tiers - 1) && _radix_up[tier] == 0) {
            cerr << "Missing radix_up for tier " << tier << endl;
            exit(1);
        }
        if (_radix_down[tier] == 0) {
            cerr << "Missing radix_down for tier " << tier << endl;
            exit(1);
        }
        if (tier < (_tiers - 1) && _queue_up[tier] == 0) {
            cerr << "Missing queue_up for tier " << tier << endl;
            exit(1);
        }
        if (_queue_down[tier] == 0) {
            cerr << "Missing queue_down for tier " << tier << endl;
            exit(1);
        }
    }

    cout << "Topology load done\n";
    FatTreeInterDCTopology *ft = new FatTreeInterDCTopology(no_of_nodes, 0, 0, logger_factory, &eventlist, NULL, q_type,
                                                            0, 0, sender_q_type);
    cout << "FatTree constructor done, " << ft->no_of_nodes() << " nodes created\n";
    return ft;
}

FatTreeInterDCTopology::FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
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

    cout << "Fat Tree topology with " << timeAsUs(_hop_latency) << "us links and " << timeAsUs(_switch_latency)
         << "us switching latency." << endl;
    set_params(no_of_nodes);

    init_network();
}

FatTreeInterDCTopology::FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
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

FatTreeInterDCTopology::FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
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

FatTreeInterDCTopology::FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
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

void FatTreeInterDCTopology::set_linkspeeds(linkspeed_bps linkspeed) {
    if (linkspeed != 0 && _downlink_speeds[TOR_TIER] != 0) {
        cerr << "Don't set linkspeeds using both the constructor and "
                "set_tier_parameters - use only one of the two\n";
        exit(1);
    }
    if (linkspeed == 0 && _downlink_speeds[TOR_TIER] == 0) {
        cerr << "Linkspeed is not set, either as a default or by constructor\n";
        exit(1);
    }
    // set tier linkspeeds if no defaults are specified
    if (_downlink_speeds[TOR_TIER] == 0) {
        _downlink_speeds[TOR_TIER] = linkspeed;
    }
    if (_downlink_speeds[AGG_TIER] == 0) {
        _downlink_speeds[AGG_TIER] = linkspeed;
    }
    if (_downlink_speeds[CORE_TIER] == 0) {
        _downlink_speeds[CORE_TIER] = linkspeed;
    }
}

void FatTreeInterDCTopology::set_queue_sizes(mem_b queuesize) {
    if (queuesize != 0) {
        // all tiers use the same queuesize
        for (int tier = TOR_TIER; tier <= CORE_TIER; tier++) {
            _queue_down[tier] = queuesize;
            if (tier != CORE_TIER)
                _queue_up[tier] = queuesize;
        }
    } else {
        // the tier queue sizes must have already been set
        assert(_queue_down[TOR_TIER] != 0);
    }
}

void FatTreeInterDCTopology::set_custom_params(uint32_t no_of_nodes) {
    // cout << "set_custom_params" << endl;
    //  do some sanity checking before we proceed
    assert(_hosts_per_pod > 0);

    // check bundlesizes are feasible with switch radix
    for (uint32_t tier = TOR_TIER; tier < _tiers; tier++) {
        if (_radix_down[tier] == 0) {
            cerr << "Custom topology, but radix_down not set for tier " << tier << endl;
            exit(1);
        }
        if (_radix_down[tier] % _bundlesize[tier] != 0) {
            cerr << "Mismatch between tier " << tier << " down radix of " << _radix_down[tier] << " and bundlesize "
                 << _bundlesize[tier] << "\n";
            cerr << "Radix must be a multiple of bundlesize\n";
            exit(1);
        }
        if (tier < (_tiers - 1) && _radix_up[tier] == 0) {
            cerr << "Custom topology, but radix_up not set for tier " << tier << endl;
            exit(1);
        }
        if (tier < (_tiers - 1) && _radix_up[tier] % _bundlesize[tier + 1] != 0) {
            cerr << "Mismatch between tier " << tier << " up radix of " << _radix_up[tier] << " and tier " << tier + 1
                 << " down bundlesize " << _bundlesize[tier + 1] << "\n";
            cerr << "Radix must be a multiple of bundlesize\n";
            exit(1);
        }
    }

    int no_of_pods = 0;
    _no_of_nodes = no_of_nodes;
    _tor_switches_per_pod = 0;
    _agg_switches_per_pod = 0;
    int no_of_tor_uplinks = 0;
    int no_of_agg_uplinks = 0;
    int no_of_core_switches = 0;
    if (no_of_nodes % _hosts_per_pod != 0) {
        cerr << "No_of_nodes is not a multiple of hosts_per_pod\n";
        exit(1);
    }

    no_of_pods = no_of_nodes / _hosts_per_pod; // we don't allow multi-port hosts yet
    assert(_bundlesize[TOR_TIER] == 1);
    if (_hosts_per_pod % _radix_down[TOR_TIER] != 0) {
        cerr << "Mismatch between TOR radix " << _radix_down[TOR_TIER] << " and podsize " << _hosts_per_pod << endl;
        exit(1);
    }
    _tor_switches_per_pod = _hosts_per_pod / _radix_down[TOR_TIER];

    assert((no_of_nodes * _downlink_speeds[TOR_TIER]) % (_downlink_speeds[AGG_TIER] * _oversub[TOR_TIER]) == 0);
    no_of_tor_uplinks = (no_of_nodes * _downlink_speeds[TOR_TIER]) / (_downlink_speeds[AGG_TIER] * _oversub[TOR_TIER]);
    cout << "no_of_tor_uplinks: " << no_of_tor_uplinks << endl;

    if (_radix_down[TOR_TIER] / _radix_up[TOR_TIER] != _oversub[TOR_TIER]) {
        cerr << "Mismatch between TOR linkspeeds (" << speedAsGbps(_downlink_speeds[TOR_TIER]) << "Gbps down, "
             << speedAsGbps(_downlink_speeds[AGG_TIER]) << "Gbps up) and TOR radix (" << _radix_down[TOR_TIER]
             << " down, " << _radix_up[TOR_TIER] << " up) and oversubscription ratio of " << _oversub[TOR_TIER] << endl;
        exit(1);
    }

    assert(no_of_tor_uplinks % (no_of_pods * _radix_down[AGG_TIER]) == 0);
    _agg_switches_per_pod = no_of_tor_uplinks / (no_of_pods * _radix_down[AGG_TIER]);
    if (_agg_switches_per_pod * _bundlesize[AGG_TIER] != _radix_up[TOR_TIER]) {
        cerr << "Mismatch between TOR up radix " << _radix_up[TOR_TIER] << " and " << _agg_switches_per_pod
             << " aggregation switches per pod required by " << no_of_tor_uplinks << " TOR uplinks in " << no_of_pods
             << " pods "
             << " with an aggregation switch down radix of " << _radix_down[AGG_TIER] << endl;
        if (_bundlesize[AGG_TIER] == 1 && _radix_up[TOR_TIER] % _agg_switches_per_pod == 0 &&
            _radix_up[TOR_TIER] / _agg_switches_per_pod > 1) {
            cerr << "Did you miss specifying a Tier 1 bundle size of " << _radix_up[TOR_TIER] / _agg_switches_per_pod
                 << "?" << endl;
        } else if (_radix_up[TOR_TIER] % _agg_switches_per_pod == 0 &&
                   _radix_up[TOR_TIER] / _agg_switches_per_pod != _bundlesize[AGG_TIER]) {
            cerr << "Tier 1 bundle size is " << _bundlesize[AGG_TIER] << ". Did you mean it to be "
                 << _radix_up[TOR_TIER] / _agg_switches_per_pod << "?" << endl;
        }
        exit(1);
    }

    if (_tiers == 3) {
        assert((no_of_tor_uplinks * _downlink_speeds[AGG_TIER]) % (_downlink_speeds[CORE_TIER] * _oversub[AGG_TIER]) ==
               0);
        no_of_agg_uplinks =
                (no_of_tor_uplinks * _downlink_speeds[AGG_TIER]) / (_downlink_speeds[CORE_TIER] * _oversub[AGG_TIER]);
        cout << "no_of_agg_uplinks: " << no_of_agg_uplinks << endl;

        assert(no_of_agg_uplinks % _radix_down[CORE_TIER] == 0);
        no_of_core_switches = no_of_agg_uplinks / _radix_down[CORE_TIER];

        if (no_of_core_switches % _agg_switches_per_pod != 0) {
            cerr << "Topology results in " << no_of_core_switches
                 << " core switches, which isn't an integer multiple of " << _agg_switches_per_pod
                 << " aggregation switches per pod, computed from Tier 0 and 1 "
                    "values\n";
            exit(1);
        }

        if ((no_of_core_switches * _bundlesize[CORE_TIER]) / _agg_switches_per_pod != _radix_up[AGG_TIER]) {
            cerr << "Mismatch between the AGG switch up-radix of " << _radix_up[AGG_TIER] << " and calculated "
                 << _agg_switches_per_pod << " aggregation switched per pod with " << no_of_core_switches
                 << " core switches" << endl;
            if (_bundlesize[CORE_TIER] == 1 &&
                _radix_up[AGG_TIER] % (no_of_core_switches / _agg_switches_per_pod) == 0 &&
                _radix_up[AGG_TIER] / (no_of_core_switches / _agg_switches_per_pod) > 1) {
                cerr << "Did you miss specifying a Tier 2 bundle size of "
                     << _radix_up[AGG_TIER] / (no_of_core_switches / _agg_switches_per_pod) << "?" << endl;
            } else if (_radix_up[AGG_TIER] % (no_of_core_switches / _agg_switches_per_pod) == 0 &&
                       _radix_up[AGG_TIER] / (no_of_core_switches / _agg_switches_per_pod) != _bundlesize[CORE_TIER]) {
                cerr << "Tier 2 bundle size is " << _bundlesize[CORE_TIER] << ". Did you mean it to be "
                     << _radix_up[AGG_TIER] / (no_of_core_switches / _agg_switches_per_pod) << "?" << endl;
            }
            exit(1);
        }
    }

    cout << "No of nodes: " << no_of_nodes << endl;
    cout << "No of pods: " << no_of_pods << endl;
    cout << "Hosts per pod: " << _hosts_per_pod << endl;
    cout << "Hosts per pod: " << _hosts_per_pod << endl;
    cout << "ToR switches per pod: " << _tor_switches_per_pod << endl;
    cout << "Agg switches per pod: " << _agg_switches_per_pod << endl;
    cout << "No of core switches: " << no_of_core_switches << endl;
    for (uint32_t tier = TOR_TIER; tier < _tiers; tier++) {
        cout << "Tier " << tier << " QueueSize Down " << _queue_down[tier] << " bytes" << endl;
        if (tier < CORE_TIER)
            cout << "Tier " << tier << " QueueSize Up " << _queue_up[tier] << " bytes" << endl;
    }

    // looks like we're OK, lets build it
    NSRV = no_of_nodes;
    NTOR = _tor_switches_per_pod * no_of_pods;
    NAGG = _agg_switches_per_pod * no_of_pods;
    NPOD = no_of_pods;
    NCORE = no_of_core_switches;
    alloc_vectors();
}

void FatTreeInterDCTopology::alloc_vectors() {

    switches_lp.resize(number_datacenters, vector<Switch *>(NTOR));
    switches_up.resize(number_datacenters, vector<Switch *>(NAGG));
    switches_c.resize(number_datacenters, vector<Switch *>(NCORE));
    switches_border.resize(number_datacenters, vector<Switch *>(number_border_switches));

    // These vectors are sparse - we won't use all the entries
    if (_tiers == 3) {
        // resizing 3d vectors is scary magic
        pipes_nc_nup.resize(number_datacenters,
                            vector<vector<vector<Pipe *>>>(
                                    NCORE, vector<vector<Pipe *>>(NAGG, vector<Pipe *>(_bundlesize[CORE_TIER]))));
        queues_nc_nup.resize(
                number_datacenters,
                vector<vector<vector<BaseQueue *>>>(
                        NCORE, vector<vector<BaseQueue *>>(NAGG, vector<BaseQueue *>(_bundlesize[CORE_TIER]))));
    }

    pipes_nup_nlp.resize(
            number_datacenters,
            vector<vector<vector<Pipe *>>>(NAGG, vector<vector<Pipe *>>(NTOR, vector<Pipe *>(_bundlesize[AGG_TIER]))));
    queues_nup_nlp.resize(number_datacenters,
                          vector<vector<vector<BaseQueue *>>>(
                                  NAGG, vector<vector<BaseQueue *>>(NTOR, vector<BaseQueue *>(_bundlesize[AGG_TIER]))));

    pipes_nlp_ns.resize(
            number_datacenters,
            vector<vector<vector<Pipe *>>>(NTOR, vector<vector<Pipe *>>(NSRV, vector<Pipe *>(_bundlesize[TOR_TIER]))));

    queues_nlp_ns.resize(number_datacenters,
                         vector<vector<vector<BaseQueue *>>>(
                                 NTOR, vector<vector<BaseQueue *>>(NSRV, vector<BaseQueue *>(_bundlesize[TOR_TIER]))));

    if (_tiers == 3) {
        pipes_nup_nc.resize(number_datacenters,
                            vector<vector<vector<Pipe *>>>(
                                    NAGG, vector<vector<Pipe *>>(NCORE, vector<Pipe *>(_bundlesize[CORE_TIER]))));
        queues_nup_nc.resize(
                number_datacenters,
                vector<vector<vector<BaseQueue *>>>(
                        NAGG, vector<vector<BaseQueue *>>(NCORE, vector<BaseQueue *>(_bundlesize[CORE_TIER]))));
    }

    pipes_nlp_nup.resize(
            number_datacenters,
            vector<vector<vector<Pipe *>>>(NTOR, vector<vector<Pipe *>>(NAGG, vector<Pipe *>(_bundlesize[AGG_TIER]))));
    pipes_ns_nlp.resize(
            number_datacenters,
            vector<vector<vector<Pipe *>>>(NSRV, vector<vector<Pipe *>>(NTOR, vector<Pipe *>(_bundlesize[TOR_TIER]))));

    queues_nlp_nup.resize(number_datacenters,
                          vector<vector<vector<BaseQueue *>>>(
                                  NTOR, vector<vector<BaseQueue *>>(NAGG, vector<BaseQueue *>(_bundlesize[AGG_TIER]))));
    queues_ns_nlp.resize(number_datacenters,
                         vector<vector<vector<BaseQueue *>>>(
                                 NSRV, vector<vector<BaseQueue *>>(NTOR, vector<BaseQueue *>(_bundlesize[TOR_TIER]))));
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
            cerr << "Topology Error: can't have a 3-Tier FatTree with " << no_of_nodes << " nodes, the closet is "
                 << _no_of_nodes << "nodes with K=" << K << "\n";
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
            cerr << "Topology Error: can't have a 2-Tier FatTree with " << no_of_nodes << " nodes, the closet is "
                 << _no_of_nodes << "nodes with K=" << K << "\n";
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
    switches_border.resize(number_datacenters, vector<Switch *>(number_border_switches));

    // These vectors are sparse - we won't use all the entries
    if (_tiers == 3) {
        pipes_nc_nup.resize(number_datacenters, vector<vector<Pipe *>>(NCORE, vector<Pipe *>(NAGG)));
        queues_nc_nup.resize(number_datacenters, vector<vector<BaseQueue *>>(NCORE, vector<BaseQueue *>(NAGG)));
    }

    pipes_nup_nlp.resize(number_datacenters, vector<vector<Pipe *>>(NAGG, vector<Pipe *>(NTOR)));
    queues_nup_nlp.resize(number_datacenters, vector<vector<BaseQueue *>>(NAGG, vector<BaseQueue *>(NTOR)));

    pipes_nlp_ns.resize(number_datacenters, vector<vector<Pipe *>>(NTOR, vector<Pipe *>(NSRV)));
    queues_nlp_ns.resize(number_datacenters, vector<vector<BaseQueue *>>(NTOR, vector<BaseQueue *>(NSRV)));

    if (_tiers == 3) {
        pipes_nup_nc.resize(number_datacenters, vector<vector<Pipe *>>(NAGG, vector<Pipe *>(NCORE)));
        queues_nup_nc.resize(number_datacenters, vector<vector<BaseQueue *>>(NAGG, vector<BaseQueue *>(NCORE)));
    }

    pipes_nlp_nup.resize(number_datacenters, vector<vector<Pipe *>>(NTOR, vector<Pipe *>(NAGG)));
    pipes_ns_nlp.resize(number_datacenters, vector<vector<Pipe *>>(NSRV, vector<Pipe *>(NTOR)));
    queues_nlp_nup.resize(number_datacenters, vector<vector<BaseQueue *>>(NTOR, vector<BaseQueue *>(NAGG)));
    queues_ns_nlp.resize(number_datacenters, vector<vector<BaseQueue *>>(NSRV, vector<BaseQueue *>(NTOR)));

    // Double Check Later
    uint32_t uplink_numbers = max((unsigned int)1, (K / 2) / _os);
    _no_of_core_to_border = uplink_numbers * NAGG / NCORE;
    printf("Number of core to border links: %d - uplinks %d \n", _no_of_core_to_border, uplink_numbers);
    _num_links_same_border_from_core = NAGG / _no_of_core_to_border;
    _num_links_between_borders = uplink_numbers * NAGG / number_border_switches;
    _num_links_same_border_from_core = _num_links_between_borders / NCORE;

    printf("Number of links between borders: %d - uplinks %d \n", _num_links_between_borders, uplink_numbers);

    _num_links_same_border_from_core = _num_links_same_border_from_core / 1;
    _num_links_between_borders = _num_links_between_borders / os_ratio_border;

    if (_num_links_same_border_from_core == 0 || _num_links_between_borders == 0) {
        exit(0);
    }

    pipes_nborder_nc.resize(number_datacenters,
                            vector<vector<vector<Pipe *>>>(
                                    number_border_switches,
                                    vector<vector<Pipe *>>(NCORE, vector<Pipe *>(_num_links_same_border_from_core))));

    queues_nborder_nc.resize(
            number_datacenters,
            vector<vector<vector<BaseQueue *>>>(
                    number_border_switches,
                    vector<vector<BaseQueue *>>(NCORE, vector<BaseQueue *>(_num_links_same_border_from_core))));

    pipes_nc_nborder.resize(number_datacenters,
                            vector<vector<vector<Pipe *>>>(
                                    NCORE, vector<vector<Pipe *>>(number_border_switches,
                                                                  vector<Pipe *>(_num_links_same_border_from_core))));

    queues_nc_nborder.resize(
            number_datacenters,
            vector<vector<vector<BaseQueue *>>>(
                    NCORE, vector<vector<BaseQueue *>>(number_border_switches,
                                                       vector<BaseQueue *>(_num_links_same_border_from_core))));

    queues_nborderl_nborderu.resize(
            number_border_switches,
            vector<vector<BaseQueue *>>(number_border_switches, vector<BaseQueue *>(_num_links_between_borders)));
    queues_nborderu_nborderl.resize(
            number_border_switches,
            vector<vector<BaseQueue *>>(number_border_switches, vector<BaseQueue *>(_num_links_between_borders)));

    pipes_nborderl_nborderu.resize(
            number_border_switches,
            vector<vector<Pipe *>>(number_border_switches, vector<Pipe *>(_num_links_between_borders)));
    pipes_nborderu_nborderl.resize(
            number_border_switches,
            vector<vector<Pipe *>>(number_border_switches, vector<Pipe *>(_num_links_between_borders)));
}

BaseQueue *FatTreeTopology::alloc_src_queue(QueueLogger *queueLogger) {
    linkspeed_bps linkspeed = _downlink_speeds[TOR_TIER]; // linkspeeds are symmetric
    switch (_sender_qt) {
    case SWIFT_SCHEDULER:
        return new FairScheduler(linkspeed, *_eventlist, queueLogger);
    case PRIORITY:
        return new PriorityQueue(linkspeed, memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    case FAIR_PRIO:
        return new FairPriorityQueue(linkspeed, memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
    default:
        abort();
    }
}

BaseQueue *FatTreeTopology::alloc_queue(QueueLogger *queueLogger, mem_b queuesize, link_direction dir, int switch_tier,
                                        bool tor = false) {
    if (dir == UPLINK) {
        switch_tier++; // _downlink_speeds is set for the downlinks, so uplinks
                       // need to use the tier above's linkspeed
    }
    return alloc_queue(queueLogger, _downlink_speeds[switch_tier], queuesize, dir, switch_tier, tor);
}

BaseQueue *FatTreeTopology::alloc_queue(QueueLogger *queueLogger, linkspeed_bps speed, mem_b queuesize,
                                        link_direction dir, int switch_tier, bool tor) {
    switch (_qt) {
    case RANDOM:
        return new RandomQueue(speed, queuesize, *_eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    case COMPOSITE: {
        CompositeQueue *q = new CompositeQueue(speed, queuesize, *_eventlist, queueLogger);

        if (_enable_ecn) {
            if (!tor || dir == UPLINK || _enable_on_tor_downlink) {
                // don't use ECN on ToR downlinks unless configured so.
                q->set_ecn_thresholds(_ecn_low, _ecn_high);
            }
        }
        return q;
    }
    case CTRL_PRIO:
        return new CtrlPrioQueue(speed, queuesize, *_eventlist, queueLogger);
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
                    for (uint32_t b = 0; b < _bundlesize[CORE_TIER]; b++) {
                        queues_nc_nup[i][j][k][b] = NULL;
                        pipes_nc_nup[i][j][k][b] = NULL;
                        queues_nup_nc[i][k][j][b] = NULL;
                        pipes_nup_nc[i][k][j][b] = NULL;
                    }
                }
            }
        }
    }

    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t j = 0; j < number_border_switches; j++) {
            for (uint32_t k = 0; k < NCORE; k++) {
                for (uint32_t p = 0; p < _num_links_same_border_from_core; p++) {
                    queues_nborder_nc[i][j][k][p] = NULL;
                    pipes_nborder_nc[i][j][k][p] = NULL;
                    queues_nc_nborder[i][k][j][p] = NULL;
                    pipes_nc_nborder[i][k][j][p] = NULL;
                }
            }
        }
    }

    for (uint32_t j = 0; j < number_border_switches; j++) {
        for (uint32_t k = 0; k < number_border_switches; k++) {
            for (uint32_t p = 0; p < _num_links_between_borders; p++) {
                queues_nborderl_nborderu[j][k][p] = NULL;
                pipes_nborderl_nborderu[j][k][p] = NULL;
                queues_nborderu_nborderl[k][j][p] = NULL;
                pipes_nborderu_nborderl[k][j][p] = NULL;
            }
        }
    }

    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t j = 0; j < NAGG; j++) {
            for (uint32_t k = 0; k < NTOR; k++) {
                for (uint32_t b = 0; b < _bundlesize[AGG_TIER]; b++) {
                    queues_nup_nlp[i][j][k][b] = NULL;
                    pipes_nup_nlp[i][j][k][b] = NULL;
                    queues_nlp_nup[i][k][j][b] = NULL;
                    pipes_nlp_nup[i][k][j][b] = NULL;
                }
            }
        }
    }

    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t j = 0; j < NTOR; j++) {
            for (uint32_t k = 0; k < NSRV; k++) {
                for (uint32_t b = 0; b < _bundlesize[TOR_TIER]; b++) {
                    queues_nlp_ns[i][j][k][b] = NULL;
                    pipes_nlp_ns[i][j][k][b] = NULL;
                    queues_ns_nlp[i][k][j][b] = NULL;
                    pipes_ns_nlp[i][k][j][b] = NULL;
                }
            }
        }
    }

    // create switches if we have lossless operation
    // if (_qt==LOSSLESS)
    //  changed to always create switches
    for (int i = 0; i < number_datacenters; i++) {
        cout << "total switches: ToR " << NTOR << " NAGG " << NAGG << " NCORE " << NCORE << " srv_per_tor " << K / 2
             << endl;
        for (uint32_t j = 0; j < NTOR; j++) {
            simtime_picosec switch_latency =
                    (_switch_latencies[TOR_TIER] > 0) ? _switch_latencies[TOR_TIER] : _switch_latency;
            switches_lp[i][j] = new FatTreeInterDCSwitch(*_eventlist, "Switch_LowerPod_" + ntoa(j),
                                                         FatTreeInterDCSwitch::TOR, j, _switch_latency, this, i);
        }
        for (uint32_t j = 0; j < NAGG; j++) {
            simtime_picosec switch_latency =
                    (_switch_latencies[AGG_TIER] > 0) ? _switch_latencies[AGG_TIER] : _switch_latency;
            switches_up[i][j] = new FatTreeInterDCSwitch(*_eventlist, "Switch_UpperPod_" + ntoa(j),
                                                         FatTreeInterDCSwitch::AGG, j, _switch_latency, this, i);
        }
        for (uint32_t j = 0; j < NCORE; j++) {
            simtime_picosec switch_latency =
                    (_switch_latencies[CORE_TIER] > 0) ? _switch_latencies[CORE_TIER] : _switch_latency;
            switches_c[i][j] = new FatTreeInterDCSwitch(*_eventlist, "Switch_Core_" + ntoa(j),
                                                        FatTreeInterDCSwitch::CORE, j, _switch_latency, this, i);
        }
        for (uint32_t j = 0; j < number_border_switches; j++) {
            switches_border[i][j] = new FatTreeInterDCSwitch(*_eventlist, "Switch_Border_" + ntoa(j),
                                                             FatTreeInterDCSwitch::BORDER, j, _switch_latency, this, i);
        }
    }

    // links from lower layer pod switch to server
    for (int i = 0; i < number_datacenters; i++) {
        for (uint32_t tor = 0; tor < NTOR; tor++) {
            uint32_t link_bundles = _radix_down[TOR_TIER] / _bundlesize[TOR_TIER];
            for (uint32_t l = 0; l < link_bundles; l++) {
                uint32_t srv = tor * link_bundles + l;
                for (uint32_t b = 0; b < _bundlesize[TOR_TIER]; b++) {
                    // Downlink
                    if (_logger_factory) {
                        queueLogger = _logger_factory->createQueueLogger();
                    } else {
                        queueLogger = NULL;
                    }

                    queues_nlp_ns[i][tor][srv][b] =
                            alloc_queue(queueLogger, _queue_down[TOR_TIER], DOWNLINK, TOR_TIER, true);
                    queues_nlp_ns[i][tor][srv][b]->setName("LS" + ntoa(tor) + "->DST" + ntoa(srv) + "(" + ntoa(b) +
                                                           ")");
                    // if (logfile) logfile->writeName(*(queues_nlp_ns[tor][srv]));
                    simtime_picosec hop_latency = (_hop_latency == 0) ? _link_latencies[TOR_TIER] : _hop_latency;
                    pipes_nlp_ns[i][tor][srv][b] = new Pipe(hop_latency, *_eventlist);
                    pipes_nlp_ns[i][tor][srv][b]->setName("Pipe-LS" + ntoa(tor) + "->DST" + ntoa(srv) + "(" + ntoa(b) +
                                                          ")");
                    // if (logfile) logfile->writeName(*(pipes_nlp_ns[tor][srv]));

                    // Uplink
                    if (_logger_factory) {
                        queueLogger = _logger_factory->createQueueLogger();
                    } else {
                        queueLogger = NULL;
                    }
                    queues_ns_nlp[i][srv][tor][b] = alloc_src_queue(queueLogger);
                    queues_ns_nlp[i][srv][tor][b]->setName("SRC" + ntoa(srv) + "->LS" + ntoa(tor) + "(" + ntoa(b) +
                                                           ")");
                    // cout << queues_ns_nlp[srv][tor][b]->str() << endl;
                    // if (logfile) logfile->writeName(*(queues_ns_nlp[srv][tor]));

                    queues_ns_nlp[i][srv][tor][b]->setRemoteEndpoint(switches_lp[tor]);

                    assert(switches_lp[i][tor]->addPort(queues_nlp_ns[tor][srv][b]) < 96);

                    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                        // no virtual queue needed at server
                        new LosslessInputQueue(*_eventlist, queues_ns_nlp[srv][tor][b], switches_lp[tor], _hop_latency);
                    }

                    pipes_ns_nlp[i][srv][tor][b] = new Pipe(hop_latency, *_eventlist);
                    pipes_ns_nlp[i][srv][tor][b]->setName("Pipe-SRC" + ntoa(srv) + "->LS" + ntoa(tor) + "(" + ntoa(b) +
                                                          ")");
                    // if (logfile) logfile->writeName(*(pipes_ns_nlp[srv][tor]));

                    if (ff) {
                        ff->add_queue(queues_nlp_ns[tor][srv][b]);
                        ff->add_queue(queues_ns_nlp[srv][tor][b]);
                    }
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

                queues_nup_nlp[i][agg][tor] = alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1,
                                                          _queuesize / _os_ratio_stage_1, DOWNLINK, false);

                queues_nup_nlp[i][agg][tor]->setName("DC" + ntoa(i) + "-US" + ntoa(agg) + "->LS_" + ntoa(tor));
                // if (logfile) logfile->writeName(*(queues_nup_nlp[agg][tor]));

                pipes_nup_nlp[i][agg][tor] = new Pipe(_hop_latency, *_eventlist);
                pipes_nup_nlp[i][agg][tor]->setName("DC" + ntoa(i) + "-Pipe-US" + ntoa(agg) + "->LS" + ntoa(tor));
                // if (logfile) logfile->writeName(*(pipes_nup_nlp[agg][tor]));

                // Uplink
                if (_logger_factory) {
                    queueLogger = _logger_factory->createQueueLogger();
                } else {
                    queueLogger = NULL;
                }
                queues_nlp_nup[i][tor][agg] = alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1,
                                                          _queuesize / _os_ratio_stage_1, UPLINK, true);
                queues_nlp_nup[i][tor][agg]->setName("DC" + ntoa(i) + "-LS" + ntoa(tor) + "->US" + ntoa(agg));
                // cout << queues_nlp_nup[tor][agg]->str() << endl;
                //  if (logfile)
                //  logfile->writeName(*(queues_nlp_nup[tor][agg]));

                switches_lp[i][tor]->addPort(queues_nlp_nup[i][tor][agg]);
                switches_up[i][agg]->addPort(queues_nup_nlp[i][agg][tor]);
                queues_nlp_nup[i][tor][agg]->setRemoteEndpoint(switches_up[i][agg]);
                queues_nup_nlp[i][agg][tor]->setRemoteEndpoint(switches_lp[i][tor]);

                /*if (_qt==LOSSLESS){
                  ((LosslessQueue*)queues_nlp_nup[tor][agg])->setRemoteEndpoint(queues_nup_nlp[agg][tor]);
                  ((LosslessQueue*)queues_nup_nlp[agg][tor])->setRemoteEndpoint(queues_nlp_nup[tor][agg]);
                  }else */
                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                    new LosslessInputQueue(*_eventlist, queues_nlp_nup[i][tor][agg], switches_up[i][agg]);
                    new LosslessInputQueue(*_eventlist, queues_nup_nlp[i][agg][tor], switches_lp[i][tor]);
                }

                pipes_nlp_nup[i][tor][agg] = new Pipe(_hop_latency, *_eventlist);
                pipes_nlp_nup[i][tor][agg]->setName("DC" + ntoa(i) + "-Pipe-LS" + ntoa(tor) + "->US" + ntoa(agg));
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

                    if (curr_failed_link < num_failing_links) {
                        queues_nup_nc[i][agg][core] = alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1,
                                                                  _queuesize / _os_ratio_stage_1, UPLINK, false, true);
                        curr_failed_link++;
                    } else {
                        queues_nup_nc[i][agg][core] = alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1,
                                                                  _queuesize / _os_ratio_stage_1, UPLINK, false, false);
                    }

                    queues_nup_nc[i][agg][core]->setName("DC" + ntoa(i) + "-US" + ntoa(agg) + "->CS" + ntoa(core));
                    // cout << queues_nup_nc[agg][core]->str() << endl;
                    //  if (logfile)
                    //  logfile->writeName(*(queues_nup_nc[agg][core]));

                    pipes_nup_nc[i][agg][core] = new Pipe(_hop_latency, *_eventlist);

                    pipes_nup_nc[i][agg][core]->setName("DC" + ntoa(i) + "-Pipe-US" + ntoa(agg) + "->CS" + ntoa(core));
                    // if (logfile)
                    // logfile->writeName(*(pipes_nup_nc[agg][core]));

                    // Uplink
                    if (_logger_factory) {
                        queueLogger = _logger_factory->createQueueLogger();
                    } else {
                        queueLogger = NULL;
                    }

                    if ((l + agg * K / 2) < failed_links) {
                        queues_nc_nup[i][core][agg] = alloc_queue(queueLogger, 0, _queuesize, //_linkspeed/10
                                                                  DOWNLINK, false);
                        cout << "Adding link failure for agg_sw " << ntoa(agg) << " l " << ntoa(l) << endl;
                    } else {

                        queues_nc_nup[i][core][agg] = alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1,
                                                                  _queuesize / _os_ratio_stage_1, DOWNLINK, false);
                    }

                    queues_nc_nup[i][core][agg]->setName("DC" + ntoa(i) + "-CS" + ntoa(core) + "->US" + ntoa(agg));

                    switches_up[i][agg]->addPort(queues_nup_nc[i][agg][core]);
                    switches_c[i][core]->addPort(queues_nc_nup[i][core][agg]);
                    queues_nup_nc[i][agg][core]->setRemoteEndpoint(switches_c[i][core]);
                    queues_nc_nup[i][core][agg]->setRemoteEndpoint(switches_up[i][agg]);

                    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                        new LosslessInputQueue(*_eventlist, queues_nup_nc[i][agg][core], switches_c[i][core]);
                        new LosslessInputQueue(*_eventlist, queues_nc_nup[i][core][agg], switches_up[i][agg]);
                    }
                    // if (logfile)
                    // logfile->writeName(*(queues_nc_nup[core][agg]));

                    pipes_nc_nup[i][core][agg] = new Pipe(_hop_latency, *_eventlist);

                    pipes_nc_nup[i][core][agg]->setName("DC" + ntoa(i) + "-Pipe-CS" + ntoa(core) + "->US" + ntoa(agg));
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
            for (uint32_t border_sw = 0; border_sw < number_border_switches; border_sw++) {

                for (int link_num = 0; link_num < _num_links_same_border_from_core; link_num++) {

                    // UpLinks Queues and Pipes
                    queues_nc_nborder[i][core][border_sw][link_num] =
                            alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1, _queuesize / _os_ratio_stage_1,
                                        UPLINK, false, false);

                    queues_nc_nborder[i][core][border_sw][link_num]->setName("DC" + ntoa(i) + "-CS" + ntoa(core) +
                                                                             "->BORDER" + ntoa(border_sw) + "_LINK" +
                                                                             ntoa(link_num));

                    pipes_nc_nborder[i][core][border_sw][link_num] = new Pipe(_hop_latency, *_eventlist);

                    pipes_nc_nborder[i][core][border_sw][link_num]->setName("DC" + ntoa(i) + "-Pipe-CS" + ntoa(core) +
                                                                            "->BORDER" + ntoa(border_sw) + "_LINK" +
                                                                            ntoa(link_num));

                    // DownLinks Queues and Pipes
                    queues_nborder_nc[i][border_sw][core][link_num] =
                            alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1, _queuesize / _os_ratio_stage_1,
                                        DOWNLINK, false);

                    queues_nborder_nc[i][border_sw][core][link_num]->setName("DC" + ntoa(i) + "-BORDER" +
                                                                             ntoa(border_sw) + "->CS" + ntoa(core) +
                                                                             "_LINK" + ntoa(link_num));

                    pipes_nborder_nc[i][border_sw][core][link_num] = new Pipe(_hop_latency, *_eventlist);

                    pipes_nborder_nc[i][border_sw][core][link_num]->setName("DC" + ntoa(i) + "-Pipe-BORDER" +
                                                                            ntoa(border_sw) + "->CS" + ntoa(core) +
                                                                            "_LINK" + ntoa(link_num));

                    // Add Ports to switches
                    switches_c[i][core]->addPort(queues_nc_nborder[i][core][border_sw][link_num]);
                    switches_border[i][border_sw]->addPort(queues_nborder_nc[i][border_sw][core][link_num]);

                    // Add Remote Endpoints
                    queues_nc_nborder[i][core][border_sw][link_num]->setRemoteEndpoint(switches_border[i][border_sw]);
                    queues_nborder_nc[i][border_sw][core][link_num]->setRemoteEndpoint(switches_c[i][core]);

                    if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                        printf("Not Supported Yet!\n");
                        exit(0);
                    }

                    if (ff) {
                        ff->add_queue(queues_nc_nborder[i][core][border_sw][link_num]);
                        ff->add_queue(queues_nborder_nc[i][border_sw][core][link_num]);
                    }
                }
            }
        }
    }

    // Between border switches
    for (uint32_t border_l = 0; border_l < number_border_switches; border_l++) {
        for (uint32_t border_u = 0; border_u < number_border_switches; border_u++) {
            for (int link_num = 0; link_num < _num_links_between_borders; link_num++) {

                printf("Creating link between border switches %d and %d\n", link_num, link_num);

                // UpLinks Queues and Pipes
                queues_nborderl_nborderu[border_l][border_u][link_num] =
                        alloc_queue(queueLogger, _linkspeed / _os_ratio_stage_1, _queuesize / _os_ratio_stage_1, UPLINK,
                                    false, false);

                queues_nborderl_nborderu[border_l][border_u][link_num]->setName(
                        "DC" + ntoa(0) + "-BORDER" + ntoa(border_l) + "->BORDER" + ntoa(border_u) +
                        "_LINK:" + ntoa(link_num));

                pipes_nborderl_nborderu[border_l][border_u][link_num] = new Pipe(_interdc_delay, *_eventlist);

                pipes_nborderl_nborderu[border_l][border_u][link_num]->setName(
                        "DC" + ntoa(0) + "-Pipe-BORDER" + ntoa(border_l) + "->BORDER" + ntoa(border_u) + "_LINK" +
                        ntoa(link_num));

                // DownLinks Queues and Pipes
                queues_nborderu_nborderl[border_u][border_l][link_num] = alloc_queue(
                        queueLogger, _linkspeed / _os_ratio_stage_1, _queuesize / _os_ratio_stage_1, DOWNLINK, false);

                queues_nborderu_nborderl[border_u][border_l][link_num]->setName(
                        "DC" + ntoa(1) + "-BORDER" + ntoa(border_u) + "->BORDER" + ntoa(border_l) + "_LINK" +
                        ntoa(link_num));

                pipes_nborderu_nborderl[border_u][border_l][link_num] = new Pipe(_interdc_delay, *_eventlist);

                pipes_nborderu_nborderl[border_u][border_l][link_num]->setName(
                        "DC" + ntoa(0) + "-Pipe-BORDER" + ntoa(border_u) + "->BORDER" + ntoa(border_l) + "_LINK" +
                        ntoa(link_num));

                // Add Ports to switches
                switches_border[0][border_l]->addPort(queues_nborderl_nborderu[border_l][border_u][link_num]);
                switches_border[1][border_u]->addPort(queues_nborderu_nborderl[border_u][border_l][link_num]);

                // Add Remote Endpoints
                queues_nborderl_nborderu[border_l][border_u][link_num]->setRemoteEndpoint(switches_border[1][border_u]);
                queues_nborderu_nborderl[border_u][border_l][link_num]->setRemoteEndpoint(switches_border[0][border_l]);

                if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                    printf("Not Supported Yet!\n");
                    exit(0);
                }

                if (ff) {
                    printf("FIRSTFIT");
                    ff->add_queue(queues_nborderl_nborderu[border_l][border_u][link_num]);
                    ff->add_queue(queues_nborderu_nborderl[border_u][border_l][link_num]);
                }
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

vector<const Route *> *FatTreeInterDCTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse) {
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

void FatTreeInterDCTopology::add_switch_loggers(Logfile &log, simtime_picosec sample_period) { return; }
