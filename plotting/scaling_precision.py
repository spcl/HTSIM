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

bw_ndp = {}
bw_bts = {}
bw_trimming = {}
size_incast = 0
size_p = 0

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
precision = 0

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

            # Theo BW
            
            result = re.search(r"Precision: (\d+)", line)
            if result:
                precision = int(int(result.group(1)) / 1000)

            # Size
            result = re.search(r"Incast Size: (\d+)", line)
            if result:
                size_incast = int(result.group(1))
                size_p = size_incast
                # Theoretical BW
                latency = args.latency
                if (args.incast_degree != -1):
                    theoretical_bw = ((((size_incast * 0.015625) + size_incast ) * 8) * args.incast_degree / args.link_speed) + (latency * 12) + ((4096+64)*8*6/args.link_speed) +  (64*8*6/args.link_speed)
                else:
                    theoretical_bw = ((((size_incast * 0.015625) + size_incast ) * 8) * 1 / (args.link_speed / 4)) + (latency * 12) + ((4096+64)*8*6/(args.link_speed / 4)) +  (64*8*6/(args.link_speed / 4))
            # BW
            result = re.search(r"Max FCT: (\d+.\d+)", line)
            if result:
                bw_1 = float(result.group(1))
                print(precision)
                print(bw_1)
                print(theoretical_bw)
                print()
                bw_1 = 1 /(bw_1 / theoretical_bw)
                if (bw_1 > 1):
                    bw_1 = 1
                if ("NDP" in str(files)):
                    bw_ndp[precision] = bw_1
                elif ("Swift" in str(files)):
                    bw_bts[precision] = bw_1
                elif ("SMaRTT" in str(files)):
                    bw_trimming[precision] = bw_1
        

bw_bts = dict(sorted(bw_bts.items()))
bw_trimming = dict(sorted(bw_trimming.items()))
bw_ndp = dict(sorted(bw_ndp.items()))
print(bw_trimming)

# Calculate percentage difference between Line 2 and Line 1
y1 = list(bw_bts.values())
y2 = list(bw_trimming.values())
percentage_diff = [((y2_val - y1_val) / y1_val) * 100 for y1_val, y2_val in zip(y2, y1)]

plt.figure(figsize=(8.5, 5))

# Increase the size of the plot
plt.figure(figsize=(8.5, 5))

# Plot Line 3
x3 = list(bw_ndp.keys())
y3 = list(bw_ndp.values())
plt.plot(x3, y3, marker='x', label='EQDS', linewidth=2)

# Plot Line 1
x1 = list(bw_bts.keys())
y1 = list(bw_bts.values())
plt.plot(x1, y1, marker='s', label='Swift*', linewidth=2)




# Plot Line 2
x2 = list(bw_trimming.keys())
y2 = list(bw_trimming.values())
plt.plot(x2, y2, marker='s', label='SMaRTT', linewidth=2)

x_vertical_line = args.bdp / 1000

#plt.axvline(x_vertical_line, color='gray', linestyle='dashed', alpha=0.3, linewidth=2)

# Set x-axis to log2 scale
#plt.xscale('log', base=2)
#plt.ylim(0.85, 1.01)
# Set plot labels and title
plt.xlabel('Timestamp Precision (ns)',fontsize=17)
plt.ylabel('Normalized Runtime to Theoretical Best',fontsize=17)
if (args.incast_degree == -1):
    plt.title('Performance drop due to precision loss - Permutation {} KiB\nLink Speed {}Gbps - 4KiB MTU'.format(size_p, args.link_speed),fontsize=17)
else:
    plt.title('Performance drop due to precision loss - {}:1 Incast\nLink Speed {}Gbps - 4KiB MTU'.format(args.incast_degree, args.link_speed),fontsize=17)#plt.gca().xaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x)))
plt.grid()  #just add this
plt.legend([],[], frameon=False)
plt.xticks(fontsize=16)  # Set the font size for x-axis tick labels
plt.yticks(fontsize=16)  # Set the font size for y-axis tick labels
# Make boxplot for one group only
plt.tight_layout()
plt.savefig(folder + "/scaling.png", bbox_inches='tight')
plt.savefig(folder + "/scaling.pdf", bbox_inches='tight')
plt.savefig(folder + "/scaling.svg", bbox_inches='tight')