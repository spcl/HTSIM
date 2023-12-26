import os
import re
import argparse

class Configuration:
    def __init__(self, k, fd, fi, super_fi, x, z, y, bonus_drop, algo_name, runtime_vs_ndp=0, maxvsminfair=[]):
        self.k = k
        self.fd = fd
        self.fi = fi
        self.super_fi = super_fi
        self.x = x
        self.z = z
        self.y = y
        self.bonus_drop = bonus_drop
        self.algo_name = algo_name
        self.runtime_vs_ndp = runtime_vs_ndp
        self.maxvsminfair = []
        self.my_results = []
        self.my_results_raw = []
    def __eq__(self, other):
        return ((self.k == other.k) and (self.fd == other.fd) and (self.fi == other.fi) and (self.super_fi == other.super_fi) and (self.x == other.x) and  (self.z == other.z) and  (self.y == other.y)  and  (self.bonus_drop == other.bonus_drop)  and  (self.algo_name == other.algo_name)) 

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

def load_filtered_list(my_file, filter_out_list):
    with open(my_file) as file:
        
        for line in file:
            algo_tmp = ""
            kmin_tmp = ""
            fd_tmp = ""
            fi_tmp = ""
            e_tmp = ""
            j_tmp = ""
            d_tmp = ""

            result = re.search(r"Algo: (\w+)", line)
            if result:
                algo_tmp = str(result.group(1))

            result = re.search(r"KMin: (\d+)", line)
            if result:
                kmin_tmp = int(result.group(1))

            result = re.search(r"FastDrop: (\d+)", line)
            if result:
                fd_tmp = int(result.group(1))

            result = re.search(r"FastInc: (\d+)", line)
            if result:
                fi_tmp = int(result.group(1))

            result = re.search(r"DoExpGain: (\d+)", line)
            if result:
                e_tmp = int(result.group(1))

            result = re.search(r"DoJitter: (\d+)", line)
            if result:
                j_tmp = int(result.group(1))

            result = re.search(r"DelayGainValue: (\d+)", line)
            if result:
                d_tmp = int(result.group(1))

            tmp_config = Configuration(kmin_tmp, fd_tmp, fi_tmp, e_tmp, j_tmp, d_tmp, algo_tmp)
            filter_out_list.append(tmp_config)
    return filter_out_list

def extract_first_number(input_string):
    # Use regular expression to find the first sequence of digits in the string
    match = re.search(r'\d+', input_string)
    
    if match:
        return int(match.group())  # Convert the matched string to an integer
    else:
        return None  # Return None if no number was found

# Parser
parser = argparse.ArgumentParser()
parser.add_argument('--input_filter', dest='input_filter', type=str, help='FInput file name to use as filter')
args = parser.parse_args()

link_speeds = [400000]
incast_degree_and_sizes = ["incast_128_8_64.bin", "incast_128_32_64.bin", "incast_128_64_64.bin", "incast_128_8_100.bin", "incast_128_32_100.bin", "incast_128_64_100.bin", "incast_128_8_512.bin", "incast_128_32_512.bin", "incast_128_64_512.bin", "incast_128_8_2000.bin", "incast_128_32_2000.bin", "incast_128_64_2000.bin"]
incast_degree_and_sizes = ["incast_128_8_512.bin"]

incast_degree_and_sizes = ["incast_128_8_100.bin","incast_128_32_100.bin","incast_128_64_100.bin","incast_128_8_512.bin", "incast_128_32_512.bin", "incast_128_64_512.bin", "incast_128_8_2000.bin", "incast_128_32_2000.bin", "incast_128_64_2000.bin"]
incast_degree_and_sizes = ["incast_128_8_512.bin", "incast_128_32_512.bin", "incast_128_64_512.bin", "incast_128_8_2000.bin", "incast_128_32_2000.bin", "incast_128_64_2000.bin"]
incast_degree_and_sizes = ["incast_128_8_512.bin"]

incast_degree_and_sizes = ["incast_128_32_100.bin","incast_128_8_512.bin", "incast_128_32_512.bin", "incast_128_8_2000.bin", "incast_128_32_2000.bin"]


kmins = [20]
use_fast_drops = [1]
use_fast_incs = [0, 1]
use_exp_gains = [0, 1]
use_jitters = [0, 1]
delay_gain_values = [0, 2, 5]
algorithm_names = ["delayB"]
cwnd_and_buffer = 112500
filtered_out_value = []
good_config = []
config_considered = 0
config_run = 0
killed_config = 0

should_skip = False
list_experiments = []

threshold = 0.03
rtt_target_fixed = 50
w = 4
kmin = 20

use_super_fast_increases = [0, 1]
x_list = [0.20, 0.25, 0.35, 0.45]
z_list = [0.5, 0.75, 1.0, 2.0]
y_list = [1, 2, 3, 4]
bonus_drops = [1.00]


