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

increase_list = []
benchmark = []

pathlist = Path(folder).glob('**/*.tmp')
for files in sorted(pathlist):
    if ("GeneratedReport" not in str(files)):
        continue
    smartt_time = 0
    with open(files) as file:
        if ("Drop" in str(files)):
            pathlist2 = Path(folder).glob('**/*.tmp')
            for compared_file in sorted(pathlist2):
                
                str_com = str(files)
                str_com = str_com.replace("SMaRTT Drop", "SMaRTT")
                #print("{} vs {}".format(str_com, compared_file))
                if (str(compared_file) == str(str_com)):
                    with open(compared_file) as compared_file_o:
                        for line_c in compared_file_o:
                            # Time
                            result = re.search(r"Max FCT: (\d+)", line_c)
                            if result:
                                smartt_time = int(result.group(1))

            for line in file:
                # Time
                result = re.search(r"Max FCT: (\d+)", line)
                if result:
                    smartt_no_trimming = int(result.group(1))   
                    print("{} --> {} vs {}\n".format(str(files), smartt_time,smartt_no_trimming )) 
                    if (smartt_no_trimming - smartt_time < 0):
                        increase_list.append(1)
                    else:
                        increase_list.append(((smartt_no_trimming/1000) - (smartt_time/1000))/1)
                    benchmark.append(str(files).replace("TRIMMING/","").replace("GeneratedReportSMaRTT Drop", ""))

benchmark = ["100:1\n32MiB", "100:1\n4MiB", "100:1\n512KiB", "Perm.\n16MiB", "32:1\n32MiB", "32:1\n4MiB", "32:1\n512KiB", "Perm.\n4MiB","8:1\n32MiB", "8:1\n4MiB", "8:1\n512KiB",]
print(benchmark)
print(increase_list)

plt.figure(figsize=(7, 5))

# Define the custom sorting order
custom_order = ["8:1\n512KiB", "8:1\n4MiB", "8:1\n32MiB","32:1\n512KiB", "32:1\n4MiB", "32:1\n32MiB","100:1\n512KiB", "100:1\n4MiB", "100:1\n32MiB","Perm.\n4MiB","Perm.\n16MiB",]

# Sample DataFrame
data = pd.DataFrame({'Values': increase_list, 'Strings': benchmark})

# Create a categorical data type with the custom order
custom_order_dtype = pd.CategoricalDtype(categories=custom_order, ordered=True)

# Apply the custom order to the 'Strings' column
data['Strings'] = data['Strings'].astype(custom_order_dtype)

# Sort the DataFrame based on the custom order
data = data.sort_values(by='Strings').reset_index(drop=True)

# Set a single color palette for all bars
custom_colors = ["#5975A4"] * 9 + ["#CC8963", "#CC8963"]  # Customize the colors as needed

# Create the bar plot
ax = sns.barplot(x='Strings', y='Values', data=data, palette=custom_colors)

# Add a horizontal dashed line at a certain y-value (e.g., y=15)
y_value = 8645 / 1000
plt.axhline(y=y_value, color='gray', linestyle='--', alpha=0.5)
ax.set_axisbelow(True)

# Add text near the line
plt.text(0.5, y_value + 0.5, 'Base RTT', color='black', fontsize=12, ha='center')

# Add a horizontal dashed line at a certain y-value (e.g., y=15)
y_value = 8645 * 2 / 1000
plt.axhline(y=y_value, color='gray', linestyle='--', alpha=0.65)

# Add text near the line
plt.text(0.5, y_value - 1, '2x Base RTT', color='black', fontsize=12, ha='center')

#ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = 15)
ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 15)

# Add labels and a title
plt.xlabel('Experiment',fontsize=15)
plt.ylabel('Additional delay due to no trimming (us)',fontsize=15)
plt.title('Trimming vs Timeout, multiple scenarios\nLink Speed 800Gbps - 4KiB MTU',fontsize=17)
plt.grid()  #just add this
plt.gca().yaxis.set_major_formatter(FuncFormatter(lambda x, _: int(x)))

# Show the plot
plt.tight_layout()
plt.savefig(folder + "/scaling.svg", bbox_inches='tight')
plt.savefig(folder + "/scaling.png", bbox_inches='tight')
plt.savefig(folder + "/scaling.pdf", bbox_inches='tight')