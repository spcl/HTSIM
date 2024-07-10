import re
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

list_latencies_uec = []
with open("../sim/datacenter/out2.tmp") as file:
    for line in file:
        result = re.search(r"Flow Completion time is (\d+)", line)
        if result:
            rtt = int(result.group(1))
            list_latencies_uec.append(rtt)


# set a grey background (use sns.set_theme() if seaborn version 0.11.0 or above) 
sns.set(style="darkgrid")

# initialize list of lists
df = pd.DataFrame()
list_latencies_uec.sort()
df['SMaRTT'] = list_latencies_uec

print(list_latencies_uec)
# Create the pandas DataFrame

""" df = pd.DataFrame({'Sender Based CC': list_latencies_uec})
my = sns.lineplot(data=df[['Sender Based CC']]) """


cumulative_probabilities = []

# Step 2: Sort and compute cumulative probabilities for each dataset
for data in list_latencies_uec:
    data.sort()
    n = len(data)
    cumulative_probabilities.append(np.arange(1, n + 1) / n)

# Step 3: Plot the CDFs using Seaborn
plt.figure(figsize=(6, 4.6))

for i, data in enumerate(list_latencies_uec):
    ax = sns.lineplot(x=data, y=cumulative_probabilities[i], label="SMaRTT", linewidth = 3.3)

ax.set_title('Flow Completion Time Permutation Workload')
ax.set_ylabel('FCT (us)')
 
# Make boxplot for one group only
plt.savefig('foo.png', bbox_inches='tight')
plt.show()


