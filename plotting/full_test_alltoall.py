import os
import re

# Overall Variables
X_GAIN=1.25
Y_GAIN=5
W_GAIN=2
Z_GAIN=0.8
MTU_SIZE=4096
LINK_SPEED_VALUE=800000
COLLECT_DATA=0

# List Experiments
list_experiments = os.listdir('../sim/datacenter/connection_matrices/alltoall_normal_tm_16')
print(list_experiments)
list_algorithm = ["SMaRTT", "Swift", "EQDS"]
list_os = [1, 4]
sizes_full = [4096, 16384, 131072, 524288, 1048576, 4194304, 16777216, 33554432, 67108864, 134217728]
list_sizes = []
overall_list = list_experiments

print(overall_list)

def getRunString(algo, exp, os_ratio):
    num_nodes = 128
    if ("1024" in exp):
        num_nodes = 128

    if (algo == "SMaRTT"):
            string_to_run="../sim/datacenter/htsim_uec -o uec_entry -k {} -algorithm delayB -nodes {} -q 445200 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50  -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed {} -mtu {} -seed 919 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 1 -tm ../sim/datacenter/connection_matrices/alltoall_normal_tm_16/{} -ignore_ecn_data 1 -ignore_ecn_ack 1 -x_gain {}  -y_gain {} -w_gain {} -z_gain {} -bonus_drop 0.8 -collect_data {} -drop_value_buffer 1.0 > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, X_GAIN, Y_GAIN, W_GAIN, Z_GAIN, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "Swift"):
        string_to_run="../sim/datacenter/htsim_swift_trimming -k {} -ratio_os_stage_1 1 -o uec_entry -nodes {} -q 448500 -strat ecmp_host_random2_ecn -kmin 20 -target_rtt_percentage_over_base 50 -kmax 80 -linkspeed {} -mtu {} -seed 95 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 0 -tm ../sim/datacenter/connection_matrices/alltoall_normal_tm_16/{} -number_entropies 256  -collect_data {}  > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "EQDS"):
        string_to_run="../sim/datacenter/htsim_ndp -o uec_entry -nodes {} -k {} -kmin 20 -kmax 80 -ratio_os_stage_1 1 -cwnd 864500 -q 864500 -strat ecmp_host_random2_ecn -linkspeed {} -mtu {} -seed 95 -hop_latency 700 -switch_latency 0 -tm ../sim/datacenter/connection_matrices/alltoall_normal_tm_16/{} -collect_data {} -number_entropies 256 > {}/{}/P".format(num_nodes, os_ratio, LINK_SPEED_VALUE, MTU_SIZE, exp, COLLECT_DATA, FOLDER, SUB_FOLDER)

    print(string_to_run)
    return  string_to_run

def getRunStringUnbalanced(algo, exp, os_ratio):
    num_nodes = 128
    if ("1024" in exp):
        num_nodes = 128

    if (algo == "SMaRTT"):
            string_to_run="../sim/datacenter/htsim_uec -o uec_entry -k {} -algorithm delayB -nodes {} -q 445200 -strat ecmp_host_random2_ecn -number_entropies 256 -kmin 20 -kmax 80 -target_rtt_percentage_over_base 50  -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed {} -mtu {} -seed 919 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 1 -tm ../sim/datacenter/connection_matrices/alltoall_normal_tm_16/{} -ignore_ecn_data 1 -ignore_ecn_ack 1 -x_gain {}  -y_gain {} -w_gain {} -z_gain {} -bonus_drop 0.8 -collect_data {} -drop_value_buffer 1.0 > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, X_GAIN, Y_GAIN, W_GAIN, Z_GAIN, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "Swift"):
        string_to_run="../sim/datacenter/htsim_swift_trimming -k {} -ratio_os_stage_1 1 -o uec_entry -nodes {} -q 448500 -strat ecmp_host_random2_ecn -kmin 20 -target_rtt_percentage_over_base 50 -kmax 80 -linkspeed {} -mtu {} -seed 95 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 0 -tm ../sim/datacenter/connection_matrices/alltoall_normal_tm_16/{} -number_entropies 256  -collect_data {}  > {}/{}/P".format(os_ratio, num_nodes, LINK_SPEED_VALUE, MTU_SIZE, exp, COLLECT_DATA, FOLDER, SUB_FOLDER)
    elif (algo == "EQDS"):
        string_to_run="../sim/datacenter/htsim_ndp -o uec_entry -nodes {} -k {} -kmin 20 -kmax 80 -ratio_os_stage_1 1 -cwnd 864500 -q 864500 -strat ecmp_host_random2_ecn -linkspeed {} -mtu {} -seed 95 -hop_latency 700 -switch_latency 0 -tm ../sim/datacenter/connection_matrices/alltoall_normal_tm_16/{} -collect_data {} -number_entropies 256 > {}/{}/P".format(num_nodes, os_ratio, LINK_SPEED_VALUE, MTU_SIZE, exp, COLLECT_DATA, FOLDER, SUB_FOLDER)

    print(string_to_run)
    return  string_to_run

# No OverSub Experiments
for os_rat in list_os:
    if (os_rat == 1):
        FOLDER="400_ALLTOALL1_NO_OS_{}".format(1)
    else:
        FOLDER="400_ALLTOALL1_OS4:1_{}".format(1)
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