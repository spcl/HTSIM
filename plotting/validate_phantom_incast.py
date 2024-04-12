import os
import re
import numpy as np
import matplotlib.pyplot as plt
import statsmodels.api as sm
import seaborn as sns
from pathlib import Path
import matplotlib.pyplot as plt
import argparse
import pandas as pd
from  matplotlib.ticker import FuncFormatter

bdp = 6450000
goal_file = ["paper_tuning_1", "paper_tuning_2"]
incast_size = [12000000, 36000000]
phantom_type = ["-phantom_in_series", ""]
phantom_slowdown = [2, 5, 10, 20]
incast_amount = [8]

id_name = []
percentage_trimmed = []
runtime_to_ideal = []
completion_times_raw = []
number_trimmed_after = []

def getProNameFromGoal(goal_name):
    if ("1" in goal_name):
        return "2BDP"
    elif ("2" in goal_name):
        return "6BDP"

def getProNameFromPQtype(pq_parameter):
    if (pq_parameter == ""):
        return "Par."
    else:
        return "Ser."

def getMaxCompletionTime(name_file_to_use):
    temp_list = []
    with open(name_file_to_use) as file:
        for line in file:
            # FCT
            result = re.search(r"Flow Completion time is (\d+.\d+) - Flow Finishing Time (\d+) - Flow Start Time (\d+) - Size Finished Flow (\d+)", line)
            if result:
                actual = float(result.group(1))
                temp_list.append(actual)
    return max(temp_list)

def getNumTrimmed(name_file_to_use):
    num_nack = 0
    with open(name_file_to_use) as file:
        for line in file:
            # FCT
            result = re.search(r"NACK", line)
            if result:
                num_nack += 1
    return 100

def updateGoalList(incast_am):
    global goal_file
    global id_name
    global percentage_trimmed
    global runtime_to_ideal
    global completion_times_raw
    global number_trimmed_after
    goal_file = []
    goal_file.append("paper_tuning_1")
    goal_file.append("paper_tuning_2")
    id_name = []
    percentage_trimmed = []
    runtime_to_ideal = []
    completion_times_raw = []
    number_trimmed_after = []

def getIdealJump(incast_am):
    if (incast_am == 2):
        return 2820000
    elif (incast_am == 8):
        return 708000
    if (incast_am == 16):
        return 340000

def getNumTrimmedAfter(name_file_to_use, after):
    num_nack = 0
    with open(name_file_to_use) as file:
        for line in file:
            # FCT
            result = re.search(r"NACK at (\d+)", line)
            if result:
                time = int(int(result.group(1))/1000)
                if (time > after):
                    num_nack += 1
    return num_nack

def create_plot(x_labels, y_values, type_plot, min_plot, max_plot, incast_am):
    plt.figure(figsize=(8, 4.6))

    # Set a single color palette for all bars
    custom_colors = ["#2F4858"] * int(len(x_labels) / 2) + ["#8FAADC"] * int(len(x_labels) / 2)  # Customize the colors as needed

    # Create a bar plot with customizations
    ax = sns.barplot(x=x_labels, y=y_values, palette=custom_colors)

    # Adjust the starting point on the y-axis
    plt.ylim(min_plot,max_plot)
    plt.xticks(fontsize=8.8)


    #ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = 15)
    ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 15)
    ax.set_axisbelow(True)

    # Add labels and a title
    plt.xlabel('Experiment',fontsize=15)
    if (type_plot == "completion"):
        plt.ylabel('Percentage of Ideal Completion Time',fontsize=15)
    elif (type_plot == "trimming"):
        plt.ylabel('Percentage of Trimming to Total',fontsize=15)
    else:
        plt.ylabel('Number of Trims after initial state',fontsize=15)
    plt.title('Comparing Different Phantom Queue Configurations',fontsize=17)
    plt.grid()  #just add this
    plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x)))

    # Show the plot
    plt.tight_layout()
    if (type_plot == "completion"):
        plt.savefig("out{}_1completion.png".format(incast_am), bbox_inches='tight')
        plt.savefig("out{}_1completion.pdf".format(incast_am), bbox_inches='tight')
    elif (type_plot == "trimming"):
        plt.savefig("out{}_1trimming.png".format(incast_am), bbox_inches='tight')
        plt.savefig("out{}_1trimming.pdf".format(incast_am), bbox_inches='tight')
    else:
        plt.savefig("out{}_1trimming_after.png".format(incast_am), bbox_inches='tight')
        plt.savefig("out{}_1trimming_after.pdf".format(incast_am), bbox_inches='tight')

