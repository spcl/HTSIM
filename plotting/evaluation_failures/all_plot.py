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


def run(short_title, title, failures_input, msg_size):

    os.system("mkdir -p experiments")
    os.system("rm -rf experiments/{}".format(short_title))
    os.system("mkdir experiments/{}".format(short_title))

    out_name = "experiments/{}/out-reps.txt".format(short_title)
    string_to_run = "../../sim/datacenter/htsim_uec_entry_modern -o uec_entry -algorithm smartt -strat reps -use_freezing_reps  -end_time 10 -bonus_drop 1.5 -nodes 1024 -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 700 -reuse_entropy 1 -topo ../../sim/datacenter/topologies/fat_tree_1024_8os_800.topo -tm ../../sim/datacenter/connection_matrices/incast_1024_32_4MiB -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../../sim/datacenter/../failures_input/{}.txt > {}".format(
        failures_input, out_name
    )
    os.system(string_to_run)
    list_reps = getListFCT(out_name)
    num_nack_reps = getNumTrimmed(out_name)
    print(
        "REPS: Flow Diff {} - Total {}".format(
            max(list_reps) - min(list_reps), max(list_reps)
        )
    )

    out_name = "experiments/{}/out-repsC.txt".format(short_title)
    string_to_run = "../../sim/datacenter/htsim_uec_entry_modern -o uec_entry -algorithm smartt -strat reps_circular -use_freezing_reps -end_time 10 -bonus_drop 1.5 -nodes 1024 -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 700 -reuse_entropy 1 -topo ../../sim/datacenter/topologies/fat_tree_1024_8os_800.topo -tm ../../sim/datacenter/connection_matrices/incast_1024_32_4MiB -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../../sim/datacenter/../failures_input/{}.txt > {}".format(
        failures_input, out_name
    )
    os.system(string_to_run)
    list_repsC = getListFCT(out_name)
    num_nack_repsC = getNumTrimmed(out_name)
    print(
        "REPS Circular: Flow Diff {} - Total {}".format(
            max(list_repsC) - min(list_repsC), max(list_repsC)
        )
    )

    out_name = "experiments/{}/out-Spraying.txt".format(short_title)
    string_to_run = "../../sim/datacenter/htsim_uec_entry_modern -o uec_entry -algorithm smartt -strat spraying -use_freezing_reps -end_time 10 -bonus_drop 1.5 -nodes 1024 -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 700 -reuse_entropy 1 -topo ../../sim/datacenter/topologies/fat_tree_1024_8os_800.topo -tm ../../sim/datacenter/connection_matrices/incast_1024_32_4MiB -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../../sim/datacenter/../failures_input/{}.txt > {}".format(
        failures_input, out_name
    )
    os.system(string_to_run)
    list_spraying = getListFCT(out_name)
    num_nack_spraying = getNumTrimmed(out_name)
    print(
        "Spraying: Flow Diff {} - Total {}".format(
            max(list_spraying) - min(list_spraying), max(list_spraying)
        )
    )

    list_fct = [list_reps, list_repsC, list_spraying]
    list_max = [max(list_reps), max(list_repsC), max(list_spraying)]
    list_nacks = [num_nack_reps, num_nack_repsC, num_nack_spraying]
    list_labels = ["REPS", "REPS Circular", "Spraying"]

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


def main():

    titles = [
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One failed switch",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One failed cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One failed switch and cable",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 30% failed switches",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 30% failed cables",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 30% failed switches and cables",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: Only one healthy path",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: Packet drop rate of 10^-4",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One switch degraded",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: One cable degraded",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 30% degraded switches",
        "Incast 32:1 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows \n Failure mode: 30% degraded cables",
    ]
    failures_input = [
        "fail_one_switch",
        "fail_one_cable",
        "fail_one_switch_one_cable",
        "30_percent_failed_switches",
        "30_percent_failed_cables",
        "30_percent_failed_switches_and_cables",
        "fail_all_switches_and_cables_except_one_path",
        "packet_drop_rate_10^-4",
        "degrade_one_switch",
        "degrade_one_cable",
        "30_percent_degraded_switches",
        "30_percent_degraded_cables",
    ]

    for i, title in enumerate(titles):
        run(failures_input[i], title, failures_input[i], 2**22)


if __name__ == "__main__":
    main()
