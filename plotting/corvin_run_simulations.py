import os
import re 
import matplotlib.pyplot as plt
import statsmodels.api as sm
import numpy as np
import matplotlib.pyplot as plt
from  matplotlib.ticker import FuncFormatter
import seaborn as sns
import pandas as pd
from matplotlib.ticker import ScalarFormatter
import math

# Constants
link_speed = 100000 * 8
hops = 12
size_1 = 4160
tot_capacity = (((12 * 1000) + (6*4160*8/(link_speed/1000))) * (link_speed/1000)) / 8

paths = 128
fi_gain = 0.2 * 8
fd_gain = 0.8
md_gain = 2
mi_gain = 1 * 8
bonus_drop = 0.85
ecn_min = int(tot_capacity/size_1/5)
ecn_max = int(tot_capacity/size_1/5*4)
queue_size = int(tot_capacity/size_1)
initial_cwnd = int(tot_capacity/size_1*1.2)

print("Queue in PKT {} - Initial CWND {} - ECN {}".format(int(tot_capacity/size_1), initial_cwnd, ecn_min, ecn_max))

def getListFCT(name_file_to_use):
    temp_list = []
    with open(name_file_to_use) as file:
        for line in file:
            # FCT
            result = re.search(r"Flow Completion time is (\d+.\d+)", line)
            if result:
                actual = float(result.group(1))
                temp_list.append(actual)
    return temp_list

def getNumTrimmed(name_file_to_use):
    num_nack = 0
    with open(name_file_to_use) as file:
        for line in file:
            # FCT
            result = re.search(r"Total NACKs: (\d+)", line)
            if result:
                num_nack += int(result.group(1))
    return num_nack

def getTopoSize(topology, parameters):
    topo_size = 0
    if (topology == "Dragonfly"):
        p = parameters[0]
        a = parameters[1]
        h = parameters[2]
        topo_size = (1 + h * a) * a * p
    elif (topology == "Slimfly"):
        p = parameters[0]
        q_base = parameters[1]
        q_exp = parameters[2]
        topo_size = 2 * p * pow(q_base, 2 * q_exp)
    elif (topology == "Hammingmesh"):
        height = parameters[0]
        width = parameters[1]
        height_board = parameters[2]
        width_board = parameters[3]
        topo_size = height * width * height_board * width_board
    elif(topology == "BCube"):
        # topology == "BCube"
        n = parameters[0]
        k = parameters[1]
        topo_size = pow(n, k)
    else:
        # topology == "Fat_Tree"
        topo_size = parameters[0]
    return topo_size

def is_element(element, list):
    for e in list:
        if(e == element):
            return True
    return False


