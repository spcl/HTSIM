import numpy as np
import matplotlib.pyplot as plt
import statsmodels.api as sm
import seaborn as sns
from pathlib import Path
import matplotlib.pyplot as plt
import argparse
import pandas as pd
import re


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
            result = re.search(r"Incast Size: (\d+)", line)
            if result:
                size_incast = int(result.group(1))
                # Theoretical BW
                latency = args.latency
                theoretical_bw = ((((size_incast * 0.015625) + size_incast ) * 8) * 1 / args.link_speed) + (latency * 12) + ((4096+64)*8*6/args.link_speed) +  (64*8*6/args.link_speed)
            # BW
            result = re.search(r"FCT: (\d+.\d+)", line)
            if result:
                if ("NDP" in str(files)):
                    list_fct_ndp.append(int(result.group(1)))
                elif ("Swift" in str(files)):
                    list_fct_swift.append(int(result.group(1)))
                elif ("SMaRTT" in str(files)):
                    list_fct_smartt.append(int(result.group(1)))


# Step 2: Sort the flow completion times for each dataset
list_fct_ndp.sort()
list_fct_swift.sort()
list_fct_smartt.sort()

# Step 3: Compute the ECDF for each dataset
ecdf1 = sm.distributions.ECDF(list_fct_ndp)
ecdf2 = sm.distributions.ECDF(list_fct_swift)
ecdf3 = sm.distributions.ECDF(list_fct_smartt)

# Step 4: Create smooth CDF curves for each dataset
x = np.linspace(min(min(list_fct_ndp), min(list_fct_swift), min(list_fct_smartt)), max(max(list_fct_ndp), max(list_fct_swift), max(list_fct_smartt)), 1000)
y1 = ecdf1(x)
y2 = ecdf2(x)
y3 = ecdf3(x)

sns.set(style="darkgrid")

# Increase the size of the plot
fig = plt.figure(figsize=(6.65, 3.65))
ax1 = fig.add_subplot(111)
# Plot Line 1
sns.lineplot(x=x, y=y2, marker='s', label='Swift*')
# Plot Line 2
sns.lineplot(x=x, y=y3, marker='s', label='SMaRTT')

# Plot Line 3
sns.lineplot(x=x, y=y1, marker='x', label='EQDS')

# Set labels and title
plt.xlabel('Flow Completion Time')
plt.ylabel('CDF')
plt.title('CDF of Flow Completion Times')

# Show the plot
plt.grid(True)
plt.show()
