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

    os.system("cp -a ../../sim/output/cwd/. experiments/{}/cwd/".format(experiment))
    os.system("cp -a ../../sim/output/rtt/. experiments/{}/rtt/".format(experiment))
    os.system("cp -a ../../sim/output/queue/. experiments/{}/queue_size_normalized/".format(experiment))
    os.system("cp -a ../../sim/output/switch_drops/. experiments/{}/switch_drops/".format(experiment))
    os.system("cp -a ../../sim/output/cable_drops/. experiments/{}/cable_drops/".format(experiment))
    os.system("cp -a ../../sim/output/switch_failures/. experiments/{}/switch_failures/".format(experiment))
    os.system("cp -a ../../sim/output/cable_failures/. experiments/{}/cable_failures/".format(experiment))
    os.system("cp -a ../../sim/output/routing_failed_switch/. experiments/{}/routing_failed_switch/".format(experiment))
    os.system("cp -a ../../sim/output/routing_failed_cable/. experiments/{}/routing_failed_cable/".format(experiment))
    os.system("cp -a ../../sim/output/switch_degradations/. experiments/{}/switch_degradations/".format(experiment))
    os.system("cp -a ../../sim/output/cable_degradations/. experiments/{}/cable_degradations/".format(experiment))
    os.system("cp -a ../../sim/output/random_packet_drops/. experiments/{}/random_packet_drops/".format(experiment))
    os.system("cp -a ../../sim/output/packet_seq/. experiments/{}/packet_seq/".format(experiment))
    os.system("cp -a ../../sim/datacenter/{}.txt experiments/{}/{}.txt".format(out_name, experiment,out_name))


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
        raise Exception(
            "Number of flows does not match, one flow probably did not finish"
        )
    else:
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