def run_experiment(experiment_name, experiment_cm, list_algorithm, topology, routing_name, parameters, traffic_pattern, message_size, folder_name):
    
    os.system("mkdir -p experiments")
    os.system("rm -rf experiments/{}".format(folder_name))
    os.system("mkdir experiments/{}".format(folder_name))

    all_data = []
    numbers = []
    numbers_max = []

    list_smartt = []
    num_nack_smartt = 0
    list_ndp = []
    num_nack_ndp = 0
    list_bbr = []
    num_nack_bbr = 0

    incast_ratio = 0
    number_of_nodes = 0

    height_board = 0
    width_board = 0
    p = 0
    a = 0
    h = 0

    if(is_element("SMaRTT", list_algorithm)):
        # SMaRTT
        out_name = "experiments/{}/outSMaRTT.txt".format(folder_name)
        string_to_run = ""
        if (topology == "Dragonfly"):
            p = parameters[0]
            a = parameters[1]
            h = parameters[2]
            number_of_nodes = (1 + a * h) * a * p
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology dragonfly -df_strategy {} -p {} -a {} -h {} > {}".format(folder_name, experiment_cm, routing_name, p, a, h, out_name)
        
        elif (topology == "Slimfly"):
            p = parameters[0]
            q_base = parameters[1]
            q_exp = parameters[2]
            number_of_nodes = 2 * pow(q_base, q_exp) * p
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology slimfly -sf_strategy {} -p {} -q_base {} -q_exp {} > {}".format(folder_name, experiment_cm, routing_name, p, q_base, q_exp, out_name) 
        
        elif (topology == "Hammingmesh"):
            height = parameters[0]
            width = parameters[1]
            height_board = parameters[2]
            width_board = parameters[3]
            number_of_nodes = height * height_board * width * width_board
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology hammingmesh -hm_strategy {} -height {} -width {} -height_board {} -width_board {} > {}".format(folder_name, experiment_cm, routing_name, height, width, height_board, width_board, out_name) 
        
        elif (topology == "BCube"):
            # topology == "BCube"
            n = parameters[0]
            k = parameters[1]
            number_of_nodes = pow(n, k)
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology bcube -bc_strategy {} -n_bcube {} -k_bcube {} > {}".format(folder_name, experiment_cm, routing_name, n, k, out_name) 

        else:
            # topology == "Fat_Tree"
            topo_file = parameters[0]
            number_of_nodes = topo_file
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology fat_tree -nodes {} -topo topo/fat_tree_{}N.topo > {}".format(folder_name, experiment_cm, topo_file, topo_file, out_name)

        
        # print(string_to_run)
        # .format(link_speed, experiment_cm, queue_size, initial_cwnd, experiment_topo, ecn_min, ecn_max, paths, mi_gain, fi_gain, md_gain, fd_gain, out_name)
        os.system(string_to_run)
        list_smartt = getListFCT(out_name)
        all_data.append(list_smartt)
        numbers_max.append(max(list_smartt))
        num_nack_smartt = getNumTrimmed(out_name)
        numbers.append(num_nack_smartt)
        print("SMARTT: Flow Diff {} - Total {}".format(max(list_smartt) - min(list_smartt), max(list_smartt)))
        print("Mean: {}".format(sum(list_smartt)/len(list_smartt)))

    if(is_element("NDP", list_algorithm)):
        # NDP
        out_name = "experiments/{}/outNDP.txt".format(folder_name)
        string_to_run = ""

        if (topology == "Dragonfly"):
            p = parameters[0]
            a = parameters[1]
            h = parameters[2]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology dragonfly -df_strategy {} -p {} -a {} -h {} > {}".format(folder_name, experiment_cm, routing_name, p, a, h, out_name)
        
        elif (topology == "Slimfly"):
            p = parameters[0]
            q_base = parameters[1]
            q_exp = parameters[2]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology slimfly -sf_strategy {} -p {} -q_base {} -q_exp {} > {}".format(folder_name, experiment_cm, routing_name, p, q_base, q_exp, out_name) 
        
        elif (topology == "Hammingmesh"):
            height = parameters[0]
            width = parameters[1]
            height_board = parameters[2]
            width_board = parameters[3]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology hammingmesh -hm_strategy {} -height {} -width {} -height_board {} -width_board {} > {}".format(folder_name, experiment_cm, routing_name, height, width, height_board, width_board, out_name) 
        
        elif(topology == "BCube"):
            # topology == "BCube"
            n = parameters[0]
            k = parameters[1]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology bcube -bc_strategy {} -n_bcube {} -k_bcube {} > {}".format(folder_name, experiment_cm, routing_name, n, k, out_name) 
        else:
            # topology == "Fat_Tree"
            topo_file = parameters[0]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology fat_tree -nodes {} -topo topo/fat_tree_{}N.topo > {}".format(folder_name, experiment_cm, topo_file, topo_file, out_name)

        # print(string_to_run)
        os.system(string_to_run)
        list_ndp = getListFCT(out_name)
        all_data.append(list_ndp)
        numbers_max.append(max(list_ndp))
        num_nack_ndp = getNumTrimmed(out_name)
        numbers.append(num_nack_ndp)
        print("NDP: Flow Diff {} - Total {}".format(max(list_ndp) - min(list_ndp), max(list_ndp)))

    if(is_element("BBR", list_algorithm)):
        # BBR
        out_name = "experiments/{}/outBBR.txt".format(folder_name)
        string_to_run = ""
        
        if (topology == "Dragonfly"):
            p = parameters[0]
            a = parameters[1]
            h = parameters[2]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology dragonfly -df_strategy {} -p {} -a {} -h {} > {}".format(folder_name, experiment_cm, routing_name, p, a, h, out_name)
        
        elif (topology == "Slimfly"):
            p = parameters[0]
            q_base = parameters[1]
            q_exp = parameters[2]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology slimfly -sf_strategy {} -p {} -q_base {} -q_exp {} > {}".format(folder_name, experiment_cm, routing_name, p, q_base, q_exp, out_name) 
        
        elif (topology == "Hammingmesh"):
            height = parameters[0]
            width = parameters[1]
            height_board = parameters[2]
            width_board = parameters[3]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology hammingmesh -hm_strategy {} -height {} -width {} -height_board {} -width_board {} > {}".format(folder_name, experiment_cm, routing_name, height, width, height_board, width_board, out_name) 
        
        elif(topology == "BCube"):
            # topology == "BCube"
            n = parameters[0]
            k = parameters[1]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology bcube -bc_strategy {} -n_bcube {} -k_bcube {} > {}".format(folder_name, experiment_cm, routing_name, n, k, out_name) 
        
        else:
            # topology == "Fat_Tree"
            topo_file = parameters[0]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{}/{} -topology fat_tree -nodes {} -topo topo/fat_tree_{}N.topo > {}".format(folder_name, experiment_cm, topo_file, topo_file, out_name)
        
        # print(string_to_run)
        os.system(string_to_run)
        list_bbr = getListFCT(out_name)
        all_data.append(list_bbr)
        numbers_max.append(max(list_bbr))
        num_nack_bbr = getNumTrimmed(out_name)
        numbers.append(num_nack_bbr)
        print("BBR: Flow Diff {} - Total {}".format(max(list_bbr) - min(list_bbr), max(list_bbr)))
   
    # Create a list of labels for each dataset
    labels = list_algorithm
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
    plt.figure(figsize=(6, 4.2))

    for i, data in enumerate(all_data):
        ax = sns.lineplot(x=data, y=cumulative_probabilities[i], label=labels[i], linewidth = 3.3)

    ax.set_xticks(ax.get_xticks())
    ax.set_yticks(ax.get_yticks())
    ax.set(ylim=(0, 1))
    ax.tick_params(labelsize=10)

    ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = smaller_font)
    ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 16)

    # Add a vertical dashed line at x=1000 with lower opacity
    best_time = 0

    # 1MiB / 100Gb/s ~= 84us
    if(traffic_pattern == "Incast"):
        incast_ratio = parameters[-1]
        best_time = incast_ratio * message_size * 84
    elif(traffic_pattern == "All-to-all"):
        best_time = 8 * message_size * 84
    elif(traffic_pattern == "Permutation"):
        best_time = message_size * 84
        if(topology == "Dragonfly"):
            if(routing_name == "minimal"):
                best_time += 0.4
            else:
                # routing_name == "valiants"
                best_time += 0.4
        elif(topology == "Slimfly"):
            if(routing_name == "minimal"):
                best_time += 0.4
            else:
                # routing_name == "valiants"
                best_time += 0.4
        elif(topology == "Hammingmesh"):
            best_time += 0.6
        elif(topology == "BCube"):
            best_time += 8
        else:
            # topology == "Fat_Tree"
            best_time += 4
    # (traffic_patter == "All-Reduce"):
    else:
        best_time = message_size * 84 / number_of_nodes
        if(topology == "Dragonfly"):
            if(routing_name == "minimal"):
                best_time += 2.8
            else:
                # routing_name == "valiants"
                best_time += 5
        elif(topology == "Slimfly"):
            if(routing_name == "minimal"):
                best_time += 4.4
            else:
                # routing_name == "valiants"
                best_time += 8.4
        elif(topology == "Hammingmesh"):
            best_time += (height_board + width_board) * 0.2 + 8.4
        elif(topology == "BCube"):
            best_time += 4 * k + 4
        else:
            # topology == "Fat_Tree"
            best_time += 12
 
        

    if(best_time != 0):
        print("Best Time {}".format(best_time))
        plt.axvline(x=best_time, linestyle='--', color='#3b3b3b', alpha=0.55, linewidth = 3)

    # Set labels and title
    plt.xlabel('Flow Completion Time ({})'.format(unit),fontsize=19.5)
    plt.ylabel('CDF (%)',fontsize=19.5)
    plt.title('{}\n100Gbps - 4KiB MTU'.format(experiment_name), fontsize=20)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: (int(x) / 1000)))
    plt.grid()  #just add this
    plt.legend(frameon=True)

    # Show the plot
    plt.tight_layout()
    plt.savefig("experiments/{}/cdf.svg".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.png".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.pdf".format(folder_name), bbox_inches='tight')

    plt.savefig("../../Plots/CDF/cdf_{}.png".format(experiment_cm), bbox_inches='tight')
    plt.close()


    # PLOT 2 (NACK)
    
    plt.figure(figsize=(7, 5))
    # num_nack_smartt, num_nack_ndp, 
    labels = list_algorithm

    # Create a DataFrame from the lists
    data = pd.DataFrame({'Packets Trimmed': numbers, 'Algorithm': labels})

    # Create a bar chart using Seaborn
    ax3 = sns.barplot(x='Algorithm', y='Packets Trimmed', hue="Algorithm", data=data)
    ax3.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation
    
    ax3.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))# Show the plot
    plt.title('{}\nLink Speed 100Gbps - 4KiB MTU'.format(experiment_name), fontsize=16.5)
    plt.grid()  #just add this
    
    plt.savefig("experiments/{}/nack.svg".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.png".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.pdf".format(folder_name), bbox_inches='tight')

    plt.savefig("../../Plots/NACK/nack_{}.png".format(experiment_cm), bbox_inches='tight')
    plt.close()

    # PLOT 3 (COMPLETION TIME)
    
    plt.figure(figsize=(7, 5))

    labels = list_algorithm
    # Create a DataFrame from the lists
    data2 = pd.DataFrame({'Completion Time': numbers_max, 'Algorithm': labels})

    # Create a bar chart using Seaborn
    ax2 = sns.barplot(x='Algorithm', y='Completion Time', hue="Algorithm", data=data2)
    ax2.tick_params(labelsize=9.5)

    if(best_time != 0):
        plt.axhline(y=best_time, linestyle='--', color='#3b3b3b', alpha=0.55, linewidth = 3)

    # Format y-axis tick labels without scientific notation
    ax2.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))# Show the plot
    plt.title('{}\n100Gbps - 4KiB MTU'.format(experiment_name), fontsize=17)
    plt.grid()  #just add this

    plt.savefig("experiments/{}/completion.svg".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/completion.png".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/completion.pdf".format(folder_name), bbox_inches='tight')

    plt.savefig("../../Plots/Completion/completion_{}.png".format(experiment_cm), bbox_inches='tight')
    plt.close()

    # PLOT 4 (FLOW DISTR)
    
    plt.figure(figsize=(7, 5))

    combined_data = []
    hue_list = []
    for idx, names in enumerate(labels):
        combined_data += all_data[idx]
        hue_list += [labels[idx]] * len(all_data[idx])

    # Create the violin plot
    my = sns.violinplot(x=hue_list, y=combined_data, hue=hue_list)
    # cut=0 ?
    my.set_axisbelow(True)
    my.tick_params(labelsize=9.5)

    if(best_time != 0):
        plt.axhline(y=best_time, linestyle='--', color='#3b3b3b', alpha=0.55, linewidth = 3)

    plt.title('{}\nLink Speed 100Gbps - 4KiB MTU'.format(experiment_name), fontsize=17)
    plt.grid()  #just add this

    plt.savefig("experiments/{}/violin_fct.svg".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.png".format(folder_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.pdf".format(folder_name), bbox_inches='tight')

    plt.savefig("../../Plots/Violin_FCT/violin_fct_{}.png".format(experiment_cm), bbox_inches='tight')
    plt.close()
    