for idx_incast, incast_flow in enumerate(incast_amount):
    updateGoalList(incast_flow)
    for idx_goal, goal_name in enumerate(goal_file):
        for idx_slowdown, slowdown_percentage in enumerate(phantom_slowdown):
            for idx_pq_type, pq_type in enumerate(phantom_type):
                unique_id = "{}\nS:{}%\nQ:{}".format(getProNameFromGoal(goal_name), slowdown_percentage, getProNameFromPQtype(pq_type))
                print("Considering {}".format(unique_id))
                out_file = "tmp.out"

                run_string = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -topo ../sim/datacenter/topologies/fat_tree_128_4os.topo -algorithm intersmartt_test -nodes 128 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 50000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border 1 -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data 1 -use_pacing 1 -use_phantom 1 -phantom_slowdown {} -phantom_size 6470912 -decrease_on_nack 1 {} -topology interdc -max_queue_size 300000 -interdc_delay 500000 -phantom_both_queues > tmp.out".format(goal_name, slowdown_percentage, pq_type, out_file)
                #run_string = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm intersmartt_composed -nodes 8192 -q 4452000 -strat ecmp_host_random_ecn -number_entropies 256 -kmin 2 -kmax 80 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 80000 -mtu 4096 -seed 2819 -queue_type composite -hop_latency 70000 -switch_latency 0 -reuse_entropy 1 -goal {} -x_gain 6 -y_gain 0 -w_gain 0 -z_gain 2.5 -bonus_drop 1 -collect_data 0 -explicit_starting_cwnd 5600000 -explicit_starting_buffer 560000 -explicit_base_rtt 561400000 -explicit_target_rtt 610000000 -use_pacing 1 -use_phantom 1 -phantom_slowdown {} -phantom_size 5600000 -decrease_on_nack 0 -jump_to {} {} > {}".format(goal_name, slowdown_percentage, getIdealJump(incast_flow), pq_type, out_file)
                os.system(run_string)
                
                actual_completion_time = getMaxCompletionTime(out_file)
                trimmed_amount = getNumTrimmed(out_file)
                tot_mb_sent = 8 * incast_size[idx_goal]
                ideal_completion_time = ((tot_mb_sent + tot_mb_sent*0.035) * incast_flow / 50/ 1000) + 1400
                id_name.append(unique_id)
                runtime_to_ideal.append(ideal_completion_time/actual_completion_time*100)
                completion_times_raw.append(actual_completion_time)
                percentage_trimmed.append(trimmed_amount / (incast_size[idx_goal]/4096*incast_flow) * 100)
                number_trimmed_after.append(getNumTrimmedAfter(out_file, 5000))

                print(run_string)
                print(actual_completion_time)
                print(ideal_completion_time)
                print(ideal_completion_time/actual_completion_time*100)
                print(trimmed_amount)
                print(trimmed_amount / (tot_mb_sent/4096/8*incast_flow) *100)
                print(getNumTrimmedAfter(out_file, 500))
                print()

    create_plot(id_name, runtime_to_ideal, "completion", 60, 100, incast_flow)
    create_plot(id_name, percentage_trimmed, "trimming", 0, 80, incast_flow)
    create_plot(id_name, number_trimmed_after, "trimming_after", 0, max(number_trimmed_after) + 10, incast_flow)