def getNrDroppedPackets(name_file_to_use):
    res = 0
    with open(name_file_to_use) as file:
        for line in file:
            result = re.search(r"Total number of dropped packets: (\d+)", line)
            if result:
                return int(result.group(1))
    raise Exception("Number of dropped Packets not found")


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
    os.chdir("experiments/" + experiment+"/")
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
    os.chdir("experiments/" + experiment+"/")
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
    os.chdir("experiments/" + experiment+"/")
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

    print("Running {}".format(title))

    os.system("mkdir experiments/{}".format(short_title))

    balancer = "reps"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps -end_time 10 -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    print(string_to_run)
    execute_string(string_to_run, short_title, balancer)
    filename = "experiments/{}/{}.txt".format(short_title,balancer)
    list_reps = getListFCT(filename)
    num_nack_reps = getNumTrimmed(filename)
    num_lost_packets_reps = getNrDroppedPackets(filename)
    list_total_out_of_order_reps = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_reps = getOutofOrderRatio(short_title)
    list_out_of_order_distance_reps = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title)
    complete_plot.write_image("experiments/{}/complete_plot_{}.svg".format(short_title,balancer))
    print(
        "REPS: Flow Diff {} - Total {}".format(
            max(list_reps) - min(list_reps), max(list_reps)
        )
    )

    balancer = "reps_circular"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps -end_time 10 -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    print(string_to_run)
    execute_string(string_to_run, short_title, balancer)
    filename = "experiments/{}/{}.txt".format(short_title,balancer)
    list_repsC = getListFCT(filename)
    num_nack_repsC = getNumTrimmed(filename)
    num_lost_packets_repsC = getNrDroppedPackets(filename)
    list_total_out_of_order_repsC = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_repsC = getOutofOrderRatio(short_title)
    list_out_of_order_distance_repsC = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title)
    complete_plot.write_image("experiments/{}/complete_plot_{}.svg".format(short_title,balancer))
    print(
        "REPS circular: Flow Diff {} - Total {}".format(
            max(list_repsC) - min(list_repsC), max(list_repsC)
        )
    )

    balancer = "reps_circular"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -end_time 10 -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}Freezing.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    print(string_to_run)
    execute_string(string_to_run, short_title, balancer+"Freezing")
    filename = "experiments/{}/{}Freezing.txt".format(short_title,balancer)
    list_repsCF = getListFCT(filename)
    num_nack_repsCF = getNumTrimmed(filename)
    num_lost_packets_repsCF = getNrDroppedPackets(filename)
    list_total_out_of_order_repsCF = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_repsCF = getOutofOrderRatio(short_title)
    list_out_of_order_distance_repsCF = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title)
    complete_plot.write_image("experiments/{}/complete_plot_{}.svg".format(short_title,balancer))
    print(
        "repsCF: Flow Diff {} - Total {}".format(
            max(list_repsCF) - min(list_repsCF), max(list_repsCF)
        )
    )

    balancer = "spraying"
    string_to_run = "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -use_timeouts -strat {} -use_freezing_reps -end_time 10 -bonus_drop 1.5 -nodes {} -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 1000 -reuse_entropy 1 -topo topologies/{} -tm connection_matrices/{} -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/{}.txt > {}.txt".format(
            balancer, nodes, topology, connection_matrix, failures_input, balancer
        )
    print(string_to_run)
    execute_string(string_to_run, short_title, balancer)
    filename = "experiments/{}/{}.txt".format(short_title,balancer)
    list_spraying = getListFCT(filename)
    num_nack_spraying = getNumTrimmed(filename)
    num_lost_packets_spraying = getNrDroppedPackets(filename)
    list_total_out_of_order_spraying = getMaxTotalOutOfOrderPackets(short_title)
    list_out_of_order_ratio_spraying = getOutofOrderRatio(short_title)
    list_out_of_order_distance_spraying = getOutOfOrderDistance(short_title)
    complete_plot = run_complete(title,short_title)
    complete_plot.write_image("experiments/{}/complete_plot_{}.svg".format(short_title,balancer))
    print(
        "Spraying: Flow Diff {} - Total {}".format(
            max(list_spraying) - min(list_spraying), max(list_spraying)
        )
    )

    list_lost_packets = [
        num_lost_packets_reps,
        num_lost_packets_repsC,
        num_lost_packets_repsCF,
        num_lost_packets_spraying,
    ]
    list_fct = [list_reps, list_repsC, list_repsCF, list_spraying]
    list_max = [max(list_reps), max(list_repsC), max(list_repsCF), max(list_spraying)]
    list_nacks = [num_nack_reps, num_nack_repsC, num_nack_repsCF, num_nack_spraying]
    list_mean_total_out_of_order = [np.mean(list_total_out_of_order_reps), np.mean(list_total_out_of_order_repsC),np.mean(list_total_out_of_order_repsCF), np.mean(list_total_out_of_order_spraying)]
    list_std_total_out_of_order = [np.std(list_total_out_of_order_reps), np.std(list_total_out_of_order_repsC), np.std(list_total_out_of_order_repsCF), np.std(list_total_out_of_order_spraying)]
    list_max_total_out_of_order = [max(list_total_out_of_order_reps), max(list_total_out_of_order_repsC),max(list_total_out_of_order_repsCF), max(list_total_out_of_order_spraying)]
    list_mean_total_out_of_order_ratio = [np.mean(list_out_of_order_ratio_reps), np.mean(list_out_of_order_ratio_repsC),np.mean(list_out_of_order_ratio_repsCF), np.mean(list_out_of_order_ratio_spraying)]
    list_std_total_out_of_order_ratio = [np.std(list_out_of_order_ratio_reps), np.std(list_out_of_order_ratio_repsC),np.std(list_out_of_order_ratio_repsCF), np.std(list_out_of_order_ratio_spraying)]
    list_max_total_out_of_order_ratio = [max(list_out_of_order_ratio_reps), max(list_out_of_order_ratio_repsC), max(list_out_of_order_ratio_repsCF), max(list_out_of_order_ratio_spraying)]
    list_mean_total_out_of_order_distance = [np.mean(list_out_of_order_distance_reps), np.mean(list_out_of_order_distance_repsC),np.mean(list_out_of_order_distance_repsCF), np.mean(list_out_of_order_distance_spraying)]
    list_std_total_out_of_order_distance = [np.std(list_out_of_order_distance_reps), np.std(list_out_of_order_distance_repsC),np.std(list_out_of_order_distance_repsCF), np.std(list_out_of_order_distance_spraying)]
    list_max_total_out_of_order_distance = [max(list_out_of_order_distance_reps), max(list_out_of_order_distance_repsC),max(list_out_of_order_distance_repsCF), max(list_out_of_order_distance_spraying)]
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
    best_time = (msg_size * 32 / 100 / 1000) + 12
    print("Best Time {}".format(best_time))
    plt.axvline(x=best_time, linestyle="--", color="#3b3b3b", alpha=0.55, linewidth=3)

    # Set labels and title
    plt.xlabel("Flow Completion Time ({})".format(unit), fontsize=19.5)
    plt.ylabel("CDF (%)", fontsize=19.5)
    plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: (int(x) / 1000)))
    plt.grid()  # just add this
    plt.legend(frameon=False)

    # Show the plot
    plt.tight_layout()
    plt.savefig("experiments/{}/cdf.svg".format(short_title), bbox_inches="tight")
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

    ax3.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))  # Show the plot
    plt.title(title, fontsize=16.5)
    plt.grid()  # just add this

    plt.savefig("experiments/{}/nack.svg".format(short_title), bbox_inches="tight")
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
    ax2.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))  # Show the plot

    for p in ax2.patches:
        ax2.annotate(
            format(int(p.get_height()), ""),
            (p.get_x() + p.get_width() / 2.0, p.get_height()),
            ha="center",
            va="center",
            xytext=(0, 7),
            textcoords="offset points",
        )

    plt.title(title, fontsize=17)
    plt.grid()

    plt.savefig(
        "experiments/{}/completion.svg".format(short_title), bbox_inches="tight"
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

    plt.title(title, fontsize=17)
    plt.grid()  # just add this
    plt.savefig(
        "experiments/{}/violin_fct.svg".format(short_title), bbox_inches="tight"
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

    plt.title(title, fontsize=17)
    plt.grid()
    plt.savefig(
        "experiments/{}/lost_packets.svg".format(short_title), bbox_inches="tight"
    )
    plt.close()


    # PLOT 6 (Max total out of order Packets)
    plt.figure(figsize=(7, 5))
    means = list_mean_total_out_of_order
    maxs = list_max_total_out_of_order
    errors = list_std_total_out_of_order
    labels = list_labels

    # Make bar chart
    data4 = pd.DataFrame({"Total out of order packets": means, "Algorithm": labels, "Errors": errors})
    ax5 = sns.barplot(x="Algorithm", y="Total out of order packets", data=data4)
    ax5.tick_params(labelsize=9.5)

    for index, row in data4.iterrows():
        ax5.errorbar(x=index, y=row["Total out of order packets"], yerr=errors[index], fmt='none', capsize=5, color='black')
    # Format y-axis tick labels without scientific notation
    ax5.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))

    for index, p in enumerate(ax5.patches):
        mean_value = round(means[index],2)
        max_value = round(maxs[index],2)
        error = round(errors[index],2)
        ax5.annotate(f'Mean: {mean_value}\nMax: {max_value}\n Std Dev: {error}',
                    (p.get_x() + p.get_width() / 2.0, 0),
                    ha="center", va="bottom", fontsize=8, color='black',
                    xytext=(0, 7), textcoords='offset points')

    plt.title(title, fontsize=17)
    plt.grid()
    plt.savefig("experiments/{}/total_out_of_order.svg".format(short_title), bbox_inches="tight")
    plt.close()


    # PLOT 7 (Out of order ratio)
    plt.figure(figsize=(7, 5))
    means = list_mean_total_out_of_order_ratio
    maxs = list_max_total_out_of_order_ratio
    errors = list_std_total_out_of_order_ratio
    labels = list_labels

    # Make bar chart
    data4 = pd.DataFrame({"Out of order ratio": means, "Algorithm": labels,"Errors": errors})
    ax5 = sns.barplot(x="Algorithm", y="Out of order ratio", data=data4)
    ax5.tick_params(labelsize=9.5)

    for index, row in data4.iterrows():
        ax5.errorbar(x=index, y=row["Out of order ratio"], yerr=errors[index], fmt='none', capsize=5, color='black')
    # Format y-axis tick labels without scientific notation
    ax5.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))

    for index, p in enumerate(ax5.patches):
        mean_value = round(means[index],2)
        max_value = round(maxs[index],2)
        error = round(errors[index],2)
        ax5.annotate(f'Mean: {mean_value}\nMax: {max_value}\n Std Dev: {error}',
                    (p.get_x() + p.get_width() / 2.0, 0),
                    ha="center", va="bottom", fontsize=8, color='black',
                    xytext=(0, 7), textcoords='offset points')

    plt.title(title, fontsize=17)
    plt.grid()
    plt.savefig("experiments/{}/out_of_order_ratio.svg".format(short_title), bbox_inches="tight")
    plt.close()

    # PLOT 7 (Out of order Distance)
    plt.figure(figsize=(7, 5))
    means = list_mean_total_out_of_order_distance
    maxs = list_max_total_out_of_order_distance
    errors = list_std_total_out_of_order_distance
    labels = list_labels

    # Make bar chart
    data4 = pd.DataFrame({"Out of order Distance": means, "Algorithm": labels, "Errors": errors})
    ax5 = sns.barplot(x="Algorithm", y="Out of order Distance", data=data4)
    ax5.tick_params(labelsize=9.5)

    for index, row in data4.iterrows():
        ax5.errorbar(x=index, y=row["Out of order Distance"], yerr=errors[index], fmt='none', capsize=5, color='black')
    
    # Format y-axis tick labels without scientific notation
    ax5.yaxis.set_major_formatter(ScalarFormatter(useMathText=False))

    for index, p in enumerate(ax5.patches):
        mean_value = round(means[index],2)
        max_value = round(maxs[index],2)
        error = round(errors[index],2)
        ax5.annotate(f'Mean: {mean_value}\nMax: {max_value}\n Std Dev: {error}',
                    (p.get_x() + p.get_width() / 2.0, 0),
                    ha="center", va="bottom", fontsize=8, color='black',
                    xytext=(0, 7), textcoords='offset points')

    plt.title(title, fontsize=17)
    plt.grid()
    plt.savefig("experiments/{}/out_of_order_distance.svg".format(short_title), bbox_inches="tight")
    plt.close()


