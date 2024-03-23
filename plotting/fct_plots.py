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
from matplotlib.ticker import FormatStrFormatter

# Constants
rateo = 8
link_speed = 100000 * rateo
hops = 12
size_1 = 4160
tot_capacity = (((12 * 1000) + (6*4160*8/(link_speed/1000))) * (link_speed/1000)) / 8

paths = 128
fi_gain = 0.25 * rateo
fd_gain = 0.8
md_gain = 1 * 2
mi_gain = 2 * (rateo / 2)
bonus_drop = 0.85
ecn_min = int(tot_capacity/size_1/5)
ecn_max = int(tot_capacity/size_1/5*4)
queue_size = int(tot_capacity/size_1)
initial_cwnd = int(tot_capacity/size_1*1.25)

os_ratio = 1
eqds_cwnd = 50000
inter_dc_delay = 500000

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


def run_experiment(experiment_name, experiment_cm, name_title, msg_size):
    
    os.system("rm -rf experiments/{}".format(experiment_name))
    os.system("mkdir experiments/{}".format(experiment_name))

    # PhantomCC
    out_name = "experiments/{}/out.txt".format(experiment_name)
    string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm intersmartt -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 50000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data 1 -use_pacing 1 -use_phantom 1 -phantom_slowdown 10 -phantom_size 2600000 -decrease_on_nack 0 -topology interdc -max_queue_size 175000 -interdc_delay {} -phantom_both_queues -stop_after_quick > {}".format(os_ratio, experiment_cm, inter_dc_delay, out_name)
    os.system(string_to_run)
    print(string_to_run)
    list_smartt1 = getListFCT(out_name)
    num_nack_smartt = getNumTrimmed(out_name)
    print("PhantomCC: Flow Diff {} - Total {}".format(max(list_smartt1) - min(list_smartt1), max(list_smartt1)))
    #os.system("python3 performance_complete.py")

    # MPRDMA
    out_name = "experiments/{}/out.txt".format(experiment_name)
    string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm mprdma -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 20 -kmax 80 -use_fast_increase 0 -use_super_fast_increase 1 -fast_drop 0 -linkspeed 50000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data 1 -use_pacing 0 -use_phantom 0 -phantom_slowdown 3 -phantom_size 2600000 -decrease_on_nack 1 -topology interdc -max_queue_size 175000 -interdc_delay {} > {}".format(os_ratio, experiment_cm, inter_dc_delay, out_name) 
    os.system(string_to_run)
    print(string_to_run)
    list_smartt2 = getListFCT(out_name)
    num_nack_smartt = getNumTrimmed(out_name)
    print("MPRDMA: Flow Diff {} - Total {}".format(max(list_smartt2) - min(list_smartt2), max(list_smartt2)))
    #os.system("python3 performance_complete.py")

    # EQDS
    out_name = "experiments/{}/out.txt".format(experiment_name)
    string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -k 1 -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -linkspeed 50000 -mtu 4096 -seed 15 -hop_latency 1000 -switch_latency 0 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -collect_data 1 -topology interdc  -max_queue_size 175000 -interdc_delay {} -cwnd {} > {}".format(os_ratio, experiment_cm, eqds_cwnd, inter_dc_delay, out_name)
    os.system(string_to_run)
    print(string_to_run)
    list_smartt3 = getListFCT(out_name)
    num_nack_smartt = getNumTrimmed(out_name)
    print("EQDS: Flow Diff {} - Total {}".format(max(list_smartt3) - min(list_smartt3), max(list_smartt3)))
    #os.system("python3 performance_complete.py")

    # Combine all data into a list of lists
    list_smartt1 = [x / 1000 for x in list_smartt1]
    list_smartt2 = [x / 1000 for x in list_smartt2]
    list_smartt3 = [x / 1000 for x in list_smartt3]

    all_data = [list_smartt1, list_smartt2, list_smartt3]
    # Create a list of labels for each dataset
    labels = ['PhantomCC', 'MPRDMA', 'EQDS']
    # Initialize an empty list to store cumulative probabilities

    unit = "μs"
    smaller_font = 15

    cumulative_probabilities = []

    # Step 2: Sort and compute cumulative probabilities for each dataset
    for data in all_data:
        data.sort()
        n = len(data)
        cumulative_probabilities.append(np.arange(1, n + 1) / n)

    # Step 3: Plot the CDFs using Seaborn
    plt.figure(figsize=(7, 5))

    for i, data in enumerate(all_data):
        ax = sns.lineplot(x=data, y=cumulative_probabilities[i], label=labels[i], linewidth = 3.3)

    ax.set_xticks(ax.get_xticks())
    ax.set_yticks(ax.get_yticks())
    ax.set(ylim=(0, 1))
    ax.tick_params(labelsize=10)

    ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = smaller_font)
    ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 16)

    # Set labels and title
    plt.xlabel('Flow Completion Time ({})'.format(unit),fontsize=18.5)
    plt.ylabel('CDF (%)',fontsize=18.5)
    plt.title('{}\n1024 Nodes - 100Gbps - 4KiB MTU'.format(name_title), fontsize=18.5)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x)))
    plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x) / 1000))
    plt.grid()  #just add this
    plt.legend([],[], frameon=False)

    # Show the plot
    plt.tight_layout()
    plt.savefig("experiments/{}/cdf.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()


    # PLOT 3 BARPLOT
    # Calculate the average value and 95% confidence interval for each list
    averages = [sum(lst) / len(lst) for lst in all_data]

    # Create a bar plot using Seaborn
    sns.barplot(x=['List 1', 'List 2', 'List 3'], y=averages, ci=None)  # Setting ci=None disables the default error bars
    
    plt.grid()  #just add this
    plt.legend([],[], frameon=False)

    # Show the plot
    plt.tight_layout()
    plt.savefig("experiments/{}/avg_fct.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/avg_fct.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/avg_fct.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()

    # Step 3: Plot the CDFs using Seaborn
    plt.figure(figsize=(7, 5))

    for i, data in enumerate(all_data):
        ax = sns.lineplot(x=data, y=cumulative_probabilities[i], label=labels[i], linewidth = 3.3)

    ax.set_xticks(ax.get_xticks())
    ax.set_yticks(ax.get_yticks())
    ax.set(ylim=(0, 1))
    ax.tick_params(labelsize=10)

    ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = smaller_font)
    ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 16)

    # Set labels and title
    plt.xlabel('Flow Completion Time ({})'.format(unit),fontsize=18.5)
    plt.ylabel('CDF (%)',fontsize=18.5)
    plt.title('{}\n1024 Nodes - 100Gbps - 4KiB MTU'.format(name_title), fontsize=18.5)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x)))
    plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x) / 1000))
    plt.grid()  #just add this
    plt.legend([],[], frameon=False)

    # Show the plot
    plt.tight_layout()
    plt.savefig("experiments/{}/cdf.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/cdf.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()


    # PLOT 4 (FLOW DISTR)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 4.5))

    combined_data = []
    hue_list = []
    for idx, names in enumerate(labels):
        combined_data += all_data[idx]
        hue_list += [labels[idx]] * len(all_data[idx])

    # Create the violin plot
    my = sns.violinplot(x=hue_list, y=combined_data, cut=0)
    my.tick_params(labelsize=9.5)

    my.set_yticks(my.get_yticks())
    my.tick_params(labelsize=15)
    my.set_yticklabels([str(round(i,1)) for i in my.get_yticks()], fontsize = 15)
    my.set_axisbelow(True)
    sns.set_context("paper", rc={"axes.labelsize":15})

    plt.xlabel('SMaRTT Version'.format(unit),fontsize=16)
    plt.ylabel('Flow Completion Time (ms)',fontsize=16)
    plt.title('{}\n1024 Nodes - 800Gbps - 4KiB MTU'.format(name_title), fontsize=17)
    
    my.yaxis.set_major_formatter(FormatStrFormatter('%.2f'))
    plt.grid()  #just add this

    plt.savefig("experiments/{}/violin_fct.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()
    

def main():
    # General Exp Settings
    list_title_names = ["Permutation Test InterDC"]
    list_custom_names = ["Permutation_InterDC"]
    list_exp = ["perm32_both"]
    msg_sizes = [2**25]
    for idx, item in enumerate(list_exp):
        print("Running Experiment Named {}".format(list_custom_names[idx]))
        run_experiment(list_custom_names[idx], list_exp[idx],  list_title_names[idx], msg_sizes[idx])


if __name__ == "__main__":
    main()