import os
import re
import matplotlib.pyplot as plt
import statsmodels.api as sm
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
import seaborn as sns
import pandas as pd
from matplotlib.ticker import ScalarFormatter, MaxNLocator
from complete_plot import run as run_complete
from pathlib import Path


def execute_string(string_to_run, experiment, out_name):
    os.chdir("../../sim/datacenter/")
    os.system(string_to_run)
    os.chdir("../../plotting/evaluation_failures/")

    os.system("cp -a ../../sim/output/cwd/. experiments2MB/{}/cwd/".format(experiment))
    os.system("cp -a ../../sim/output/rtt/. experiments2MB/{}/rtt/".format(experiment))
    os.system("cp -a ../../sim/output/queue/. experiments2MB/{}/queue_size_normalized/".format(experiment))
    os.system("cp -a ../../sim/output/switch_drops/. experiments2MB/{}/switch_drops/".format(experiment))
    os.system("cp -a ../../sim/output/cable_drops/. experiments2MB/{}/cable_drops/".format(experiment))
    os.system("cp -a ../../sim/output/switch_failures/. experiments2MB/{}/switch_failures/".format(experiment))
    os.system("cp -a ../../sim/output/cable_failures/. experiments2MB/{}/cable_failures/".format(experiment))
    os.system("cp -a ../../sim/output/routing_failed_switch/. experiments2MB/{}/routing_failed_switch/".format(experiment))
    os.system("cp -a ../../sim/output/routing_failed_cable/. experiments2MB/{}/routing_failed_cable/".format(experiment))
    os.system("cp -a ../../sim/output/switch_degradations/. experiments2MB/{}/switch_degradations/".format(experiment))
    os.system("cp -a ../../sim/output/cable_degradations/. experiments2MB/{}/cable_degradations/".format(experiment))
    os.system("cp -a ../../sim/output/random_packet_drops/. experiments2MB/{}/random_packet_drops/".format(experiment))
    os.system("cp -a ../../sim/output/packet_seq/. experiments2MB/{}/packet_seq/".format(experiment))
    os.system("cp -a ../../sim/datacenter/{}.txt experiments2MB/{}/{}.txt".format(out_name, experiment,out_name))


def getListFCT(name_file_to_use):
    temp_list = []
    number_of_flows = 0
    with open(name_file_to_use) as file:
        for line in file:
            # FCT
            result = re.search(r"Flow Completion time is (\d+.\d+)", line)
            result2 = re.search(r"Connections: (\d+)", line)

            if result:
                actual = float(result.group(1))
                temp_list.append(actual)
            if result2:
                number_of_flows = int(result2.group(1))
    if len(temp_list) != number_of_flows:
        return [0]
    else:
        return temp_list


def getNumTrimmed(name_file_to_use):
    num_nack = 0
    with open(name_file_to_use) as file:
        for line in file:
            result = re.search(r"Total NACKs: (\d+)", line)
            if result:
                num_nack += int(result.group(1))
    return num_nack


def getNrDroppedPackets(name_file_to_use):
    res = 0
    with open(name_file_to_use) as file:
        for line in file:
            result = re.search(r"Total number of dropped packets: (\d+)", line)
            if result:
                return int(result.group(1))
    return -1


def getTotalOutofOrderPackets(sequence):
    res = 0
    cur_max = 0
    for index, row in sequence.iterrows():
        val = row['Index']
        if val < cur_max:
            res += 1
        if val > cur_max:
            cur_max = val
    return res

def getMaxTotalOutOfOrderPackets(experiment):
    os.chdir("experiments2MB/" + experiment+"/")
    pathlist = Path("packet_seq").glob("**/*.txt")
    list = []
    for files in sorted(pathlist):
        path_in_str = str(files)
        df = pd.read_csv(path_in_str, names=["Index"], header=None, index_col=False, sep=",")
        cur = getTotalOutofOrderPackets(df)
        list.append(cur)
    os.chdir("../..")
    return list

def getOutofOrderRatio(experiment):
    os.chdir("experiments2MB/" + experiment+"/")
    pathlist = Path("packet_seq").glob("**/*.txt")
    list = []
    for files in sorted(pathlist):
        path_in_str = str(files)
        df = pd.read_csv(path_in_str, names=["Index"], header=None, index_col=False, sep=",")
        cur = getTotalOutofOrderPackets(df)
        res = cur/len(df)
        list.append(res)
    os.chdir("../..")
    return list

def getDistance(sequence):
    cur_max = 0
    max_distance = 0
    for index, row in sequence.iterrows():
        val = row['Index']
        if val < cur_max:
            distance = abs(index - val)
            if distance > max_distance:
                max_distance = distance
        if val > cur_max:
            cur_max = val
    return max_distance


