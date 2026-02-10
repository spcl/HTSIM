// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
// #include "config.h"
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <list>
#include <sstream>
#include <memory>

#include "clock.h"
#include "compositequeue.h"
#include "connection_matrix.h"
#include "eventlist.h"
#include "logfile.h"
#include "main.h"
#include "network.h"
#include "oversubscribed_cc.h"
#include "pciemodel.h"
#include "pipe.h"
#include "topology.h"
#include "uec.h"
#include "uec_logger.h"
#include "uec_mp.h"

#include "slimfly_switch.h"
#include "slimfly_topology.h"

EventList& eventlist = EventList::getTheEventList();

int main(int argc, char** argv) {
    Clock c(timeFromSec(5 / 100.), eventlist);

    int seed = 13;
    srand(seed);
    srandom(seed);

    enum LoadBalancing_Algo { BITMAP, REPS, REPS_LEGACY, OBLIVIOUS, MIXED, ECMP };

    // Commandline Arguments
    SlimFlySwitch::RoutingStrategy routing_strategy = SlimFlySwitch::MINIMAL;
    LoadBalancing_Algo load_balancing_algo = MIXED;
    std::string topo_base_path, tm_file;
    std::string host_table_base_path;
    mem_b cwnd_b = 0;
    mem_b queuesize = 88;  // default (1x BDP)

    bool receiver_driven_cc = false;
    bool sender_driven_cc = true;

    uint32_t path_entropy_size = 64;

    int i = 1;
    while (i < argc) {
        if (!strcmp(argv[i], "-basepath")) {
            topo_base_path = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-topo")) {
            topo_base_path = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-routing")) {
            if (!strcmp(argv[i + 1], "MINIMAL")) {
                routing_strategy = SlimFlySwitch::MINIMAL;
            } else if (!strcmp(argv[i + 1], "VALIANT")) {
                routing_strategy = SlimFlySwitch::VALIANT;
            } else if (!strcmp(argv[i + 1], "UGAL_L")) {
                routing_strategy = SlimFlySwitch::UGAL_L;
            } else if (!strcmp(argv[i + 1], "SOURCE")) {
                routing_strategy = SlimFlySwitch::SOURCE;
            } else {
                throw std::logic_error("Routing strategy not recognized");
            }
            i++;
        } else if (!strcmp(argv[i], "-host_table_basepath")) {
            host_table_base_path = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-sender_cc_only")) {
            UecSrc::_sender_based_cc = true;
            UecSrc::_receiver_based_cc = false;
            UecSink::_oversubscribed_cc = false;
            sender_driven_cc = true;
            receiver_driven_cc = false;
        } else if (!strcmp(argv[i], "-receiver_cc_only")) {
            UecSrc::_sender_based_cc = false;
            UecSrc::_receiver_based_cc = true;
            UecSink::_oversubscribed_cc = false;
            sender_driven_cc = false;
            receiver_driven_cc = true;
        } else if (!strcmp(argv[i], "-sender_cc")) {
            UecSrc::_sender_based_cc = true;
            sender_driven_cc = true;
        } else if (!strcmp(argv[i], "-receiver_cc")) {
            UecSrc::_receiver_based_cc = true;
            receiver_driven_cc = true;
        } else if (!strcmp(argv[i], "-sender_cc_algo")) {
            UecSrc::_sender_based_cc = true;
            sender_driven_cc = true;
            if (!strcmp(argv[i + 1], "dctcp"))
                UecSrc::_sender_cc_algo = UecSrc::DCTCP;
            else if (!strcmp(argv[i + 1], "nscc"))
                UecSrc::_sender_cc_algo = UecSrc::NSCC;
            else if (!strcmp(argv[i + 1], "constant"))
                UecSrc::_sender_cc_algo = UecSrc::CONSTANT;
            else
                throw std::logic_error("CC not recognized");
            i++;
        } else if (!strcmp(argv[i], "-load_balancing_algo")) {
            if (!strcmp(argv[i + 1], "bitmap"))
                load_balancing_algo = BITMAP;
            else if (!strcmp(argv[i + 1], "reps"))
                load_balancing_algo = REPS;
            else if (!strcmp(argv[i + 1], "reps_legacy"))
                load_balancing_algo = REPS_LEGACY;
            else if (!strcmp(argv[i + 1], "oblivious"))
                load_balancing_algo = OBLIVIOUS;
            else if (!strcmp(argv[i + 1], "mixed"))
                load_balancing_algo = MIXED;
            else if (!strcmp(argv[i + 1], "ecmp"))
                load_balancing_algo = ECMP;
            else
                throw std::logic_error("LB not recognized");
            i++;
        } else if (!strcmp(argv[i], "-paths")) {
            path_entropy_size = std::stoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-tm")) {
            tm_file = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-cwnd")) {
            cwnd_b = (mem_b)std::stoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-q")) {
            queuesize = (mem_b)std::stoi(argv[i + 1]);
            i++;
        } else {
            throw std::logic_error("Argument not recognized");
        }
        i++;
    }

    // Fixed Parameters
    queue_type qt = COMPOSITE;

    int packet_size = 4160;
    uint32_t ports = 1;

    bool trim_disable = false;
    uint16_t trim_size = 64;

    bool ecn = true;
    mem_b ecn_low = 0.2 * queuesize;
    mem_b ecn_high = 0.8 * queuesize;

    uint32_t end_time = 1000000;

    assert(trim_size >= 64 && trim_size <= (uint32_t)packet_size);

    Packet::set_packet_size(packet_size);
    queuesize = memFromPkt(queuesize);

    if (ecn) {
        ecn_low = memFromPkt(ecn_low);
        ecn_high = memFromPkt(ecn_high);
        SlimFlyTopology::set_ecn(ecn, ecn_low, ecn_high);
    }

    eventlist.setEndtime(timeFromUs(end_time));

    // Logging
    bool log_sink = false;
    bool log_nic = false;
    bool log_flow_events = false;
    bool log_traffic = false;
    bool log_queue_usage = false;

    simtime_picosec logtime = timeFromMs(0.25);

    Logfile logfile("logout.dat", eventlist);
    logfile.setStartTime(timeFromSec(0));

    UecSinkLoggerSampling* sink_logger = NULL;
    if (log_sink) {
        sink_logger = new UecSinkLoggerSampling(logtime, eventlist);
        logfile.addLogger(*sink_logger);
    }
    NicLoggerSampling* nic_logger = NULL;
    if (log_nic) {
        nic_logger = new NicLoggerSampling(logtime, eventlist);
        logfile.addLogger(*nic_logger);
    }
    TrafficLoggerSimple* traffic_logger = NULL;
    if (log_traffic) {
        traffic_logger = new TrafficLoggerSimple();
        logfile.addLogger(*traffic_logger);
    }
    FlowEventLoggerSimple* event_logger = NULL;
    if (log_flow_events) {
        event_logger = new FlowEventLoggerSimple();
        logfile.addLogger(*event_logger);
    }

    QueueLoggerFactory* qlf = 0;
    if (log_queue_usage) {
        qlf = new QueueLoggerFactory(&logfile, QueueLoggerFactory::LOGGER_EMPTY, eventlist);
        qlf->set_sample_period(timeFromUs(10.0));
    }

    // Topology
    SlimFlySwitch::set_config(routing_strategy, trim_disable, trim_size);
    SlimFlyTopology* topo = new SlimFlyTopology(topo_base_path, qt, queuesize, qlf, &eventlist);
    topo->load_topology(topo_base_path);

    if (routing_strategy == SlimFlySwitch::SOURCE) {
        if (host_table_base_path.empty())
            host_table_base_path = topo_base_path;
        UecSink::setHostTablePath(host_table_base_path);
        UecSink::setHostTableP(topo->get_p());
    }

    uint32_t no_hosts = topo->get_no_hosts();
    ConnectionMatrix* conns = new ConnectionMatrix(no_hosts);

    if (!conns->load(tm_file.c_str()))
        throw std::logic_error("Could not load connection matrix file");

    if (conns->N != no_hosts)
        throw std::logic_error("Number of nodes mismatch");

    UecSrc::_global_node_count = no_hosts;

    simtime_picosec max_rtt = topo->get_max_rtt(routing_strategy, Packet::data_packet_size(),
                                                UecBasePacket::get_ack_size());
    linkspeed_bps linkspeed = topo->get_linkspeed();

    if (sender_driven_cc)
        UecSrc::initNsccParams(max_rtt, linkspeed, timeFromUs((uint32_t)0), -1, !trim_disable);

    std::vector<std::unique_ptr<UecPullPacer>> pacers;
    std::vector<std::unique_ptr<UecNIC>> nics;
    std::vector<OversubscribedCC*> oversubscribed_ccs;

    for (size_t ix = 0; ix < no_hosts; ix++) {
        pacers.emplace_back(std::make_unique<UecPullPacer>(linkspeed, 0.99,
                                                           UecBasePacket::unquantize(UecSink::_credit_per_pull),
                                                           eventlist, ports));
        if (UecSink::_oversubscribed_cc)
            oversubscribed_ccs.push_back(new OversubscribedCC(eventlist, pacers[ix].get()));
        nics.emplace_back(std::make_unique<UecNIC>(ix, eventlist, linkspeed, ports));
        if (log_nic)
            nic_logger->monitorNic(nics[ix].get());
    }

    std::vector<UecSrc*> uec_srcs;
    std::map<flowid_t, std::pair<UecSrc*, UecSink*>> flowmap;

    std::vector<connection*>* all_conns = conns->getAllConnections();

    for (size_t c = 0; c < all_conns->size(); c++) {
        connection* crt = all_conns->at(c);

        int src = crt->src;
        int dest = crt->dst;

        std::unique_ptr<UecMultipath> mp = nullptr;
        if (routing_strategy == SlimFlySwitch::SOURCE) {
            mp = std::make_unique<UecMpSource>(host_table_base_path, src, dest, topo->get_p(), UecSrc::_debug);
        } else if (load_balancing_algo == BITMAP) {
            mp = std::make_unique<UecMpBitmap>(path_entropy_size, UecSrc::_debug);
        } else if (load_balancing_algo == REPS) {
            mp = std::make_unique<UecMpReps>(path_entropy_size, UecSrc::_debug, !trim_disable);
        } else if (load_balancing_algo == REPS_LEGACY) {
            mp = std::make_unique<UecMpRepsLegacy>(path_entropy_size, UecSrc::_debug);
        } else if (load_balancing_algo == OBLIVIOUS) {
            mp = std::make_unique<UecMpOblivious>(path_entropy_size, UecSrc::_debug);
        } else if (load_balancing_algo == MIXED) {
            mp = std::make_unique<UecMpMixed>(path_entropy_size, UecSrc::_debug);
        } else if (load_balancing_algo == ECMP) {
            mp = std::make_unique<UecMpEcmp>(path_entropy_size, UecSrc::_debug);
        }

        UecSrc* uec_src = new UecSrc(traffic_logger, eventlist, std::move(mp), *nics.at(src), ports);

        if (receiver_driven_cc)
            uec_src->initRccc(cwnd_b, max_rtt);
        else
            uec_src->initNscc(cwnd_b, max_rtt);

        uec_srcs.push_back(uec_src);

        if (crt->size > 0)
            uec_src->setFlowSize(crt->size);

        uec_src->setSrc(src);
        uec_src->setDst(dest);
        uec_src->setName("Uec_" + ntoa(src) + "_" + ntoa(dest));

        if (log_flow_events)
            uec_src->logFlowEvents(*event_logger);

        logfile.writeName(*uec_src);

        UecSink* uec_snk;
        if (receiver_driven_cc)
            uec_snk = new UecSink(NULL, pacers[dest].get(), *nics.at(dest), ports);
        else
            uec_snk = new UecSink(NULL, linkspeed, 1.1,
                                  UecBasePacket::unquantize(UecSink::_credit_per_pull), eventlist,
                                  *nics.at(dest), ports);

        uec_snk->setSrc(src);
        uec_snk->setDst(dest);

        if (UecSink::_oversubscribed_cc)
            uec_snk->setOversubscribedCC(oversubscribed_ccs[dest]);

        ((DataReceiver*)uec_snk)->setName("Uec_sink_" + ntoa(src) + "_" + ntoa(dest));
        logfile.writeName(*(DataReceiver*)uec_snk);

        if (crt->flowid) {
            uec_src->setFlowId(crt->flowid);
            uec_snk->setFlowId(crt->flowid);
            assert(flowmap.find(crt->flowid) == flowmap.end());
            flowmap[crt->flowid] = {uec_src, uec_snk};
        }

        if (crt->trigger) {
            Trigger* trig = conns->getTrigger(crt->trigger, eventlist);
            trig->add_target(*uec_src);
        }

        if (crt->send_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->send_done_trigger, eventlist);
            uec_src->setEndTrigger(*trig);
        }

        if (crt->recv_done_trigger) {
            Trigger* trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
            uec_snk->setEndTrigger(*trig);
        }

        Route* srctotor = new Route();
        uint32_t src_switch = topo->get_host_switch(src);
        srctotor->push_back(topo->queues_host_switch[src][src_switch]);
        srctotor->push_back(topo->pipes_host_switch[src][src_switch]);
        srctotor->push_back(topo->queues_host_switch[src][src_switch]->getRemoteEndpoint());

        Route* dsttotor = new Route();
        uint32_t dst_switch = topo->get_host_switch(dest);
        dsttotor->push_back(topo->queues_host_switch[dest][dst_switch]);
        dsttotor->push_back(topo->pipes_host_switch[dest][dst_switch]);
        dsttotor->push_back(topo->queues_host_switch[dest][dst_switch]->getRemoteEndpoint());

        uint32_t p = 0;

        uec_src->connectPort(p, *srctotor, *dsttotor, *uec_snk, crt->start);

        topo->switches[src_switch]->addHostPort(src, uec_snk->flowId(), uec_src->getPort(p));
        topo->switches[dst_switch]->addHostPort(dest, uec_src->flowId(), uec_snk->getPort(p));

        if (log_sink)
            sink_logger->monitorSink(uec_snk);
    }

    Logged::dump_idmap();

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# hostnicrate = " + ntoa(linkspeed / 1000000) + " Mbps");

    // Simulate
    cout << "Starting simulation" << endl;
    while (eventlist.doNextEvent()) {
        // Simulate
    }

    cout << "Done" << endl;
    int new_pkts = 0, rtx_pkts = 0, rts_pkts = 0, bounce_pkts = 0, ack_pkts = 0, nack_pkts = 0, pull_pkts = 0, sleek_pkts = 0;
    for (size_t ix = 0; ix < uec_srcs.size(); ix++) {
        const struct UecSrc::Stats& s = uec_srcs[ix]->stats();
        new_pkts += s.new_pkts_sent;
        rtx_pkts += s.rtx_pkts_sent;
        rts_pkts += s.rts_pkts_sent;
        bounce_pkts += s.bounces_received;
        ack_pkts += s.acks_received;
        nack_pkts += s.nacks_received;
        pull_pkts += s.pulls_received;
        sleek_pkts += s._sleek_counter;
    }
    cout << "New: " << new_pkts << " Rtx: " << rtx_pkts << " RTS: " << rts_pkts
         << " Bounced: " << bounce_pkts << " ACKs: " << ack_pkts << " NACKs: " << nack_pkts
         << " Pulls: " << pull_pkts << " sleek_pkts: " << sleek_pkts << endl;

    delete conns;
}
