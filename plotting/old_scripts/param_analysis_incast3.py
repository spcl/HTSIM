import os
import re

class Configuration:
    def __init__(self, k, fd, fi, e, j, d, version, runtime_vs_ndp=0, maxvsminfair=0):
        self.k = k
        self.fd = fd
        self.fi = fi
        self.e = e
        self.j = j
        self.d = d
        self.version = version
        self.runtime_vs_ndp = runtime_vs_ndp
        self.maxvsminfair = maxvsminfair
    def __eq__(self, other):
        return ((self.k == other.k) and (self.fd == other.fd) and (self.fi == other.fi) and (self.e == other.e) and (self.j == other.j) and  (self.d == other.d) and  (self.version == other.version)) 

def get_runtime(my_folder, my_file):
    runtime = 0
    with open(my_folder + "/" + my_file) as file:
        
        for line in file:
            result = re.search(r"Max FCT: (\d+)", line)
            if result:
                runtime = int(result.group(1))

    return int(runtime)

def create_folder_clean(my_folder):
    cmd="rm -r {}".format(my_folder)
    os.system(cmd)
    cmd="mkdir {}".format(my_folder)
    os.system(cmd)

link_speeds = [100000, 400000, 800000]
incast_degree_and_sizes = ["incast_128_8_64.bin", "incast_128_32_64.bin", "incast_128_64_64.bin", "incast_128_8_100.bin", "incast_128_32_100.bin", "incast_128_64_100.bin", "incast_128_8_512.bin", "incast_128_32_512.bin", "incast_128_64_512.bin", "incast_128_8_2000.bin", "incast_128_32_2000.bin", "incast_128_64_2000.bin"]
incast_degree_and_sizes = ["incast_128_8_64.bin"]
incast_degree_and_sizes = ["incast_128_8_512.bin", "incast_128_32_512.bin", "incast_128_64_512.bin", "incast_128_8_2000.bin", "incast_128_32_2000.bin", "incast_128_64_2000.bin"]


kmins = [50, 20]
use_fast_drops = [0, 1]
use_fast_incs = [0, 1]
use_exp_gains = [0, 1]
use_jitters = [0, 1]
delay_gain_values = [0, 2, 5]
algorithm_names = ["standard_trimming", "delayA"]
cwnd_and_buffer = 112500
filtered_out_value = []
good_config = []
config_considered = 0
config_run = 0
killed_config = 0
threshold = 0.07
should_skip = False

for link_speed in link_speeds:
    if (link_speed == 100000):
        cwnd_and_buffer = 112500
    elif (link_speed == 400000):
        cwnd_and_buffer = 432500
    elif (link_speed == 800000):
        cwnd_and_buffer = 882500
    for incast_degree_and_size in incast_degree_and_sizes:
        # Define FileName
        RES_FOLDER="Parameter_Analysis_{}_{}Gbps".format(incast_degree_and_size.replace(".", "_" ), link_speed)
        create_folder_clean(RES_FOLDER)
        # Run NDP and collect runtime
        FILE_NAME="ndp.tmp"
        cmd="../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -nodes 128 -cwnd {} -q {} -strat perm -linkspeed {} -mtu 2048 -seed 99 -hop_latency 700 -switch_latency 0 -goal {} > {}/{}".format(cwnd_and_buffer, cwnd_and_buffer, link_speed, incast_degree_and_size, RES_FOLDER, FILE_NAME)
        #print(cmd)
        os.system(cmd)
        cmd="python3 generate_report.py --input_file={} --folder={} --parameter_analysis=1 --complex_name={}".format(FILE_NAME, RES_FOLDER, FILE_NAME)
        #print(cmd)
        os.system(cmd)
        report_file_name = "GeneratedReport{}.tmp".format(FILE_NAME)
        ndp_best_runtime = get_runtime(RES_FOLDER, report_file_name)
        cmd="rm {}/{}".format(RES_FOLDER, FILE_NAME)

        # STrack
        FILE_NAME="LinkSpeed{}_Type{}_Version{}_KMin{}_UseFastDrop{}_UseFastIncrease{}_DoExpGain{}_DoJitter{}_DelayGainvalue{}".format(link_speed, incast_degree_and_size.replace(".", "_" ), "delayD", 20, 1, 1, 1, 1, 1)
        report_file_name = "GeneratedReport{}.tmp".format(FILE_NAME)
        cmd="../sim/datacenter/htsim_uec_entry_modern -o uec_entry -algorithm {} -nodes 128 -q {} -strat perm -kmin {} -kmax 80 -target_rtt_percentage_over_base {} -delay_gain_value_med_inc {} -do_jitter {} -jitter_value_med_inc {} -use_fast_increase {} -fast_drop {} -do_exponential_gain {} -gain_value_med_inc {} -linkspeed {} -mtu 2048 -seed 99 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 0 -goal {} -ignore_ecn_data 1 -ignore_ecn_ack 1 -number_entropies -1 > {}/{}".format("delayD", cwnd_and_buffer, 20, 20, 1, 1, 1, 1, 1, 1, 1, link_speed, incast_degree_and_size, RES_FOLDER, FILE_NAME)
        #print(cmd)
        os.system(cmd)
        cmd="python3 generate_report.py --input_file={} --folder={} --parameter_analysis=1 --complex_name={}".format(FILE_NAME, RES_FOLDER, FILE_NAME)
        os.system(cmd)
        strack_best_runtime = get_runtime(RES_FOLDER, report_file_name)

        print("Experiment: {} - STrack {} vs NDP {} --> {}\n".format(FILE_NAME, strack_best_runtime, ndp_best_runtime, strack_best_runtime / ndp_best_runtime * 100))