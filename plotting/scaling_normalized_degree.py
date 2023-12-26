import re
import numpy as np
import pandas as pd
import seaborn as sns
from pathlib import Path
import matplotlib.pyplot as plt
import argparse
from matplotlib.ticker import ScalarFormatter


list_names = []
df = pd.DataFrame()
i = 0

bw_ndp = {}
bw_bts = {}
bw_trimming = {}
size_incast = 0

parser = argparse.ArgumentParser()
parser.add_argument('--input_file', dest='input_file', type=str, help='File to parse.')
parser.add_argument('--folder', dest='folder', type=str, help='Folder to parse and save')
parser.add_argument('--bdp', dest='bdp', type=int, help='BDP Value', default=0)
parser.add_argument('--incast_degree', dest='incast_degree', type=int, help='BDP Value', default=1)
parser.add_argument('--link_speed', dest='link_speed', type=int, help='In Gbps', default=1)
parser.add_argument('--latency', dest='latency', type=int, help='Latency in NS', default=1)
parser.add_argument('--name', dest='name', type=int, help='Name', default=1)




args = parser.parse_args()
folder = args.folder
file_name = args.input_file
my_bw = 0
degree_incast = 0
def average(lst):
    return sum(lst) / len(lst)

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
            result = re.search(r"IncastDegree: (\d+)", line)
            if result:
                degree_incast = int(result.group(1))
            # Size
            result = re.search(r"Incast Size: (\d+)", line)
            if result:
                size_incast = int(result.group(1))
                # Theoretical BW
                latency = args.latency
                theoretical_bw = ((((size_incast * 0.015625) + size_incast ) * 8) * degree_incast / args.link_speed) + (latency * 12) + ((4096+64)*8*6/args.link_speed) +  (64*8*6/args.link_speed)
            # BW
            result = re.search(r"Max FCT: (\d+.\d+)", line)
            if result:
                bw_1 = float(result.group(1))
                print(size_incast * 1000)
                print(bw_1)
                print(theoretical_bw)
                print()
                bw_1 = 1 /(bw_1 / theoretical_bw)
                if (bw_1 > 1):
                    bw_1 = 1
                if ("NDP" in str(files)):
                    bw_ndp[degree_incast] = bw_1
                elif ("Swift" in str(files)):
                    bw_bts[degree_incast] = bw_1
                elif ("SMaRTT" in str(files)):
                    bw_trimming[degree_incast] = bw_1
        

bw_bts = dict(sorted(bw_bts.items()))
bw_trimming = dict(sorted(bw_trimming.items()))
bw_ndp = dict(sorted(bw_ndp.items()))
print(bw_ndp)
print(bw_trimming)

# Calculate percentage difference between Line 2 and Line 1
y1 = list(bw_bts.values())
y2 = list(bw_trimming.values())
percentage_diff = [((y2_val - y1_val) / y1_val) * 100 for y1_val, y2_val in zip(y2, y1)]

sns.set(style="darkgrid")

# Increase the size of the plot
fig = plt.figure(figsize=(6.65, 3.65))
ax1 = fig.add_subplot(111)
# Plot Line 1
x1 = list(bw_bts.keys())
y1 = list(bw_bts.values())
ax1.plot(x1, y1, marker='s', label='Swift*')
# Plot Line 2
x2 = list(bw_trimming.keys())
y2 = list(bw_trimming.values())
ax1.plot(x2, y2, marker='s', label='SMaRTT')

# Plot Line 3
x3 = list(bw_ndp.keys())
y3 = list(bw_ndp.values())
ax1.plot(x3, y3, marker='x', label='EQDS')


x_vertical_line = args.bdp / 1000


# Set x-axis to log2 scale
plt.xscale('log', base=2)
for axis in [ax1.xaxis, ax1.yaxis]:
    axis.set_major_formatter(ScalarFormatter())
# Set plot labels and title
plt.xlabel('Incast Degree')
plt.ylabel('Normalized to Theoretical Best')
size_incast_str = ""
if (size_incast == 4096): 
    size_incast_str = "4KiB"
elif (size_incast == 1048576):
    size_incast_str = "1MiB"
elif(size_incast == 33554432):
    size_incast_str = "32MiB"
plt.title('Scaling During {} Incast - Link Speed {}Gbps - 4KiB MTU'.format(size_incast_str, args.link_speed))
plt.legend()

 
# Make boxplot for one group only
plt.tight_layout()
plt.savefig(folder + "/scaling.png", bbox_inches='tight')
plt.savefig(folder + "/scaling.pdf", bbox_inches='tight')