if (args.input_filter is not None):
    filtered_out_value = load_filtered_list(args.input_filter, filtered_out_value)


for link_speed in link_speeds:

    if (link_speed == 100000):
        cwnd_and_buffer = 118500
    elif (link_speed == 400000):
        cwnd_and_buffer = 432500
    elif (link_speed == 800000):
        cwnd_and_buffer = 882500

    for incast_degree_and_size in incast_degree_and_sizes:
        # Define FileName
        RES_FOLDER="Parameter_Analysis_Fairness_{}_{}Gbps".format(incast_degree_and_size.replace(".", "_" ), link_speed)
        list_experiments.append("{} - {} ".format(incast_degree_and_size.replace(".", "_" ), link_speed))
        create_folder_clean(RES_FOLDER)
        # Run NDP and collect runtime
        FILE_NAME="ndp.tmp"
        cmd="../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -nodes 128 -cwnd {} -q {} -strat perm -linkspeed {} -mtu 2048 -seed 99 -hop_latency 700 -switch_latency 0 -goal {} > {}/{}".format(cwnd_and_buffer, cwnd_and_buffer, link_speed, incast_degree_and_size, RES_FOLDER, FILE_NAME)
        #print(cmd)
        os.system(cmd)
        cmd="python3 generate_report.py --input_file={} --folder={} --parameter_analysis=1 --complex_name={}".format(FILE_NAME, RES_FOLDER, FILE_NAME)
        os.system(cmd)
        report_file_name = "GeneratedReport{}.tmp".format(FILE_NAME)
        ndp_best_runtime = get_runtime(RES_FOLDER, report_file_name)
        print("Best NDP --> {}\n".format(ndp_best_runtime))
        cmd="rm {}/{}".format(RES_FOLDER, FILE_NAME)
        os.system(cmd)
       
        for algorithm_name in algorithm_names:
            for use_fast_drop in use_fast_drops:
                for use_fast_inc in use_fast_incs:
                    for use_super_fast_increase in use_super_fast_increases:
                        for x in x_list:
                            for z in z_list:
                                for y in y_list:
                                    for bonus_drop in bonus_drops:
                                        for bad_config in filtered_out_value:
                                            # Check if the config we are using is bad
                                            if (bad_config.k == kmin and bad_config.fd == use_fast_drop and bad_config.fi == use_fast_inc and bad_config.super_fi == use_super_fast_increase and bad_config.x == x and bad_config.z == z and bad_config.y == y  and bad_config.bonus_drop == bonus_drop  and bad_config.algo_name == algorithm_name):
                                                should_skip = True
                                                break
                                            
                                        config_considered += 1
                                        if (should_skip):
                                            should_skip = False
                                            continue
                                        # Run My Exp
                                        config_run += 1
                                        FILE_NAME="LinkSpeed{}_Type{}_Version{}_KMin{}_UseFastDrop{}_UseFastIncrease{}_DoSuperFastInc{}_X{}_Z{}_Y{}_BonusDrop{}".format(link_speed, incast_degree_and_size.replace(".", "_" ), algorithm_name, kmin, use_fast_drop, use_fast_inc, use_super_fast_increase, x, y, z, bonus_drop)
                                        FILE_NAME = FILE_NAME.replace(".", "")
                                        nodes_num = extract_first_number(incast_degree_and_size)
                                        cmd="../sim/datacenter/htsim_uec_entry_modern -o uec_entry -algorithm {} -nodes {} -q {} -strat perm -kmin {} -kmax 80 -target_rtt_percentage_over_base {}  -use_fast_increase {} -use_super_fast_increase {} -fast_drop {} -linkspeed {} -mtu 2048 -seed 99 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 0 -goal {} -ignore_ecn_data 1 -ignore_ecn_ack 1 -number_entropies -1 -x_gain {} -y_gain {} -w_gain {} -z_gain {} -bonus_drop {} > {}/{}".format(algorithm_name, nodes_num, cwnd_and_buffer, kmin, rtt_target_fixed, use_fast_inc, use_super_fast_increase, use_fast_drop,  link_speed, incast_degree_and_size, x, y, w, z, bonus_drop, RES_FOLDER, FILE_NAME)
                                        #print(cmd)
                                        os.system(cmd)
                                        

                                        cmd="python3 generate_report.py --input_file={} --folder={} --parameter_analysis=1 --complex_name={}".format(FILE_NAME, RES_FOLDER ,FILE_NAME)
                                        os.system(cmd)
                                        report_file_name = "GeneratedReport{}.tmp".format(FILE_NAME)
                                        runtime_config = get_runtime(RES_FOLDER, report_file_name)
                                        cmd="rm {}/{}".format(RES_FOLDER, FILE_NAME)
                                        os.system(cmd)
                                        #print()

                                        if (Configuration(kmin, use_fast_drop, use_fast_inc, use_super_fast_increase, x, z, y, bonus_drop, algorithm_name) not in good_config):
                                            good_config.append(Configuration(kmin, use_fast_drop, use_fast_inc, use_super_fast_increase, x, z, y, bonus_drop, algorithm_name))
                                            #print("GoodConfig Scenario {} -- Killing FastDrop {} Us {} vs NDP {}\n".format(incast_degree_and_size.replace(".", "_" ), use_fast_drop, runtime_config, ndp_best_runtime + (ndp_best_runtime * threshold)))
                                        # Add to black list
                                        #print("MyRuntime {} vs NDP {}\n", runtime_config, ndp_best_runtime + (ndp_best_runtime * threshold))
                                        min_fct = get_min_fct(RES_FOLDER, report_file_name)
                                        min_vs_max_fct = (int(runtime_config) - int(min_fct)) / int(min_fct) * 100
                                        if (min_vs_max_fct > 30):
                                            if (Configuration(kmin, use_fast_drop, use_fast_inc, use_super_fast_increase, x, z, y, bonus_drop, algorithm_name) not in filtered_out_value):
                                                killed_config += 1
                                                print("Killing Config, {}". format(min_vs_max_fct))
                                                filtered_out_value.append(Configuration(kmin, use_fast_drop, use_fast_inc, use_super_fast_increase, x, z, y, bonus_drop, algorithm_name))
                                                for idx, config in enumerate(filtered_out_value):
                                                    if (config.k == kmin and config.fd == use_fast_drop and config.fi == use_fast_inc and config.super_fi == use_super_fast_increase and config.x == x and config.z == z and config.y == y  and config.bonus_drop == bonus_drop  and config.algo_name == algorithm_name):
                                                        filtered_out_value[idx].my_results_raw.append(runtime_config)
                                                        filtered_out_value[idx].my_results.append(round(((runtime_config / ndp_best_runtime) - 1) * 100,2))
                                                        filtered_out_value[idx].maxvsminfair.append(min_vs_max_fct)
                                        else:
                                            for idx, config in enumerate(good_config):
                                                if (config.k == kmin and config.fd == use_fast_drop and config.fi == use_fast_inc and config.super_fi == use_super_fast_increase and config.x == x and config.z == z and config.y == y  and config.bonus_drop == bonus_drop  and config.algo_name == algorithm_name):
                                                    good_config[idx].my_results_raw.append(runtime_config)
                                                    print("Good Config!\n")
                                                    good_config[idx].my_results.append(round(((runtime_config / ndp_best_runtime) - 1) * 100,2))
                                                    good_config[idx].maxvsminfair.append(min_vs_max_fct)
        # We Are Switching config, save in the folder these results and move on
        cmd="python3 ranking.py --folder={}".format(RES_FOLDER)
        #print(cmd)
        os.system(cmd)

