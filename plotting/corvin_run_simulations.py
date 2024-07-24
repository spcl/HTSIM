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


def run_experiment(experiment_name, experiment_cm, list_algorithm, topology, routing_name, parameters):
    
    os.system("mkdir -p experiments")
    os.system("rm -rf experiments/{}".format(experiment_cm))
    os.system("mkdir experiments/{}".format(experiment_cm))

    all_data = []
    numbers = []
    numbers_max = []

    list_smartt = []
    num_nack_smartt = 0
    list_ndp = []
    num_nack_ndp = 0
    list_bbr = []
    num_nack_bbr = 0

    if(is_element("SMaRTT", list_algorithm)):
        # SMaRTT
        out_name = "experiments/{}/outSMaRTT.txt".format(experiment_cm)
        string_to_run = ""
        if (topology == "Dragonfly"):
            p = parameters[0]
            a = parameters[1]
            h = parameters[2]
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology dragonfly -df_strategy {} -p {} -a {} -h {} > {}".format(experiment_cm, routing_name, p, a, h, out_name)
        
        elif (topology == "Slimfly"):
            p = parameters[0]
            q_base = parameters[1]
            q_exp = parameters[2]
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology slimfly -sf_strategy {} -p {} -q_base {} -q_exp {} > {}".format(experiment_cm, routing_name, p, q_base, q_exp, out_name) 
        
        elif (topology == "Hammingmesh"):
            height = parameters[0]
            width = parameters[1]
            height_board = parameters[2]
            width_board = parameters[3]
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology hammingmesh -hm_strategy {} -height {} -width {} -height_board {} -width_board {} > {}".format(experiment_cm, routing_name, height, width, height_board, width_board, out_name) 
        
        elif (topology == "BCube"):
            # topology == "BCube"
            n = parameters[0]
            k = parameters[1]
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology bcube -bc_strategy {} -n_bcube {} -k_bcube {} > {}".format(experiment_cm, routing_name, n, k, out_name) 

        else:
            # topology == "Fat_Tree"
            topo_file = parameters[0]
            string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -x_gain 0.25 -y_gain 2 -w_gain 1 -z_gain 0.8 -bonus_drop 1.5 -collect_data 1 -decrease_on_nack 1 -algorithm smartt -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology fat_tree -nodes {} -topo topo/fat_tree_{}N.topo > {}".format(experiment_cm, topo_file, topo_file, out_name)

        
        print(string_to_run)
        # .format(link_speed, experiment_cm, queue_size, initial_cwnd, experiment_topo, ecn_min, ecn_max, paths, mi_gain, fi_gain, md_gain, fd_gain, out_name)
        os.system(string_to_run)
        list_smartt = getListFCT(out_name)
        all_data.append(list_smartt)
        numbers_max.append(max(list_smartt))
        num_nack_smartt = getNumTrimmed(out_name)
        numbers.append(num_nack_smartt)
        print("SMARTT: Flow Diff {} - Total {}".format(max(list_smartt) - min(list_smartt), max(list_smartt)))

    if(is_element("NDP", list_algorithm)):
        # NDP
        out_name = "experiments/{}/outNDP.txt".format(experiment_cm)
        string_to_run = ""

        if (topology == "Dragonfly"):
            p = parameters[0]
            a = parameters[1]
            h = parameters[2]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology dragonfly -df_strategy {} -p {} -a {} -h {} > {}".format(experiment_cm, routing_name, p, a, h, out_name)
        
        elif (topology == "Slimfly"):
            p = parameters[0]
            q_base = parameters[1]
            q_exp = parameters[2]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology slimfly -sf_strategy {} -p {} -q_base {} -q_exp {} > {}".format(experiment_cm, routing_name, p, q_base, q_exp, out_name) 
        
        elif (topology == "Hammingmesh"):
            height = parameters[0]
            width = parameters[1]
            height_board = parameters[2]
            width_board = parameters[3]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology hammingmesh -hm_strategy {} -height {} -width {} -height_board {} -width_board {} > {}".format(experiment_cm, routing_name, height, width, height_board, width_board, out_name) 
        
        elif(topology == "BCube"):
            # topology == "BCube"
            n = parameters[0]
            k = parameters[1]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o ndp_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology bcube -bc_strategy {} -n_bcube {} -k_bcube {} > {}".format(experiment_cm, routing_name, n, k, out_name) 
        else:
            # topology == "Fat_Tree"
            topo_file = parameters[0]
            string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology fat_tree -nodes {} -topo topo/fat_tree_{}N.topo > {}".format(experiment_cm, topo_file, topo_file, out_name)

        print(string_to_run)
        os.system(string_to_run)
        list_ndp = getListFCT(out_name)
        all_data.append(list_ndp)
        numbers_max.append(max(list_ndp))
        num_nack_ndp = getNumTrimmed(out_name)
        numbers.append(num_nack_ndp)
        print("NDP: Flow Diff {} - Total {}".format(max(list_ndp) - min(list_ndp), max(list_ndp)))

    if(is_element("BBR", list_algorithm)):
        # BBR
        out_name = "experiments/{}/outBBR.txt".format(experiment_cm)
        string_to_run = ""
        
        if (topology == "Dragonfly"):
            p = parameters[0]
            a = parameters[1]
            h = parameters[2]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology dragonfly -df_strategy {} -p {} -a {} -h {} > {}".format(experiment_cm, routing_name, p, a, h, out_name)
        
        elif (topology == "Slimfly"):
            p = parameters[0]
            q_base = parameters[1]
            q_exp = parameters[2]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology slimfly -sf_strategy {} -p {} -q_base {} -q_exp {} > {}".format(experiment_cm, routing_name, p, q_base, q_exp, out_name) 
        
        elif (topology == "Hammingmesh"):
            height = parameters[0]
            width = parameters[1]
            height_board = parameters[2]
            width_board = parameters[3]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -short_hop_latency 100 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology hammingmesh -hm_strategy {} -height {} -width {} -height_board {} -width_board {} > {}".format(experiment_cm, routing_name, height, width, height_board, width_board, out_name) 
        
        elif(topology == "BCube"):
            # topology == "BCube"
            n = parameters[0]
            k = parameters[1]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology bcube -bc_strategy {} -n_bcube {} -k_bcube {} > {}".format(experiment_cm, routing_name, n, k, out_name) 
        
        else:
            # topology == "Fat_Tree"
            topo_file = parameters[0]
            string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -q 50 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed 100000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -collect_data 1 -ecn 10 40 -tm ../sim/datacenter/connection_matrices/{} -topology fat_tree -nodes {} -topo topo/fat_tree_{}N.topo > {}".format(experiment_cm, topo_file, topo_file, out_name)
        
        print(string_to_run)
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

    """ # Add a vertical dashed line at x=1000 with lower opacity
    best_time = (msg_size * 8 / 100 / 1000) + 12
    print("Best Time {}".format(best_time))
    plt.axvline(x=best_time, linestyle='--', color='#3b3b3b', alpha=0.55, linewidth = 3) """

    # Set labels and title
    plt.xlabel('Flow Completion Time ({})'.format(unit),fontsize=19.5)
    plt.ylabel('CDF (%)',fontsize=19.5)
    plt.title('{}\n100Gbps - 4KiB MTU'.format(experiment_name), fontsize=20)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: (int(x) / 1000)))
    plt.grid()  #just add this
    plt.legend(frameon=True)

    # Show the plot
    plt.tight_layout()
    plt.savefig("experiments/{}/cdf.svg".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.png".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.pdf".format(experiment_cm), bbox_inches='tight')

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
    
    plt.savefig("experiments/{}/nack.svg".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.png".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.pdf".format(experiment_cm), bbox_inches='tight')

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
    # Format y-axis tick labels without scientific notation
    ax2.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))# Show the plot
    plt.title('{}\n100Gbps - 4KiB MTU'.format(experiment_name), fontsize=17)
    plt.grid()  #just add this

    plt.savefig("experiments/{}/completion.svg".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/completion.png".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/completion.pdf".format(experiment_cm), bbox_inches='tight')

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

    plt.title('{}\nLink Speed 100Gbps - 4KiB MTU'.format(experiment_name), fontsize=17)
    plt.grid()  #just add this

    plt.savefig("experiments/{}/violin_fct.svg".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.png".format(experiment_cm), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.pdf".format(experiment_cm), bbox_inches='tight')

    plt.savefig("../../Plots/Violin_FCT/violin_fct_{}.png".format(experiment_cm), bbox_inches='tight')
    plt.close()
    

