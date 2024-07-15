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

def speedUpPlot(df, title):
    replacement_rules = {
    "fail_one_switch_after_10us_for_100us": "One failed switch after 10µs for 100µs",
    "fail_one_switch": "One failed switch",
    "one_switch_drops_every_10th_packet": "One switch drops every 10th packet",
    "fail_one_cable_after_10us_for_100us": "One failed cable after 10µs for 100µs",
    "fail_one_cable": "One failed cable",
    "one_cable_drops_every_10th_packet": "One cable drops every 10th packet",
    "degrade_one_switch": "One degraded switch",
    "degrade_one_cable": "One degraded cable",
    "ber_switch_one_percent": "1% corrupted Packets at switches",
    "ber_cable_one_percent": "1% corrupted packets at cables",
    "10_percent_failed_switches": "10% failed switches",
    "10_percent_failed_cables": "10% failed cables",
    "10_percent_us-cs-cables-per-switch": "10% failed US-CS cables per switch",
    "10_percent_degraded_switches": "10% degraded switches",
    "10_percent_degraded_cables": "10% degraded cables",
    "fail_new_switch_every_100us_for_50us": "Fail new switch every 100µs for 50µs",
    "fail_new_cable_every_100us_for_50us": "Fail new cable every 100µs for 50µs",
    "fail_one_switch_and_cable_after_10us_for_100us": "Fail one switch&cable after 10µs for 100µs",
    "fail_one_switch_one_cable": "Fail one switch&cable",
    "fail_one_switch_and_cable_degrade_one_switch_and_cable": "Fail one switch&cable \n  degrade one switch&cable",
    "10_percent_failed_switches_and_cables": "10% failed switches&cables"
    }

    modes = df["Mode"].tolist()
    experiments = [replacement_rules[string] if string in replacement_rules else string for string in modes]
    time_REPS = df["REPS"].tolist()
    time_REPSC = df["REPS Circular"].tolist()
    time_REPSCNF = df["REPS Circular\n without freezing"].tolist()
    time_SPRAYING = df["Spraying"].tolist()

    speedup_REPS_percent = (np.array(time_SPRAYING) / np.array(time_REPS) - 1) * 100
    speedup_REPSC_percent = (np.array(time_SPRAYING) / np.array(time_REPSC) - 1) * 100
    speedup_REPSCNF_percent = (np.array(time_SPRAYING) / np.array(time_REPSCNF) - 1) * 100

    fig, ax = plt.subplots(figsize=(10, 6))

    y_pos = np.arange(len(experiments))

    marker_size = 100
    marker_alpha = 0.6

    ax.scatter(speedup_REPS_percent, y_pos, label='Speedup REPS', color='blue', marker='o', s=marker_size, alpha=marker_alpha)
    ax.scatter(speedup_REPSC_percent, y_pos, label='Speedup REPS Circular', color='green', marker='o', s=marker_size, alpha=marker_alpha)
    ax.scatter(speedup_REPSCNF_percent, y_pos, label='Speedup REPS Circular\n without freezing', color='orange', marker='o', s=marker_size, alpha=marker_alpha)

    for i, txt in enumerate(speedup_REPS_percent):
        ax.annotate(f'{txt:.1f}%', (speedup_REPS_percent[i], y_pos[i]), textcoords="offset points", xytext=(5, 5), ha='center', color='blue', clip_on=True, alpha=1)
    for i, txt in enumerate(speedup_REPSC_percent):
        ax.annotate(f'{txt:.1f}%', (speedup_REPSC_percent[i], y_pos[i]), textcoords="offset points", xytext=(5, 15), ha='center', color='green', clip_on=True, alpha=1)
    for i, txt in enumerate(speedup_REPSCNF_percent):
        ax.annotate(f'{txt:.1f}%', (speedup_REPSCNF_percent[i], y_pos[i]), textcoords="offset points", xytext=(5, -15), ha='center', color='orange', clip_on=True, alpha=1)

    ax.set_xlabel("Speedup (%)")
    ax.set_ylabel("Experiments")
    ax.set_title("Speedup Comparison \n" + title + "\n" + "Baseline: Spraying")
    ax.set_yticks(y_pos)
    ax.set_yticklabels(experiments)
    ax.axvline(x=0, color='grey', linestyle='--')
    ax.legend()
    ax.set_ylim(-0.5, len(experiments))

    ax.grid(True)

    ax.xaxis.set_major_formatter(FuncFormatter(lambda x, _: f'{x:.0f}%'))

    plt.tight_layout()
    plt.show()
    # plt.savefig("Speedup-Comparison", bbox_inches="tight")

def getSumRow(df):
    sum_row = {
        "ExperimentName": "",
        "REPS": df["REPS"].sum(),
        "REPS Circular": df["REPS Circular"].sum(),
        "REPS Circular\n without freezing": df["REPS Circular\n without freezing"].sum(),
        "Spraying": df["Spraying"].sum(),
        "Mode": "Overall"
    }
    return sum_row


def main():
    df = pd.read_csv('experiment_data_save.csv')
    df_Per128os1 = df[df['ExperimentName'].str.startswith('Permutation 128 - 800Gpbs - 4KiB MTU - 1:1', na=False)].reset_index(drop=True)
    df_Per128os8 = df[df['ExperimentName'].str.startswith('Permutation 128 - 800Gpbs - 4KiB MTU - 8:1', na=False)].reset_index(drop=True)
    df_Incast =    df[df['ExperimentName'].str.startswith('Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1', na=False)].reset_index(drop=True)

    df_Per128os1 = df_Per128os1._append(getSumRow(df_Per128os1), ignore_index=True)
    df_Per128os8 = df_Per128os8._append(getSumRow(df_Per128os8), ignore_index=True)
    df_Incast = df_Incast._append(getSumRow(df_Incast), ignore_index=True)

    speedUpPlot(df_Per128os1, "Permutation 128 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows")
    speedUpPlot(df_Per128os8, "Permutation 128 - 800Gpbs - 4KiB MTU - 8:1 Oversubscription - 2MB Flows")
    speedUpPlot(df_Incast, "Incast 32:1 - 800Gpbs - 4KiB MTU - 1:1 Oversubscription - 2MB Flows")

if __name__ == "__main__":
    main()