def main():

    os.system("rm -rf experiments/")
    os.system("mkdir -p experiments")

    titles = [
        "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MiB Flows \n Failure mode: 10% failed US-CS cables per switch",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One failed switch",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One failed cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One failed switch and cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 20% failed switches",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 20% failed cables",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 20% failed switches and cables",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: Packet drop rate of 10^-4",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One switch degraded",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One cable degraded",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 20% degraded switches",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 20% degraded cables",
    ]
    failures_input = [
        "10_percent_us-cs-cables-per-switch",
        "fail_one_switch",
        "fail_one_cable",
        "fail_one_switch_one_cable",
        "20_percent_failed_switches",
        "20_percent_failed_cables",
        "20_percent_failed_switches_and_cables",
        "packet_drop_rate_10^-4",
        "degrade_one_switch",
        "degrade_one_cable",
        "20_percent_degraded_switches",
        "20_percent_degraded_cables",
    ]
    nodes = [
        128,
        1024,
        1024,
        1024,
        1024,
        1024,
        1024,
        1024,
        1024,
        1024,
        1024,
        1024,
    ]
    topology = [
        "fat_tree_128_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
        "fat_tree_1024_8os_800.topo",
    ]
    connection_matrix = [
        "perm_128_2",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
        "incast_1024_32_4MiB",
    ]

    for i, title in enumerate(titles):
        run(
            failures_input[i],
            title,
            failures_input[i],
            2**22,
            nodes[i],
            topology[i],
            connection_matrix[i],
        )


if __name__ == "__main__":
    main()
