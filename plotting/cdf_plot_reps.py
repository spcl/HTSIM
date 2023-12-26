import numpy as np
import matplotlib.pyplot as plt
import statsmodels.api as sm
import seaborn as sns
from pathlib import Path
import matplotlib.pyplot as plt
import argparse
import pandas as pd
import re
from  matplotlib.ticker import FuncFormatter



parser = argparse.ArgumentParser()
parser.add_argument('--input_file', dest='input_file', type=str, help='File to parse.')
parser.add_argument('--folder', dest='folder', type=str, help='Folder to parse and save')
parser.add_argument('--bdp', dest='bdp', type=int, help='BDP Value', default=0)
parser.add_argument('--incast_degree', dest='incast_degree', type=int, help='BDP Value', default=1)
parser.add_argument('--link_speed', dest='link_speed', type=int, help='In Gbps', default=1)
parser.add_argument('--latency', dest='latency', type=int, help='Latency in NS', default=1)
parser.add_argument('--name', dest='name', type=int, help='Name', default=1)
args = parser.parse_args()


list_names = []
df = pd.DataFrame()
i = 0

bw_ndp = {}
bw_bts = {}
bw_trimming = {}
size_incast = 1048100
folder = args.folder
file_name = args.input_file
my_bw = 0
list_fct_smartt = []
list_fct_swift = []
list_fct_ndp = []
best_time = 0
list_fct_oblivious = []

pathlist = Path(folder).glob('**/*.tmp')
for files in sorted(pathlist):
    if ("GeneratedReport" not in str(files)):
        continue
    with open(files) as file:
        for line in file:
            # Theo BW
            result = re.search(r"Theo BW: (\d+)", line)
            if result:
                my_bw = int(result.group(1))
            # Size
            result = re.search(r"Size: (\d+)", line)
            if result:
                size_incast = int(result.group(1))
                # Theoretical BW
                latency = args.latency
                theoretical_bw = ((((size_incast * 0.015625) + size_incast ) * 8) * 1 / args.link_speed) + (latency * 12) + ((4096+64)*8*6/args.link_speed) +  (64*8*6/args.link_speed)
                best_time = theoretical_bw / 1000
            # BW
            result = re.search(r"FCT: (\d+.\d+)", line)
            if result:
                if (int(result.group(1)) / 1000):
                    actual = int(result.group(1)) / 1000
                else:
                    actual =  int(result.group(1)) / 1000
                if ("ECMP" in str(files) and "REPS" not in str(files) ):
                    list_fct_ndp.append(actual)
                elif ("REPS" in str(files)):
                    list_fct_swift.append(actual)
                elif ("Obl" in str(files)):
                    list_fct_oblivious.append(actual)


# Combine all data into a list of lists
all_data = [list_fct_ndp, list_fct_swift, list_fct_oblivious]

# Create a list of labels for each dataset
labels = ['SMaRTT Per-Flow ECMP', 'SMaRTT-REPS', 'SMaRTT Oblivious Spraying']

# Initialize an empty list to store cumulative probabilities
cumulative_probabilities = []

# Step 2: Sort and compute cumulative probabilities for each dataset
for data in all_data:
    data.sort()
    n = len(data)
    cumulative_probabilities.append(np.arange(1, n + 1) / n)

# Step 3: Plot the CDFs using Seaborn
plt.figure(figsize=(7, 5))

for i, data in enumerate(all_data):
    ax = sns.lineplot(x=data, y=cumulative_probabilities[i], label=labels[i], linewidth = 2)

ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = 15)
ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 15)

# Add a vertical dashed line at x=1000 with lower opacity
plt.axvline(x=best_time, linestyle='--', color='gray', alpha=0.5, linewidth = 2)

# Add text next to the vertical line
plt.text(best_time + 20, 0.33, 'Ideal Completion Time', rotation=90, va='center', color='gray',fontsize=15)

# Move the legend to the top right
plt.legend(loc='upper left')
#plt.xlim(1200, 2000)

# Set labels and title
plt.xlabel('Flow Completion Time (us)',fontsize=15)
plt.ylabel('CDF (%)',fontsize=15)
plt.title('Permtuation 4:1 oversubscribed FT 32MiB\nLink Speed 800Gbps - 4KiB MTU',fontsize=17)
plt.grid()  #just add this
plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x)))

plt.legend([],[], frameon=False)
# Show the plot
plt.tight_layout()
plt.savefig(folder + "/scaling.png", bbox_inches='tight')
plt.savefig(folder + "/scaling.pdf", bbox_inches='tight')
plt.savefig(folder + "/scaling.svg", bbox_inches='tight')