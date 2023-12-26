import re
import numpy as np
import pandas as pd
import seaborn as sns
from pathlib import Path
import matplotlib.pyplot as plt
import argparse


list_names = []
df = pd.DataFrame()
i = 0
num_bts_warning = 0
num_bts_dropped = 0
num_ecn = 0
num_nack = 0
list_num_bts_warning = []
list_num_bts_dropped = []
list_num_ecn = []
list_num_nack = []

parser = argparse.ArgumentParser()
parser.add_argument('--input_file', dest='input_file', type=str, help='File to parse.')
parser.add_argument('--folder', dest='folder', type=str, help='Folder to parse and save')
args = parser.parse_args()
folder = args.folder
file_name = args.input_file


def average(lst):
    return sum(lst) / len(lst)

pathlist = Path(folder).glob('**/*.tmp')
for files in sorted(pathlist):
    if ("GeneratedReport" not in str(files)):
        continue
    with open(files) as file:
        for line in file:
            # Name 
            if "Name: " in line:
                name = line.split(': ')[1]
                list_names.append(name)
            # BTS, ECN; NACK
            result = re.search(r"BTS Warning: (\d+)", line)
            if result:
                num_bts_warning = int(result.group(1))
            result = re.search(r"BTS Dropped: (\d+)", line)
            if result:
                num_bts_dropped = int(result.group(1))
            result = re.search(r"ECN: (\d+)", line)
            if result:
                num_ecn = int(result.group(1))
            result = re.search(r"NACK: (\d+)", line)
            if result:
                num_nack = int(result.group(1))
        list_num_bts_warning.append(num_bts_warning)
        list_num_bts_dropped.append(num_bts_dropped)
        list_num_ecn.append(num_ecn)
        list_num_nack.append(num_nack)
        i += 1

final_df = pd.DataFrame()
for idx, name in enumerate(list_names):
    for i in range(4):
        type_considered = ""
        value_considered = 0
        if (i == 0):
            type_considered = "BTS Warning"
            value_considered = list_num_bts_warning[idx]
        elif (i == 1):
            type_considered = "BTS Dropped"
            value_considered = list_num_bts_dropped[idx]
        elif (i == 2):
            type_considered = "ECN"
            value_considered = list_num_ecn[idx]
        elif (i == 3):
            type_considered = "NACK"
            value_considered = list_num_nack[idx]

        temp_df = pd.DataFrame({
            "Algorithm": name,
            "Signal": type_considered,
            "ActualValue": value_considered,
        }, index=[0])
        final_df = pd.concat([final_df, temp_df])
sns.set(style="darkgrid")
# Create the bar plot using Seaborn
ax = sns.barplot(x="Algorithm", y="ActualValue", hue="Signal", data=final_df)
ax.set(ylabel="Number Events", xlabel="Signal Considered")
ax.set_title('Congestion Signal Events')

# Make boxplot for one group only
plt.tight_layout()
plt.savefig(folder + "/events.png", bbox_inches='tight')
#plt.show()


