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

def get_min_fct(my_folder, my_file):
    runtime = 0
    with open(my_folder + "/" + my_file) as file:
        
        for line in file:
            result = re.search(r"Min FCT: (\d+)", line)
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
incast_degree_and_sizes = ["incast_128_8_100.bin","incast_128_8_512.bin", "incast_128_32_512.bin", "incast_128_64_512.bin", "incast_128_8_2000.bin", "incast_128_32_2000.bin", "incast_128_64_2000.bin"]


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
        #print(cmd)
        os.system(cmd)
        #print()
        for algorithm_name in algorithm_names:
            for use_fast_drop in use_fast_drops:
                for use_fast_inc in use_fast_incs:
                    for use_exp_gain in use_exp_gains:
                        for use_jitter in use_jitters:
                            for delay_gain_value in delay_gain_values:
                                for kmin in kmins:
                                    for bad_config in filtered_out_value:
                                        # Check if the config we are using is bad
                                        if (bad_config.k == kmin and bad_config.fd == use_fast_drop and bad_config.fi == use_fast_inc and bad_config.e == use_exp_gain and bad_config.j == use_jitter and bad_config.d == delay_gain_value and bad_config.version == algorithm_name):
                                            should_skip = True
                                            break
                                        
                                    # Exclude bad config for kmin and delay value
                                    if ((kmin == 20 and delay_gain_value == 2) or (kmin == 50 and delay_gain_value == 5)):
                                        continue
                                    config_considered += 1
                                    if (should_skip):
                                        should_skip = False
                                        continue
                                    # Run My Exp
                                    config_run += 1
                                    FILE_NAME="LinkSpeed{}_Type{}_Version{}_KMin{}_UseFastDrop{}_UseFastIncrease{}_DoExpGain{}_DoJitter{}_DelayGainvalue{}".format(link_speed, incast_degree_and_size.replace(".", "_" ), algorithm_name, kmin, use_fast_drop, use_fast_inc, use_exp_gain, use_jitter, delay_gain_value)
                                    cmd="../sim/datacenter/htsim_uec_entry_modern -o uec_entry -algorithm {} -nodes 128 -q {} -strat perm -kmin {} -kmax 80 -target_rtt_percentage_over_base {} -delay_gain_value_med_inc {} -do_jitter {} -jitter_value_med_inc {} -use_fast_increase {} -fast_drop {} -do_exponential_gain {} -gain_value_med_inc {} -linkspeed {} -mtu 2048 -seed 99 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 0 -goal {} -ignore_ecn_data 1 -ignore_ecn_ack 1 -number_entropies -1 > {}/{}".format(algorithm_name, cwnd_and_buffer, kmin, kmin, delay_gain_value, use_jitter, 1, use_fast_inc, use_fast_drop, use_exp_gain, 1, link_speed, incast_degree_and_size, RES_FOLDER, FILE_NAME)
                                    #print(cmd)
                                    os.system(cmd)
                                    cmd="python3 generate_report.py --input_file={} --folder={} --parameter_analysis=1 --complex_name={}".format(FILE_NAME, RES_FOLDER ,FILE_NAME)
                                    #print(cmd)
                                    os.system(cmd)
                                    report_file_name = "GeneratedReport{}.tmp".format(FILE_NAME)
                                    runtime_config = get_runtime(RES_FOLDER, report_file_name)
                                    min_fct = get_min_fct(RES_FOLDER, report_file_name)
                                    cmd="rm {}/{}".format(RES_FOLDER, FILE_NAME)
                                    #print(cmd)
                                    os.system(cmd)
                                    #print()
                                    if (Configuration(kmin, use_fast_drop, use_fast_inc, use_exp_gain, use_jitter, delay_gain_value, algorithm_name) not in good_config):
                                        good_config.append(Configuration(kmin, use_fast_drop, use_fast_inc, use_exp_gain, use_jitter, delay_gain_value, algorithm_name))

                                    # Add to black list
                                    #print("MyRuntime {} vs NDP {}\n", runtime_config, ndp_best_runtime + (ndp_best_runtime * threshold))
                                    min_vs_max_fct = (int(runtime_config) - int(min_fct)) / int(min_fct) * 100
                                    if (min_vs_max_fct > 30):
                                        if (Configuration(kmin, use_fast_drop, use_fast_inc, use_exp_gain, use_jitter, delay_gain_value, algorithm_name) not in filtered_out_value):
                                            killed_config += 1
                                            print("Killing KMIN {} Us {} vs NDP {}\n".format(kmin, runtime_config, ndp_best_runtime + (ndp_best_runtime * threshold)))
                                            filtered_out_value.append(Configuration(kmin, use_fast_drop, use_fast_inc, use_exp_gain, use_jitter, delay_gain_value, algorithm_name))
        # We Are Switching config, save in the folder these results and move on
        cmd="python3 ranking.py --folder={}".format(RES_FOLDER)
        #print(cmd)
        os.system(cmd)

good_config = [x for x in good_config if x not in filtered_out_value]
print("\nConsideredConfig: {} - RunConfig {} - NotRunConfig {} - KilledConfig: {} - GoodConfig {} - UniqueGoodConfig {}\n".format(config_considered, config_run, config_considered - config_run, killed_config, config_run - killed_config, len(good_config) ))
with open("Results_Fairness_Good.txt", 'w') as f:
    # Name
    for good_ele in good_config:
        f.write('Config --> Algo: {} - KMin: {} - FastDrop: {} - FastInc: {} - DoExpGain: {} - DoJitter: {} - DelayGainValue: {}\n'.format(good_ele.version, good_ele.k, good_ele.fd, good_ele.fi, good_ele.e, good_ele.j, good_ele.d)) 

with open("Results_Fairness_Bad.txt", 'w') as f:
    # Name
    for bad_ele in filtered_out_value:
        f.write('Config --> Algo: {} - KMin: {} - FastDrop: {} - FastInc: {} - DoExpGain: {} - DoJitter: {} - DelayGainValue: {}\n'.format(bad_ele.version, bad_ele.k, bad_ele.fd, bad_ele.fi, bad_ele.e, bad_ele.j, bad_ele.d)) 