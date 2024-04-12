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

link_speed = 50000

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


    # DCTCP
    out_name = "experiments/{}/out.txt".format(experiment_name)
    #string_to_run = "../sim/datacenter/htsim_uec -linkspeed {} -bonus_drop 1 -fast_drop 1 -strat ecmp_host_random2_ecn -tm ../sim/datacenter/connection_matrices/{} -q {} -cwnd {} -topo ../sim/datacenter/topologies/{} -end 4400 -seed 2 -use_exp_avg_ecn 1 -ecn {} {} -paths {} -mi_gain {} -fi_gain {} -md_gain {} -fd_gain {} -algorithm mprdma > {}".format(link_speed, experiment_cm, queue_size, initial_cwnd, experiment_topo, ecn_min, ecn_max, paths, mi_gain, fi_gain, md_gain, fd_gain, out_name)
    string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm mprdma -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 20 -kmax 80 -use_fast_increase 0 -use_super_fast_increase 1 -fast_drop 0 -linkspeed {} -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -tm ../sim/datacenter/connection_matrices/{} -x_gain 1 -y_gain 0 -w_gain 0 -z_gain 2 -bonus_drop 1.0 -collect_data 0 -use_pacing 0 -use_phantom 0 -phantom_slowdown 2 -phantom_size 5300000 -decrease_on_nack 0 -topology interdc -max_queue_size 175000 -os_border 1 -interdc_delay 1000000 > {}".format(link_speed, experiment_cm, out_name)

    os.system(string_to_run)
    print(string_to_run)
    list_dctcp_vanilla = getListFCT(out_name)
    num_nack_dctcp_vanilla = getNumTrimmed(out_name)
    print("MPRDMA: Flow Diff {} - Total {}".format(max(list_dctcp_vanilla) - min(list_dctcp_vanilla), max(list_dctcp_vanilla)))
    
    # SMaRTT
    out_name = "experiments/{}/out.txt".format(experiment_name)
    string_to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm intersmartt -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 0 -use_super_fast_increase 1 -fast_drop 1 -linkspeed {} -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -tm ../sim/datacenter/connection_matrices/{} -x_gain 1 -y_gain 0 -w_gain 0 -z_gain 2 -bonus_drop 1.0 -collect_data 0 -use_pacing 1 -use_phantom 1 -phantom_slowdown 2 -phantom_size 12000000 -decrease_on_nack 0 -topology interdc -max_queue_size 175000 -os_border 1 -interdc_delay 1000000 -phantom_both_queues -stop_after_quick > {}".format(link_speed, experiment_cm, out_name)
    os.system(string_to_run)
    print(string_to_run)
    list_smartt = getListFCT(out_name)
    num_nack_smartt = getNumTrimmed(out_name)
    print("SMARTT: Flow Diff {} - Total {}".format(max(list_smartt) - min(list_smartt), max(list_smartt)))

    # BBR - REPS
    out_name = "experiments/{}/out.txt".format(experiment_name)
    string_to_run = "../sim/datacenter/htsim_bbr -o uec_entry -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed {} -mtu 4096 -seed 125 -hop_latency 1000 -switch_latency 0 -tm ../sim/datacenter/connection_matrices/{} -collect_data 0 -topology interdc -max_queue_size 175000 -interdc_delay 1000000 -cwnd 12000000 > {}".format(link_speed, experiment_cm, out_name)

    print(string_to_run)
    os.system(string_to_run)
    list_bbr = getListFCT(out_name)
    num_nack_bbr = getNumTrimmed(out_name)
    print("BBR: Flow Diff {} - Total {}".format(max(list_bbr) - min(list_bbr), max(list_bbr)))

    # NDP - REPS
    out_name = "experiments/{}/out.txt".format(experiment_name)
    string_to_run = "../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -linkspeed {} -mtu 4096 -seed 125 -hop_latency 1000 -switch_latency 0 -tm ../sim/datacenter/connection_matrices/{} -collect_data 0 -topology interdc -max_queue_size 175000 -interdc_delay 1000000 -cwnd 12000000 > {}".format(link_speed, experiment_cm, out_name)

    print(string_to_run)
    os.system(string_to_run)
    list_ndp = getListFCT(out_name)
    num_nack_ndp = getNumTrimmed(out_name)
    print("NDP: Flow Diff {} - Total {}".format(max(list_ndp) - min(list_ndp), max(list_ndp)))

    # PLOT 2 (NACK)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 5))
    numbers = [num_nack_dctcp_vanilla, num_nack_ndp, num_nack_bbr, num_nack_smartt]
    all_data = [ list_dctcp_vanilla, list_ndp, list_bbr, list_smartt]
    # Create a list of labels for each dataset
    labels = ['MPRDMA', 'EQDS', 'BBR', 'PhantomCC']

    # Create a DataFrame from the lists
    data = pd.DataFrame({'Packets Trimmed': numbers, 'Algorithm': labels})

    # Create a bar chart using Seaborn
    ax3 = sns.barplot(x='Algorithm', y='Packets Trimmed', data=data)
    ax3.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation
    
    ax3.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))# Show the plot
    plt.title('{}\n100Gbps - Delay 1us DC and 1ms InterDC - 4KiB MTU'.format(name_title), fontsize=16)
    plt.grid()  #just add this
    
    plt.savefig("experiments/{}/nack.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/nack.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()

    # PLOT 3 (COMPLETION TIME)
    # Your list of 5 numbers and corresponding labels
    plt.figure(figsize=(7, 5))

    numbers = [ max(list_dctcp_vanilla), max(list_ndp),  max(list_bbr), max(list_smartt)]
    labels = ['MPRDMA', 'EQDS', 'BBR', 'PhantomCC']
    # Create a DataFrame from the lists
    data2 = pd.DataFrame({'Completion Time': numbers, 'Algorithm': labels})

    # Create a bar chart using Seaborn
    ax2 = sns.barplot(x='Algorithm', y='Completion Time', data=data2)
    ax2.tick_params(labelsize=9.5)
    # Format y-axis tick labels without scientific notation
    ax2.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))# Show the plot
    plt.title('{}\n100Gbps - Delay 1us DC and 1ms InterDC - 4KiB MTU'.format(name_title), fontsize=16)
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

    plt.title('{}\nLink Speed 100Gbps - 4KiB MTU'.format(experiment_name), fontsize=17)
    plt.grid()  #just add this

    plt.savefig("experiments/{}/violin_fct.svg".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.png".format(experiment_name), bbox_inches='tight')
    plt.savefig("experiments/{}/violin_fct.pdf".format(experiment_name), bbox_inches='tight')
    plt.close()
    

def main():
    # General Exp Settings
    list_title_names = ["Incast Intra-DC Only", "Incast Inter-DC Only", "Incast Mixed Inter-DC and Intra-DC"]
    list_custom_names = ["Incast_Tests_Small_1", "Incast_Tests_Small_2", "Incast_Tests_Small_3"]
    list_exp = ["incast_intra_dc", "incast_inter_dc", "incast_mixed_dc"]
    msg_sizes = [2**21, 2**25, 2**25]   
    for idx, item in enumerate(list_exp):
        print("Running Experiment Named {}".format(list_custom_names[idx]))
        run_experiment(list_custom_names[idx], list_exp[idx], list_title_names[idx], msg_sizes[idx])


if __name__ == "__main__":
    main()