# FIlter out bad ones
good_config = [x for x in good_config if x not in filtered_out_value]
print("\nConsideredConfig: {} - RunConfig {} - NotRunConfig {} - KilledConfig: {} - GoodConfig {} - UniqueGoodConfig {}\n".format(config_considered, config_run, config_considered - config_run, killed_config, config_run - killed_config, len(good_config) ))
with open("Results_Fairness_Good_400.txt", 'w') as f:
    f.write("Experiments order: {}\n\n".format(list_experiments))
    for good_ele in good_config:
        f.write('Config --> Algo: {} - KMin: {} - FastDrop: {} - FastInc: {} - SuperFastInc: {} - X: {} - Y: {} - W: {} - Z: {} - BonusDrop: {} -. Res {} {} {}\n'.format(good_ele.algo_name, good_ele.k, good_ele.fd, good_ele.fi, good_ele.super_fi, good_ele.x, good_ele.y, 4, good_ele.z, good_ele.bonus_drop, good_ele.my_results, good_ele.my_results_raw, good_ele.maxvsminfair)) 

with open("Results_Fairness_Bad_400.txt", 'w') as f:
    f.write("Experiments order: {}\n\n".format(list_experiments))
    for bad_ele in filtered_out_value:
        f.write('Config --> Algo: {} - KMin: {} - FastDrop: {} - FastInc: {} - SuperFastInc: {} - X: {} - Y: {} - W: {} - Z: {} - BonusDrop: {} -. Res {} {} {}\n'.format(bad_ele.algo_name, bad_ele.k, bad_ele.fd, bad_ele.fi, bad_ele.super_fi, bad_ele.x, bad_ele.y, 4, bad_ele.z, bad_ele.bonus_drop, bad_ele.my_results, bad_ele.my_results_raw, bad_ele.maxvsminfair)) 