def getOutOfOrderDistance(experiment):
    os.chdir("experiments2MB/" + experiment+"/")
    pathlist = Path("packet_seq").glob("**/*.txt")
    list = []
    for files in sorted(pathlist):
        path_in_str = str(files)
        df = pd.read_csv(path_in_str, names=["Index"], header=None, index_col=False, sep=",")
        cur = getDistance(df)
        list.append(cur)
    os.chdir("../..")
    return list


def run(
    short_title, title, failures_input, msg_size, nodes, topology, connection_matrix
):

    print("Running 2MB: {}".format(title))

    os.system("mkdir experiments2MB/{}".format(short_title))
    os.system("mkdir to_upload2MB/{}".format(short_title))


    # REPS
    balancer = "reps"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer)
    filename = "experiments2MB/{}/{}.txt".format(short_title,balancer)
    list_reps = getListFCT(filename)
    if(list_reps == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_reps = getNumTrimmed(filename)
    num_lost_packets_reps = getNrDroppedPackets(filename)
    if(num_lost_packets_reps == -1):
        print("Failed-Packets: {}".format(string_to_run))
    list_total_out_of_order_reps = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_reps = getOutofOrderRatio(short_title)
    list_out_of_order_distance_reps = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title,"2MB")
    complete_plot.write_image("experiments2MB/{}/complete_plot_{}.pdf".format(short_title,balancer))
    complete_plot.write_image("to_upload2MB/{}/complete_plot_{}.pdf".format(short_title,balancer))
    # print(
    #     "REPS: Flow Diff {} - Total {}".format(
    #         max(list_reps) - min(list_reps), max(list_reps)
    #     )
    # )
        ## REPS without failures
    balancer = "reps"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 > {}NoFailures.txt".format(
            balancer, nodes, topology, connection_matrix, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer+"NoFailures")
    filename = "experiments2MB/{}/{}NoFailures.txt".format(short_title,balancer)
    list_repsNoFailures = getListFCT(filename)
    if(list_repsNoFailures == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_repsNoFailures = getNumTrimmed(filename)
    list_total_out_of_order_repsNoFailures = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_repsNoFailures = getOutofOrderRatio(short_title)
    list_out_of_order_distance_repsNoFailures = getOutOfOrderDistance(short_title)
    # print(
    #     "REPS NoFailures : Flow Diff {} - Total {}".format(
    #         max(list_repsNoFailures) - min(list_repsNoFailures), max(list_repsNoFailures)
    #     )
    # )


    #REPS Circular
    balancer = "reps_circular"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer)
    filename = "experiments2MB/{}/{}.txt".format(short_title,balancer)
    list_repsC = getListFCT(filename)
    if(list_repsC == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_repsC = getNumTrimmed(filename)
    num_lost_packets_repsC = getNrDroppedPackets(filename)
    if(num_lost_packets_repsC == -1):
        print("Failed-Packets: {}".format(string_to_run))
    list_total_out_of_order_repsC = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_repsC = getOutofOrderRatio(short_title)
    list_out_of_order_distance_repsC = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title,"2MB")
    complete_plot.write_image("experiments2MB/{}/complete_plot_{}.pdf".format(short_title,balancer))
    complete_plot.write_image("to_upload2MB/{}/complete_plot_{}.pdf".format(short_title,balancer))
    # print(
    #     "REPS circular: Flow Diff {} - Total {}".format(
    #         max(list_repsC) - min(list_repsC), max(list_repsC)
    #     )
    # )
        ## REPS Circular without failures
    balancer = "reps_circular"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 > {}NoFailures.txt".format(
            balancer, nodes, topology, connection_matrix, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer+"NoFailures")
    filename = "experiments2MB/{}/{}NoFailures.txt".format(short_title,balancer)
    list_repsCNoFailures = getListFCT(filename)
    if(list_repsCNoFailures == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_repsCNoFailures = getNumTrimmed(filename)
    list_total_out_of_order_repsCNoFailures = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_repsCNoFailures = getOutofOrderRatio(short_title)
    list_out_of_order_distance_repsCNoFailures = getOutOfOrderDistance(short_title)
    # print(
    #     "REPS circular NoFailures: Flow Diff {} - Total {}".format(
    #         max(list_repsCNoFailures) - min(list_repsCNoFailures), max(list_repsCNoFailures)
    #     )
    # )

    #REPS Circular without freezing
    balancer = "reps_circular"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {}   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}NoFreezing.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer+"NoFreezing")
    filename = "experiments2MB/{}/{}NoFreezing.txt".format(short_title,balancer)
    list_repsCF = getListFCT(filename)
    if(list_repsCF == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_repsCF = getNumTrimmed(filename)
    num_lost_packets_repsCF = getNrDroppedPackets(filename)
    if(num_lost_packets_repsCF == -1):
        print("Failed-Packets: {}".format(string_to_run))
    list_total_out_of_order_repsCF = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_repsCF = getOutofOrderRatio(short_title)
    list_out_of_order_distance_repsCF = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title,"2MB")
    complete_plot.write_image("experiments2MB/{}/complete_plot_{}NoFreezing.pdf".format(short_title,balancer))
    complete_plot.write_image("to_upload2MB/{}/complete_plot_{}NoFreezing.pdf".format(short_title,balancer))
    # print(
    #     "repsCF: Flow Diff {} - Total {}".format(
    #         max(list_repsCF) - min(list_repsCF), max(list_repsCF)
    #     )
    # )
        ## REPS Circular without freezing without failures
    balancer = "reps_circular"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {}   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 > {}NoFreezingNoFailures.txt".format(
            balancer, nodes, topology, connection_matrix, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer+"NoFreezing"+"NoFailures")
    filename = "experiments2MB/{}/{}NoFreezingNoFailures.txt".format(short_title,balancer)
    list_repsCFNoFailures = getListFCT(filename)
    if(list_repsCFNoFailures == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_repsCFNoFailures = getNumTrimmed(filename)
    list_total_out_of_order_repsCFNoFailures = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_repsCFNoFailures = getOutofOrderRatio(short_title)
    list_out_of_order_distance_repsCFNoFailures = getOutOfOrderDistance(short_title)
    # print(
    #     "repsCF NoFailures: Flow Diff {} - Total {}".format(
    #         max(list_repsCFNoFailures) - min(list_repsCFNoFailures), max(list_repsCFNoFailures)
    #     )
    # )


    #Spraying
    balancer = "spraying"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer)
    filename = "experiments2MB/{}/{}.txt".format(short_title,balancer)
    list_spraying = getListFCT(filename)
    if(list_spraying == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_spraying = getNumTrimmed(filename)
    num_lost_packets_spraying = getNrDroppedPackets(filename)
    if(num_lost_packets_spraying == -1):
        print("Failed-Packets: {}".format(string_to_run))
    list_total_out_of_order_spraying = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_spraying = getOutofOrderRatio(short_title)
    list_out_of_order_distance_spraying = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title,"2MB")
    complete_plot.write_image("experiments2MB/{}/complete_plot_{}.pdf".format(short_title,balancer))
    complete_plot.write_image("to_upload2MB/{}/complete_plot_{}.pdf".format(short_title,balancer))
    # print(
    #     "Spraying: Flow Diff {} - Total {}".format(
    #         max(list_spraying) - min(list_spraying), max(list_spraying)
    #     )
    # )
        ## Spraying without failures
    balancer = "spraying"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps   -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 > {}NoFailures.txt".format(
            balancer, nodes, topology, connection_matrix, balancer
        )
    # print(string_to_run)
    execute_string(string_to_run, short_title, balancer+"NoFailures")
    filename = "experiments2MB/{}/{}NoFailures.txt".format(short_title,balancer)
    list_sprayingNoFailures = getListFCT(filename)
    if(list_sprayingNoFailures == [0]):
        print("Failed-Flows: {}".format(string_to_run))
    num_nack_sprayingNoFailures = getNumTrimmed(filename)
    list_total_out_of_order_sprayingNoFailures = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_sprayingNoFailures = getOutofOrderRatio(short_title)
    list_out_of_order_distance_sprayingNoFailures = getOutOfOrderDistance(short_title)
    # print(
    #     "Spraying: Flow Diff {} - Total {}".format(
    #         max(list_spraying) - min(list_spraying), max(list_spraying)
    #     )
    # )

    list_lost_packets = [
        num_lost_packets_reps,
        num_lost_packets_repsC,
        num_lost_packets_repsCF,
        num_lost_packets_spraying,
    ]

    list_fct = [list_reps, list_repsC, list_repsCF, list_spraying]

    list_max = [max(list_reps), max(list_repsC), max(list_repsCF), max(list_spraying)]
    list_baseline_max = [max(list_repsNoFailures), max(list_repsCNoFailures), max(list_repsCFNoFailures), max(list_sprayingNoFailures)]

    list_nacks = [num_nack_reps, num_nack_repsC, num_nack_repsCF, num_nack_spraying]
    list_baseline_nacks = [num_nack_repsNoFailures, num_nack_repsCNoFailures, num_nack_repsCFNoFailures, num_nack_sprayingNoFailures]

    list_mean_total_out_of_order = [np.mean(list_total_out_of_order_reps), np.mean(list_total_out_of_order_repsC),np.mean(list_total_out_of_order_repsCF), np.mean(list_total_out_of_order_spraying)]
    list_baseline_mean_total_out_of_order = [np.mean(list_total_out_of_order_repsNoFailures), np.mean(list_total_out_of_order_repsCNoFailures),np.mean(list_total_out_of_order_repsCFNoFailures), np.mean(list_total_out_of_order_sprayingNoFailures)]

    list_std_total_out_of_order = [np.std(list_total_out_of_order_reps), np.std(list_total_out_of_order_repsC), np.std(list_total_out_of_order_repsCF), np.std(list_total_out_of_order_spraying)]
    list_baseline_std_total_out_of_order = [np.std(list_total_out_of_order_repsNoFailures), np.std(list_total_out_of_order_repsCNoFailures),np.std(list_total_out_of_order_repsCFNoFailures), np.std(list_total_out_of_order_sprayingNoFailures)]

    list_max_total_out_of_order = [max(list_total_out_of_order_reps), max(list_total_out_of_order_repsC),max(list_total_out_of_order_repsCF), max(list_total_out_of_order_spraying)]
    list_baseline_max_total_out_of_order = [max(list_total_out_of_order_repsNoFailures), max(list_total_out_of_order_repsCNoFailures),max(list_total_out_of_order_repsCFNoFailures), max(list_total_out_of_order_sprayingNoFailures)]


    list_mean_total_out_of_order_ratio = [np.mean(list_out_of_order_ratio_reps), np.mean(list_out_of_order_ratio_repsC),np.mean(list_out_of_order_ratio_repsCF), np.mean(list_out_of_order_ratio_spraying)]
    list_baseline_mean_total_out_of_order_ratio = [np.mean(list_out_of_order_ratio_repsNoFailures), np.mean(list_out_of_order_ratio_repsCNoFailures),np.mean(list_out_of_order_ratio_repsCFNoFailures), np.mean(list_out_of_order_ratio_sprayingNoFailures)]

    list_std_total_out_of_order_ratio = [np.std(list_out_of_order_ratio_reps), np.std(list_out_of_order_ratio_repsC),np.std(list_out_of_order_ratio_repsCF), np.std(list_out_of_order_ratio_spraying)]
    list_baseline_std_total_out_of_order_ratio = [np.std(list_out_of_order_ratio_repsNoFailures), np.std(list_out_of_order_ratio_repsCNoFailures),np.std(list_out_of_order_ratio_repsCFNoFailures), np.std(list_out_of_order_ratio_sprayingNoFailures)]

    list_max_total_out_of_order_ratio = [max(list_out_of_order_ratio_reps), max(list_out_of_order_ratio_repsC), max(list_out_of_order_ratio_repsCF), max(list_out_of_order_ratio_spraying)]
    list_baseline_max_total_out_of_order_ratio = [max(list_out_of_order_ratio_repsNoFailures), max(list_out_of_order_ratio_repsCNoFailures), max(list_out_of_order_ratio_repsCFNoFailures), max(list_out_of_order_ratio_sprayingNoFailures)]


    list_mean_total_out_of_order_distance = [np.mean(list_out_of_order_distance_reps), np.mean(list_out_of_order_distance_repsC),np.mean(list_out_of_order_distance_repsCF), np.mean(list_out_of_order_distance_spraying)]
    list_baseline_mean_total_out_of_order_distance = [np.mean(list_out_of_order_distance_repsNoFailures), np.mean(list_out_of_order_distance_repsCNoFailures),np.mean(list_out_of_order_distance_repsCFNoFailures), np.mean(list_out_of_order_distance_sprayingNoFailures)]

    list_std_total_out_of_order_distance = [np.std(list_out_of_order_distance_reps), np.std(list_out_of_order_distance_repsC),np.std(list_out_of_order_distance_repsCF), np.std(list_out_of_order_distance_spraying)]
    list_baseline_std_total_out_of_order_distance = [np.std(list_out_of_order_distance_repsNoFailures), np.std(list_out_of_order_distance_repsCNoFailures),np.std(list_out_of_order_distance_repsCFNoFailures), np.std(list_out_of_order_distance_sprayingNoFailures)]

    list_max_total_out_of_order_distance = [max(list_out_of_order_distance_reps), max(list_out_of_order_distance_repsC),max(list_out_of_order_distance_repsCF), max(list_out_of_order_distance_spraying)]
    list_baseline_max_total_out_of_order_distance = [max(list_out_of_order_distance_repsNoFailures), max(list_out_of_order_distance_repsCNoFailures),max(list_out_of_order_distance_repsCFNoFailures), max(list_out_of_order_distance_sprayingNoFailures)]


    list_labels = ["REPS", "REPS Circular", "REPS Circular\n without freezing", "Spraying"]

    # Combine all data into a list of lists

    all_data = list_fct
    labels = list_labels
    # Create a list of labels for each dataset

    # Initialize an empty list to store cumulative probabilities

    unit = "ms"
    smaller_font = 15

    cumulative_probabilities = []

    # Step 2: Sort and compute cumulative probabilities for each dataset
    for data in all_data:
        data.sort()
        n = len(data)
        cumulative_probabilities.append(np.arange(1, n + 1) / n)

    # Step 3: Plot the CDFs using Seaborn
    plt.figure(figsize=(8, 4.2))

    for i, data in enumerate(all_data):
        ax = sns.lineplot(
            x=data, y=cumulative_probabilities[i], label=labels[i], linewidth=3.3
        )

    ax.set_xticks(ax.get_xticks())
    ax.set_yticks(ax.get_yticks())
    ax.set(ylim=(0, 1))
    ax.tick_params(labelsize=10)

    ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize=smaller_font)
    ax.set_yticklabels([str(round(i, 1)) for i in ax.get_yticks()], fontsize=16)

    # Add a vertical dashed line at x=1000 with lower opacity
    # best_time = (msg_size * 32 / 100 / 1000) + 12
    # print("Best Time {}".format(best_time))
    # plt.axvline(x=best_time, linestyle="--", color="#3b3b3b", alpha=0.55, linewidth=3)

    # Set labels and title
    plt.xlabel("Flow Completion Time ({})".format(unit), fontsize=19.5)
    plt.ylabel("CDF (%)", fontsize=19.5)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: (int(x) / 1000)))
    plt.grid()  # just add this
    plt.legend(frameon=False)

    # plt.title(title, fontsize=16.5)
    plt.tight_layout()
    plt.savefig("experiments2MB/{}/cdf.pdf".format(short_title), bbox_inches="tight")
    plt.savefig("to_upload2MB/{}/cdf.pdf".format(short_title), bbox_inches="tight")
    plt.close()

    # PLOT 2 (NACK)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 5))
    numbers = list_nacks
    labels = list_labels

    # Create a DataFrame from the lists
    data = pd.DataFrame({"Packets Trimmed": numbers, "Algorithm": labels})

    # Create a bar chart using Seaborn
    ax3 = sns.barplot(x="Algorithm", y="Packets Trimmed", data=data)
    ax3.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation

    ax3.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))

    for idx, baseline in enumerate(list_baseline_nacks):
        bar_center = ax3.patches[idx].get_x() + ax3.patches[idx].get_width() / 2.0
        ax3.plot([bar_center - 0.4, bar_center + 0.4], [baseline, baseline], color='black', linestyle='dashed', linewidth=1)


    # plt.title(title, fontsize=16.5)
    plt.grid()  # just add this

    plt.savefig("experiments2MB/{}/nack.pdf".format(short_title), bbox_inches="tight")
    plt.close()

    # PLOT 3 (COMPLETION TIME)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 5))

    numbers = list_max
    labels = list_labels
    # Create a DataFrame from the lists
    data2 = pd.DataFrame({"Runtime (us)": numbers, "Algorithm": labels})

    # Create a bar chart using Seaborn
    ax2 = sns.barplot(x="Algorithm", y="Runtime (us)", data=data2)
    ax2.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation
    ax2.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))

    for p in ax2.patches:
        ax2.annotate(
            format(int(p.get_height()), ""),
            (p.get_x() + p.get_width() / 2.0, p.get_height()),
            ha="center",
            va="center",
            xytext=(0, 7),
            textcoords="offset points",
        )

    for idx, baseline in enumerate(list_baseline_max):
        bar_center = ax2.patches[idx].get_x() + ax2.patches[idx].get_width() / 2.0
        ax2.plot([bar_center - 0.4, bar_center + 0.4], [baseline, baseline], color='black', linestyle='dashed', linewidth=1)

    # plt.title(title, fontsize=17)
    plt.grid()

    plt.savefig(
        "experiments2MB/{}/completion.pdf".format(short_title), bbox_inches="tight"
    )
    plt.savefig(
        "to_upload2MB/{}/completion.pdf".format(short_title), bbox_inches="tight"
    )
    plt.close()

    # PLOT 4 (FLOW DISTR)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 5))

    combined_data = []
    hue_list = []
    for idx, names in enumerate(labels):
        combined_data += all_data[idx]
        hue_list += [labels[idx]] * len(all_data[idx])

    # Create the violin plot
    my = sns.violinplot(x=hue_list, y=combined_data, cut=0)
    my.set_axisbelow(True)
    my.tick_params(labelsize=9.5)

    # plt.title(title, fontsize=17)
    plt.grid()  # just add this
    plt.savefig(
        "experiments2MB/{}/violin_fct.pdf".format(short_title), bbox_inches="tight"
    )
    plt.close()

    # PLOT 5 (LOST PACKETS)

    plt.figure(figsize=(7, 5))
    numbers = list_lost_packets
    labels = list_labels

    # Make bar chart
    data3 = pd.DataFrame({"Lost Packets": numbers, "Algorithm": labels})
    ax4 = sns.barplot(x="Algorithm", y="Lost Packets", data=data3)
    ax4.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation
    ax4.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))

    for p in ax4.patches:
        ax4.annotate(
            format(int(p.get_height()), ""),
            (p.get_x() + p.get_width() / 2.0, p.get_height()),
            ha="center",
            va="center",
            xytext=(0, 7),
            textcoords="offset points",
        )

    # plt.title(title, fontsize=17)
    plt.grid()
    plt.savefig(
        "experiments2MB/{}/lost_packets.pdf".format(short_title), bbox_inches="tight"
    )
    plt.savefig(
        "to_upload2MB/{}/lost_packets.pdf".format(short_title), bbox_inches="tight"
    )
    plt.close()


    # PLOT 6 (Max total out of order Packets)
    plt.figure(figsize=(7, 5))
    means = list_mean_total_out_of_order
    maxs = list_max_total_out_of_order
    errors = list_std_total_out_of_order

    means_baseline = list_baseline_mean_total_out_of_order
    maxs_baseline = list_baseline_max_total_out_of_order
    errors_baseline = list_baseline_std_total_out_of_order

    labels = list_labels

    num_bars = len(labels)
    bar_width = 0.9
    index = np.arange(num_bars)

    fig, ax = plt.subplots(figsize=(10, 6))

    bars1 = ax.bar(index - bar_width/4, means, bar_width/2, yerr=errors, capsize=5, label='With Failures')
    bars2 = ax.bar(index + bar_width/4, means_baseline, bar_width/2, yerr=errors_baseline, capsize=5, label='Without Failures')

    ax.set_xlabel('Algorithm')
    ax.set_ylabel('Total out of order packets')
    ax.set_title(title)
    ax.set_xticks(index)
    ax.set_xticklabels(labels, rotation=0, ha='center')

    def autolabel(bars, max_values, errors):
        for bar, max_val, error in zip(bars, max_values, errors):
            height = bar.get_height()
            ax.annotate(f'Max: {max_val:.2f}\nMean: {height:.2f}\nStd Dev: {error:.2f}',
                        xy=(bar.get_x() + bar.get_width() / 2, 0),
                        xytext=(0, 7),
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=6)

    autolabel(bars1, maxs, errors)
    autolabel(bars2, maxs_baseline, errors_baseline)
    plt.legend(title='Type', loc='upper right', fancybox=True, bbox_to_anchor=(1.25, 1))


    # plt.title(title, fontsize=15)
    plt.tight_layout()
    plt.grid(True)
    plt.savefig("experiments2MB/{}/total_out_of_order.pdf".format(short_title), bbox_inches="tight")
    plt.savefig("to_upload2MB/{}/total_out_of_order.pdf".format(short_title), bbox_inches="tight")
    plt.close()


    # PLOT 7 (Out of order ratio)
    means = list_mean_total_out_of_order_ratio
    maxs = list_max_total_out_of_order_ratio
    errors = list_std_total_out_of_order_ratio

    means_baseline = list_baseline_mean_total_out_of_order_ratio
    maxs_baseline = list_baseline_max_total_out_of_order_ratio
    errors_baseline = list_baseline_std_total_out_of_order_ratio

    labels = list_labels

    num_bars = len(labels)
    bar_width = 0.9
    index = np.arange(num_bars)

    fig, ax = plt.subplots(figsize=(10, 6))

    bars1 = ax.bar(index - bar_width/4, means, bar_width/2, yerr=errors, capsize=5, label='With Failures')
    bars2 = ax.bar(index + bar_width/4, means_baseline, bar_width/2, yerr=errors_baseline, capsize=5, label='Without Failures')

    ax.set_xlabel('Algorithm')
    ax.set_ylabel('Out of order ratio')
    # ax.set_title(title)
    ax.set_xticks(index)
    ax.set_xticklabels(labels, rotation=0, ha='center')

    def autolabel(bars, max_values, errors):
        for bar, max_val, error in zip(bars, max_values, errors):
            height = bar.get_height()
            ax.annotate(f'Max: {max_val:.2f}\nMean: {height:.2f}\nStd Dev: {error:.2f}',
                        xy=(bar.get_x() + bar.get_width() / 2, 0),
                        xytext=(0, 7),
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=6)

    autolabel(bars1, maxs, errors)
    autolabel(bars2, maxs_baseline, errors_baseline)
    plt.legend(title='Type', loc='upper right', fancybox=True, bbox_to_anchor=(1.25, 1))


    # plt.title(title, fontsize=15)
    plt.tight_layout()
    plt.grid(True)
    plt.savefig("experiments2MB/{}/out_of_order_ratio.pdf".format(short_title), bbox_inches="tight")
    plt.savefig("to_upload2MB/{}/out_of_order_ratio.pdf".format(short_title), bbox_inches="tight")
    plt.close()


    # PLOT 7 (Out of order Distance)
    plt.figure(figsize=(7, 5))
    means = list_mean_total_out_of_order_distance
    maxs = list_max_total_out_of_order_distance
    errors = list_std_total_out_of_order_distance

    means_baseline = list_baseline_mean_total_out_of_order_distance
    maxs_baseline = list_baseline_max_total_out_of_order_distance
    errors_baseline = list_baseline_std_total_out_of_order_distance


    labels = list_labels

    num_bars = len(labels)
    bar_width = 0.9
    index = np.arange(num_bars)

    fig, ax = plt.subplots(figsize=(10, 6))

    bars1 = ax.bar(index - bar_width/4, means, bar_width/2, yerr=errors, capsize=5, label='With Failures')
    bars2 = ax.bar(index + bar_width/4, means_baseline, bar_width/2, yerr=errors_baseline, capsize=5, label='Without Failures')

    ax.set_xlabel('Algorithm')
    ax.set_ylabel('Out of order distance')
    # ax.set_title(title)
    ax.set_xticks(index)
    ax.set_xticklabels(labels, rotation=0, ha='center')

    def autolabel(bars, max_values, errors):
        for bar, max_val, error in zip(bars, max_values, errors):
            height = bar.get_height()
            ax.annotate(f'Max: {max_val:.2f}\nMean: {height:.2f}\nStd Dev: {error:.2f}',
                        xy=(bar.get_x() + bar.get_width() / 2, 0),
                        xytext=(0, 7),
                        textcoords="offset points",
                        ha='center', va='bottom', fontsize=6)

    autolabel(bars1, maxs, errors)
    autolabel(bars2, maxs_baseline, errors_baseline)
    plt.legend(title='Type', loc='upper right', fancybox=True, bbox_to_anchor=(1.25, 1))


    # plt.title(title, fontsize=15)
    plt.tight_layout()
    plt.grid(True)
    plt.savefig("experiments2MB/{}/out_of_order_distance.pdf".format(short_title), bbox_inches="tight")
    plt.savefig("to_upload2MB/{}/out_of_order_distance.pdf".format(short_title), bbox_inches="tight")
    plt.close()
    return list_max

