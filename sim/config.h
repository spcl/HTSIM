// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>
#include <filesystem>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>

double drand();

#ifdef _WIN32
// Ways to refer to integer types
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef signed __int64 sint64_t;
#else
typedef long long sint64_t;
#endif

// Specify units for simulation time, link speed, buffer capacity
typedef uint64_t simtime_picosec;

extern simtime_picosec GLOBAL_TIME;
extern int PKT_SIZE_MODERN;
extern uint64_t LINK_SPEED_MODERN;
extern int SINGLE_PKT_TRASMISSION_TIME_MODERN;
extern int LINK_DELAY_MODERN;
extern uint64_t HOPS;
extern uint64_t BASE_RTT_MODERN;
extern uint64_t TARGET_RTT_MODERN;
extern uint64_t BDP_MODERN_UEC;
extern uint64_t MAX_CWD_MODERN_UEC;
extern bool COLLECT_DATA;
extern uint64_t MAX_CWD_MODERN_UEC;
extern uint64_t MIN_K_ECN_MODERN;
extern uint64_t MAX_K_ECN_MODERN;
extern uint64_t INFINITE_BUFFER_SIZE;
extern uint64_t BDP_MODERN_NDP;
extern uint64_t MAX_CWD_MODERN_NDP;
extern uint64_t BDP_OLD_NDP;
extern uint64_t MAX_CWD_OLD_NDP;
extern uint64_t ENABLE_FAST_DROP;
extern std::filesystem::path PROJECT_ROOT_PATH;

int pareto(int xm, int mean);
double exponential(double lambda);

simtime_picosec timeFromSec(double secs);
simtime_picosec timeFromMs(double msecs);
simtime_picosec timeFromMs(int msecs);
simtime_picosec timeFromUs(double usecs);
simtime_picosec timeFromUs(uint32_t usecs);
simtime_picosec timeFromNs(double nsecs);
double timeAsMs(simtime_picosec ps);
double timeAsUs(simtime_picosec ps);
double timeAsSec(simtime_picosec ps);
typedef sint64_t mem_b;
mem_b memFromPkt(double pkts);

typedef uint64_t linkspeed_bps;
linkspeed_bps speedFromGbps(double Gbitps);
linkspeed_bps speedFromMbps(uint64_t Mbitps);
linkspeed_bps speedFromMbps(double Mbitps);
linkspeed_bps speedFromKbps(uint64_t Kbitps);
linkspeed_bps speedFromPktps(double packetsPerSec);
double speedAsPktps(linkspeed_bps bps);
typedef int mem_pkts;
void initializeLoggingFolders();
std::filesystem::path findRootPath(const std::string &rootDirectoryName);
std::filesystem::path resolvePath(const std::string &root,
                                  const std::string &relativePath);

typedef uint32_t addr_t;
typedef uint16_t port_t;

// Gumph
#if defined(__cplusplus) && !defined(__STL_NO_NAMESPACES)
using namespace std;
#endif

#ifdef _WIN32
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#endif