def main():
    # General Exp Settings

    list_algorithm = ["SMaRTT", "NDP", "BBR"]
    # "SMaRTT", "NDP", "BBR"
    list_topology_routing_size = [
        {"topology": "Dragonfly", "list_routing": ["Minimal", "Valiant's"], "list_parameters_set": [[[1, 3, 2], [2, 3, 2]], [[3, 3, 3, 64], [6, 6, 4, 128]]]},
        # 
        {"topology": "Slimfly", "list_routing": ["Minimal", "Valiant's"], "list_parameters_set": [[[1, 3, 1], [2, 3, 1]], [[2, 5, 1, 64], [4, 11, 1, 128]]]},
        # 
        # {"topology": "Hammingmesh", "list_routing": ["Minimal"], "list_parameters_set": [[[2, 2, 2, 2], [2, 2, 3, 3]], [[3, 3, 3, 3, 64], [4, 5, 7, 7, 128]]]},
        # 
        # {"topology": "BCube", "list_routing": ["Minimal"], "list_parameters_set": [[[4, 2], [6, 2]], [[3, 4, 64], [4, 5, 128]]]},
        # 
        # {"topology": "Fat_Tree", "list_routing": ["Minimal"], "list_parameters_set": [[[16], [32]], [[128, 64], [1024, 128]]]}
    ]
    # maybe also "Fat_Tree"
    list_traffic_pattern = ["All-reduce"]
    # "Incast", "All-to-all", "Permutation", "All-reduce"
    # later on also "All-reduce"
    list_message_size = [1, 8]
    # 1, 8

    # list_topologies = ["fat_tree_1024_8os_800.topo", "fat_tree_1024_8os_800.topo"]   
    # msg_sizes = [2**21]   msg_sizes[idx]

    cnt = 0
    ratio = 0

    for item in list_topology_routing_size:
        topology = item["topology"]
        list_routing = item["list_routing"]
        list_parameters_set = item["list_parameters_set"]
        list_parameters_a, list_parameters = list_parameters_set
        for routing in list_routing:
            for traffic_pattern in list_traffic_pattern:
                list_parameters_tmp = []
                if (traffic_pattern == "All-to-all" or traffic_pattern == "All-reduce"):
                    list_parameters_tmp = list_parameters_a
                else:
                    list_parameters_tmp = list_parameters
                for parameters in list_parameters_tmp:
                    topo_size = getTopoSize(topology, parameters)
                    ratio = parameters[-1]
                    for message_size in list_message_size:
                        print("--------------------------------------------------------------------")
                        routing_name = ""
                        routing_name_cap = ""
                        if (routing == "Minimal"):
                            routing_name = "minimal"
                            routing_name_cap = "Minimal"
                        else:
                            # routing == "Valiant's"
                            routing_name = "valiants"
                            routing_name_cap = "Valiants"
                        
                        folder_name = "corvin/{}/{}/{}/{}N/{}MiB".format(topology, routing_name_cap, traffic_pattern, topo_size, message_size)

                        if (traffic_pattern == "Incast"):
                            name = "{}, {}, {} {}:1, {} Nodes, {} MiB".format(topology, routing, traffic_pattern, ratio, topo_size, message_size)
                            tm_name = "corvin{}{}{}{}_{}N{}MiB".format(topology, routing_name_cap, traffic_pattern, ratio, topo_size, message_size)
                            print("Running Experiment Named {}".format(name))
                            print("-tm {}".format(tm_name))
                        else:
                            name = "{}, {}, {}, {} Nodes, {} MiB".format(topology, routing, traffic_pattern, topo_size, message_size)
                            tm_name = "corvin{}{}{}{}N{}MiB".format(topology, routing_name_cap, traffic_pattern, topo_size, message_size)
                            print("Running Experiment Named {}".format(name))
                            print("-tm {}".format(tm_name))

                        """ if (traffic_pattern == "Incast"):
                            os.system("python3 ../sim/datacenter/connection_matrices/corvinGenIncast.py {} {} {} {} 0 15 && mv {} ../sim/datacenter/connection_matrices/{}/".format(tm_name, topo_size, ratio, 1048576 * message_size, tm_name, folder_name))
                        elif (traffic_pattern == "All-to-all"):
                            os.system("python3 ../sim/datacenter/connection_matrices/gen_serialn_alltoall.py {} {} {} {} 8 {} 0 15 && mv {} ../sim/datacenter/connection_matrices/{}/".format(tm_name, topo_size, topo_size, topo_size, 1048576 * message_size, tm_name, folder_name))
                        elif (traffic_pattern == "Permutation"):
                            os.system("python3 ../sim/datacenter/connection_matrices/gen_permutation.py {} {} {} {} 0 15 && mv {} ../sim/datacenter/connection_matrices/{}/".format(tm_name, topo_size, topo_size, 1048576 * message_size, tm_name, folder_name))
                        # else:
                            #  """
                        if (traffic_pattern == "All-reduce"):
                            os.system("python3 ../sim/datacenter/connection_matrices/gen_allreduce.py {} {} {} {} {} 1 15 && mv {} ../sim/datacenter/connection_matrices/{}/".format(tm_name, topo_size, topo_size, topo_size, 1048576 * message_size // topo_size, tm_name, folder_name))
                            
                        run_experiment(name, tm_name, list_algorithm, topology, routing_name, parameters, traffic_pattern, message_size, folder_name)
                        # os.system("rm ../sim/datacenter/connection_matrices/{}".format(tm_name))
                        cnt += 1
    print("\nDone.\nSimulations: {}".format(cnt))


if __name__ == "__main__":
    main()