def main():

    os.system("rm -rf experiments2MB/")
    os.system("mkdir -p experiments2MB")
    os.system("rm -rf to_upload2MB/")
    os.system("mkdir -p to_upload2MB")

    titles = [
        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch at 10µs for 100µs",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch at 10µs for 100µs",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch at 10µs for 100µs",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One switch drops every 10th packet",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One switch drops every 10th packet",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One switch drops every 10th packet",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed cable at 10µs for 100µs",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One failed cable at 10µs for 100µs",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed cable at 10µs for 100µs",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed cable",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One failed cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed cable",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One cable drops every 10th packet",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One cable drops every 10th packet",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One cable drops every 10th packet",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One degraded switch",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One degraded switch",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One degraded switch",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One degraded cable",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One degraded cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One degraded cable",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Switch BER: 1% of packets get corrupted",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: Switch BER: 1% of packets get corrupted",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Switch BER: 1% of packets get corrupted",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Cable BER: 1% of packets get corrupted",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: Cable BER: 1% of packets get corrupted",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Cable BER: 1% of packets get corrupted",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed switches",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed switches",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed switches",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed cables",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed cables",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed cables",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% cables per Switch between UpperSwitch-CoreSwitch",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: 10% cables per Switch between UpperSwitch-CoreSwitch",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% cables per Switch between UpperSwitch-CoreSwitch",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% degraded switches",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: 10% degraded switches",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% degraded switches",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% degraded cables",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: 10% degraded cables",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% degraded cables",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Fail a new switch evey 100 µs for 50µs",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: Fail a new switch evey 100 µs for 50µs",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Fail a new switch evey 100 µs for 50µs",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Fail a new cable evey 100 µs for 50µs",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: Fail a new cable evey 100 µs for 50µs",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: Fail a new cable evey 100 µs for 50µs",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable at 10µs for 100µs",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable at 10µs for 100µs",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable at 10µs for 100µs",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable, One degraded switch&cable",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable, One degraded switch&cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: One failed switch&cable, One degraded switch&cable",

        "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed switches&cables",
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed switches&cables",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows \n Failure mode: 10% failed switches&cables",
    ]

    failures_input = [
        "fail_one_switch_after_10us_for_100us",
        "fail_one_switch_after_10us_for_100us",
        "fail_one_switch_after_10us_for_100us",

        "fail_one_switch",
        "fail_one_switch",
        "fail_one_switch",

        "one_switch_drops_every_10th_packet",
        "one_switch_drops_every_10th_packet",
        "one_switch_drops_every_10th_packet",

        "fail_one_cable_after_10us_for_100us",
        "fail_one_cable_after_10us_for_100us",
        "fail_one_cable_after_10us_for_100us",

        "fail_one_cable",
        "fail_one_cable",
        "fail_one_cable",

        "one_cable_drops_every_10th_packet",
        "one_cable_drops_every_10th_packet",
        "one_cable_drops_every_10th_packet",

        "degrade_one_switch",
        "degrade_one_switch",
        "degrade_one_switch",

        "degrade_one_cable",
        "degrade_one_cable",
        "degrade_one_cable",

        "ber_switch_one_percent",
        "ber_switch_one_percent",
        "ber_switch_one_percent",

        "ber_cable_one_percent",
        "ber_cable_one_percent",
        "ber_cable_one_percent",

        "10_percent_failed_switches",
        "10_percent_failed_switches",
        "10_percent_failed_switches",

        "10_percent_failed_cables",
        "10_percent_failed_cables",
        "10_percent_failed_cables",

        "10_percent_us-cs-cables-per-switch",
        "10_percent_us-cs-cables-per-switch",
        "10_percent_us-cs-cables-per-switch",

        "10_percent_degraded_switches",
        "10_percent_degraded_switches",
        "10_percent_degraded_switches",

        "10_percent_degraded_cables",
        "10_percent_degraded_cables",
        "10_percent_degraded_cables",

        "fail_new_switch_every_100us_for_50us",
        "fail_new_switch_every_100us_for_50us",
        "fail_new_switch_every_100us_for_50us",

        "fail_new_cable_every_100us_for_50us",
        "fail_new_cable_every_100us_for_50us",
        "fail_new_cable_every_100us_for_50us",

        "fail_one_switch_and_cable_after_10us_for_100us",
        "fail_one_switch_and_cable_after_10us_for_100us",
        "fail_one_switch_and_cable_after_10us_for_100us",

        "fail_one_switch_one_cable",
        "fail_one_switch_one_cable",
        "fail_one_switch_one_cable",

        "fail_one_switch_and_cable_degrade_one_switch_and_cable",
        "fail_one_switch_and_cable_degrade_one_switch_and_cable",
        "fail_one_switch_and_cable_degrade_one_switch_and_cable",

        "10_percent_failed_switches_and_cables",
        "10_percent_failed_switches_and_cables",
        "10_percent_failed_switches_and_cables",
    ]

    connection_matrix = [
        "elias_incast_32_1_2MB",
        "elias_incast_32_1_2MB",
        "elias_perm_128_2MB",
    ]

    short_title = ["_1os_perm", "_8os_perm", "_1os_incast"]
    topo =        [ "fat_tree_128_1os_800.topo", "fat_tree_128_8os_800.topo", "fat_tree_128_1os_800.topo",]

    data = []
    df = pd.DataFrame(data)

    for i, title in enumerate(titles):
        list_fct =run(
            failures_input[i]+short_title[i%3],
            title,
            failures_input[i],
            2**22,
            128,
            topo[i%3],
            connection_matrix[i%3],
        )
        data.append({
        'ExperimentName': title,
        'REPS': list_fct[0],
        'REPS Circular': list_fct[1],
        'REPS Circular\n without freezing': list_fct[2],
        'Spraying': list_fct[3],
        "Mode": failures_input[i]    })
        df = pd.DataFrame(data)
        df.to_csv('experiment_data2MB.csv', index=False)


if __name__ == "__main__":
    main()