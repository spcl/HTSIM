import re
import numpy as np
import pandas as pd
import seaborn as sns
from pathlib import Path
import matplotlib.pyplot as plt
import argparse


list_bw = []
list_names = []
df = pd.DataFrame()
i = 0
temp_bw = []

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
            # BW
            result = re.search(r"Effective BW: (\d+.\d+)", line)
            if result:
                bw_1 = float(result.group(1))
            # Min BW
            result = re.search(r"Min BW: (\d+.\d+)", line)
            if result:
                min_bw = float(result.group(1))
                temp_bw.append(min_bw)
        list_bw.append(average(temp_bw))
        i += 1
        temp_bw = []

sns.set(style="darkgrid")
# Create a DataFrame from the lists
df = pd.DataFrame({'Effective Bandwidth (Gb/s)': list_bw, 'Y_Labels': list_names})
# Create the bar plot using Seaborn
my = sns.barplot(y='Effective Bandwidth (Gb/s)', x='Y_Labels', data=df)

my.set_title('Effective Bandwidth Plot - Sender Completion')
my.set_ylabel('Effective Bandwidth (Gb/s)')
my.set_xlabel('Congestion Control Algorithm')
 
# Make boxplot for one group only
plt.tight_layout()
plt.savefig(folder + "/bw.png", bbox_inches='tight')

#plt.show()


