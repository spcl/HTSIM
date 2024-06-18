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
import time

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


def run_experiment(experiment_name, experiment_cm, experiment_topo, name_title, msg_size):
    os.system("mkdir -p experiments")
    os.system("rm -rf experiments/{}".format(experiment_name))
    os.system("mkdir experiments/{}".format(experiment_name))


    out_name = "experiments/{}/out.txt".format(experiment_name)
    string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm smartt -nodes 128 -q {} -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 0 -use_super_fast_increase 1 -fast_drop 1 -linkspeed {} -mtu 4096 -seed 2 -queue_type composite -hop_latency 700 -switch_latency 0 -reuse_entropy 1 -tm ../sim/datacenter/connection_matrices/{} -x_gain 0.25 -y_gain 2 -w_gain 2 -z_gain 0.8 -bonus_drop 1 -collect_data 1 -use_pacing 0 -decrease_on_nack 1 -topology normal -max_queue_size 1000000 > {}".format(queue_size,link_speed, experiment_cm, out_name)
    os.system(string_to_run)
    list_smartt = getListFCT(out_name)
    num_nack_smartt = getNumTrimmed(out_name)
    print("SMARTT: Flow Diff {} - Total {}".format(max(list_smartt) - min(list_smartt), max(list_smartt)))

    
# Combine all data into a list of lists
    
    # all_data = [list_eqds_vanilla, list_dctcp_vanilla, list_swift_reps, list_bbr, list_smartt, list_smartt_eqds]
    # labels = ['EQDS', 'MPRDMA', 'Swift', 'BBR', 'SMaRTT', 'SMaRTT-EQDS']
    all_data = [list_smartt]
    labels = ['SMaRTT']
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
    # best_time = (msg_size * 8 / 100 / 1000) + 12
    # print("Best Time {}".format(best_time))
    # plt.axvline(x=best_time, linestyle='--', color='#3b3b3b', alpha=0.55, linewidth = 3)

    # Set labels and title
    plt.xlabel('Flow Completion Time ({})'.format(unit),fontsize=19.5)
    plt.ylabel('CDF (%)',fontsize=19.5)
    #plt.title('{}\n1024 Nodes - 800Gbps - 4KiB MTU'.format(name_title), fontsize=20)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: (int(x) / 1000)))
    plt.grid()  #just add this
    plt.legend([],[], frameon=False)

    # Show the plot
    plt.tight_layout()
    plt.savefig("experiments/{}/cdf.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()


    # PLOT 2 (NACK)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 5))
    # numbers = [num_nack_eqds_vanilla, num_nack_dctcp_vanilla, num_nack_swift_reps, num_nack_bbr, num_nack_smartt,  num_nack_smartt_eqds]
    # labels = ['EQDS', 'MPRDMA', 'Swift', 'BBR', 'SMaRTT', 'SMaRTT-EQDS']
    numbers = [num_nack_smartt]
    labels = ['SMaRTT']
    

    # Create a DataFrame from the lists
    data = pd.DataFrame({'Packets Trimmed': numbers, 'Algorithm': labels})

    # Create a bar chart using Seaborn
    ax3 = sns.barplot(x='Algorithm', y='Packets Trimmed', data=data)
    ax3.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation
    
    ax3.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))# Show the plot
    plt.title('{}\n1024 Nodes - Link Speed 800Gbps - 4KiB MTU'.format(experiment_name), fontsize=16.5)
    plt.grid()  #just add this
    
    plt.savefig("experiments/{}/nack.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()

    # PLOT 3 (COMPLETION TIME)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 5))

    # numbers = [max(list_eqds_vanilla), max(list_dctcp_vanilla), max(list_swift_reps), max(list_bbr), max(list_smartt), max(list_smartt_eqds)]
    # labels = ['EQDS', 'MPRDMA', 'Swift', 'BBR', 'SMaRTT', 'SMaRTT-EQDS']
    numbers = [max(list_smartt)]
    labels = ['SMaRTT']
    # Create a DataFrame from the lists
    data2 = pd.DataFrame({'Completion Time': numbers, 'Algorithm': labels})

    # Create a bar chart using Seaborn
    ax2 = sns.barplot(x='Algorithm', y='Completion Time', data=data2)
    ax2.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation
    ax2.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))# Show the plot
    plt.title('{}\n800Gbps - 4KiB MTU'.format(experiment_name), fontsize=17)
    plt.grid()  #just add this

    plt.savefig("experiments/{}/completion.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/completion.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/completion.pdf".format(experiment_name), bbox_inches='tight')
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

    plt.title('{}\nLink Speed 800Gbps - 4KiB MTU'.format(experiment_name), fontsize=17)
    plt.grid()  #just add this

    plt.savefig("experiments/{}/violin_fct.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()
    

def main():

    
 
    list_title_names = ["Permutation 8:1 OS - 1024 Nodes - 2MiB Flows", "Permutation 8:1 OS - 1024 Nodes - 32MiB Flows", "Permutation 8:1 OS - 1024 Nodes - 8MiB Flows but ont 16MiB"]
    list_custom_names = ["Permutation8OS_128_Nodes_2MB_800G", "Permutation8OS_128_Nodes_32MB_800G", "Permutation8OS_128_Nodes_8MB16MB_800G"]
    list_exp = ["mult_perm_2", "mult_perm_32","mult_perm_diff"]
    list_topologies = ["fat_tree_128_8os_800.topo", "fat_tree_128_8os_800.topo", "fat_tree_128_8os_800.topo"] 

    msg_sizes = [2**21, 2**25, 2**23]    
    for idx, item in enumerate(list_exp):
        print("Running Experiment Named {}".format(list_custom_names[idx]))
        run_experiment(list_custom_names[idx], list_exp[idx], list_topologies[idx], list_title_names[idx], msg_sizes[idx])


if __name__ == "__main__":
    main()