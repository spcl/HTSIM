import os
import re

# Overall Variables
X_GAIN=0.5
Y_GAIN=2.5
W_GAIN=2
Z_GAIN=0.8
MTU_SIZE=4096
LINK_SPEED_VALUE=400000
COLLECT_DATA=0

# List Experiments
list_experiments = os.listdir('../sim/lgs/input/allreduce_ring')
print(list_experiments)
list_algorithm = ["SMaRTT", "SMaRTT_ECN", "SMaRTT_RTT", "Swift", "EQDS"]
list_os = [1, 4]
sizes_normal = [4096, 1048576, 16777216, 134217728]
sizes_full = [4096, 16384, 131072, 524288, 1048576, 4194304, 16777216, 33554432, 67108864, 134217728]
list_sizes = []
overall_list = []

for exp in list_experiments:
    pattern = r'\d+'  
    third_number = 0
    matches = re.findall(pattern, exp)
    if len(matches) >= 2:
        third_number = int(matches[1])
        if (third_number in sizes_normal):
            overall_list.append(exp)

#overall_list = [['incast_1024_32_512000.bin', 'incast_1024_16_512000.bin', 'incast_1024_8_512000.bin', 'incast_1024_4_512000.bin', 'incast_1024_100_512000.bin', 'incast_1024_2_512000.bin']]
print(overall_list)

def getRunString(algo, exp, os_ratio):
    num_nodes = 1024
    if ("1024" in exp):
        num_nodes = 1024

    if (algo == "SMaRTT"):
            string_to_run="../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k {} -algorithm delayB -nodes {} -q 445200 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50  -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed {} -mtu {} -seed 919 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 1 -goal allreduce_ring/{} -ignore_ecn_data 1 -ignore_ecn_ack 1 -x_gain {}  -y_gain {} -w_gain {} -z_gain {} -bonus_drop 0.8 -collect_data {} -drop_value_buffer 1.0 > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, X_GAIN, Y_GAIN, W_GAIN, Z_GAIN, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "SMaRTT_ECN"):
        string_to_run="../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k {} -algorithm ecn -nodes {} -q 445200 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50  -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed {} -mtu {} -seed 919 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 1 -goal allreduce_ring/{} -ignore_ecn_data 1 -ignore_ecn_ack 1 -x_gain {}  -y_gain {} -w_gain {} -z_gain {} -bonus_drop 0.8 -collect_data {} -drop_value_buffer 1.0 > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, X_GAIN, Y_GAIN, W_GAIN, Z_GAIN, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "SMaRTT_RTT"):
        string_to_run="../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k {} -algorithm rtt -nodes {} -q 445200 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50  -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed {} -mtu {} -seed 919 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 1 -goal allreduce_ring/{} -ignore_ecn_data 1 -ignore_ecn_ack 1 -x_gain {}  -y_gain {} -w_gain {} -z_gain {} -bonus_drop 0.8 -collect_data {} -drop_value_buffer 1.0 > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, X_GAIN, Y_GAIN, W_GAIN, Z_GAIN, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "Swift"):
        string_to_run="../sim/datacenter/htsim_swift_entry -k {} -ratio_os_stage_1 1 -o uec_entry -nodes {} -q 448500 -strat ecmp_host_random2_ecn -kmin 20 -target_rtt_percentage_over_base 50 -kmax 80 -linkspeed {} -mtu {} -seed 95 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 0 -goal allreduce_ring/{} -number_entropies 256  -collect_data {}  > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "EQDS"):
        string_to_run="../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -nodes {} -k {} -kmin 20 -kmax 80 -ratio_os_stage_1 1 -cwnd 444600 -q 444600 -strat ecmp_host_random2_ecn -linkspeed {} -mtu {} -seed 95 -hop_latency 700 -switch_latency 0 -goal allreduce_ring/{} -collect_data {} -number_entropies 256 > {}/{}/P".format(num_nodes, os_ratio, LINK_SPEED_VALUE, MTU_SIZE, exp, COLLECT_DATA, FOLDER, SUB_FOLDER)

    print(string_to_run)
    return  string_to_run

# No OverSub Experiments
for os_rat in list_os:
    if (os_rat == 1):
        FOLDER="400_ALLREDUCERING_NO_OS_{}".format(1)
    else:
        FOLDER="400_ALLREDUCERING_OS4:1_{}".format(1)
    CMD="rm -r {}".format(FOLDER)
    os.system(CMD)
    CMD="mkdir {}".format(FOLDER)
    os.system(CMD)

    for experiment in overall_list:
        # Create Folder
        SUB_FOLDER=experiment.replace(".", "_" )
        CMD="rm -r {}/{}".format(FOLDER, SUB_FOLDER)
        os.system(CMD)
        CMD="mkdir {}/{}".format(FOLDER, SUB_FOLDER)
        os.system(CMD)
        for algo in list_algorithm:

            # Run Experiment
            CMD=getRunString(algo, experiment, os_rat)
            os.system(CMD)
            #os.system(CMD)
            CMD="python3 generate_report.py --input_file=P --folder={}/{}".format(FOLDER, SUB_FOLDER)
            os.system(CMD)

        # Plot Overall Results
        CMD="python3 fct.py --input_file=P --folder={}/{} --name={}".format(FOLDER, SUB_FOLDER, experiment)
        print(CMD)
        os.system(CMD)

    CMD="python3 comparison.py --folder={} --link_speed=400 --latency=700  --os=1".format(FOLDER)
    print(CMD)
    os.system(CMD)