def main():
    # General Exp Settings

    list_algorithm = ["SMaRTT", "NDP", "BBR"]
    # "SMaRTT", "NDP", "BBR"
    list_topology_routing_size = [
        {"topology": "Dragonfly", "list_routing": ["Minimal", "Valiant's"], "list_parameters_set": [[[1, 3, 2]], [[3, 3, 2]]]},
        # , [2, 3, 2], [6, 6, 4]
        {"topology": "Slimfly", "list_routing": ["Minimal", "Valiant's"], "list_parameters_set": [[[1, 3, 1]], [[2, 5, 1]]]},
        # , [2, 3, 1], [4, 11, 1]
        {"topology": "Hammingmesh", "list_routing": ["Minimal"], "list_parameters_set": [[[2, 2, 2, 2]], [[3, 3, 3, 3]]]},
        # , [2, 2, 3, 3], [4, 5, 7, 7]
        {"topology": "BCube", "list_routing": ["Minimal"], "list_parameters_set": [[[4, 2]], [[3, 4]]]},
        # , [6, 2], [4, 5]
        {"topology": "Fat_Tree", "list_routing": ["Minimal"], "list_parameters_set": [[[16]], [[128]]]}
        # , [6, 2], [4, 5]
    ]
    # maybe also "Fat_Tree"
    list_traffic_pattern = ["Incast", "All-to-all", "Permutation"]
    # later on also "All-reduce"
    list_message_size = [1]
    # 8

    # list_topologies = ["fat_tree_1024_8os_800.topo", "fat_tree_1024_8os_800.topo"]   
    # msg_sizes = [2**21]   msg_sizes[idx]

    cnt = 0

    for item in list_topology_routing_size:
        topology = item["topology"]
        list_routing = item["list_routing"]
        list_parameters_set = item["list_parameters_set"]
        list_parameters_ata, list_parameters = list_parameters_set
        for routing in list_routing:
            for traffic_pattern in list_traffic_pattern:
                list_parameters_tmp = []
                if (traffic_pattern == "All-to-all"):
                    list_parameters_tmp = list_parameters_ata
                else:
                    list_parameters_tmp = list_parameters
                for parameters in list_parameters_tmp:
                    topo_size = getTopoSize(topology, parameters)
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

                        name = "{}, {}, {}, {} Nodes, {} MiB".format(topology, routing, traffic_pattern, topo_size, message_size)
                        tm_name = "corvin{}{}{}{}N{}MiB".format(topology, routing_name_cap, traffic_pattern, topo_size, message_size)
                        print("Running Experiment Named {}".format(name))
                        print("-tm {}".format(tm_name))

                        if (traffic_pattern == "Incast"):
                            os.system("python3 ../sim/datacenter/connection_matrices/corvinGenIncast.py {} {} {} {} 0 15 && mv {} ../sim/datacenter/connection_matrices/".format(tm_name, topo_size, topo_size - 1, 1048576 * message_size, tm_name))
                        elif (traffic_pattern == "All-to-all"):
                            os.system("python3 ../sim/datacenter/connection_matrices/gen_serialn_alltoall.py {} {} {} {} 8 {} 0 15 && mv {} ../sim/datacenter/connection_matrices/".format(tm_name, topo_size, topo_size, topo_size, 1048576 * message_size, tm_name))
                        elif (traffic_pattern == "Permutation"):
                            os.system("python3 ../sim/datacenter/connection_matrices/gen_permutation.py {} {} {} {} 0 15 && mv {} ../sim/datacenter/connection_matrices/".format(tm_name, topo_size, topo_size, 1048576 * message_size, tm_name))
                        # else:
                            # traffic_pattern == "All-reduce"

                        run_experiment(name, tm_name, list_algorithm, topology, routing_name, parameters)
                        os.system("rm ../sim/datacenter/connection_matrices/{}".format(tm_name))
                        cnt += 1
    print("\nDone.\nSimulations: {}".format(cnt))


if __name__ == "__main__":
    main()