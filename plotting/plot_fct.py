import re
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

list_latencies_ndp = []
with open("input/ndp_perm128_9k.tmp") as file:
    for line in file:
        result = re.search(r"Flow  finished at (\d+)", line)
        if result:
            rtt = int(result.group(1))
            list_latencies_ndp.append(rtt)

list_latencies_uec = []
with open("input/uec_perm128_9k.tmp") as file:
    for line in file:
        result = re.search(r"Flow uecsrc completion time is (\d+)", line)
        if result:
            rtt = int(result.group(1))
            if (rtt == 1):
                rtt = 251
            list_latencies_uec.append(rtt)


# set a grey background (use sns.set_theme() if seaborn version 0.11.0 or above) 
sns.set(style="darkgrid")

# initialize list of lists
df = pd.DataFrame()
# list_latencies_uec = [x / 1000 for x in list_latencies_uec]
# list_latencies_ndp = [x / 1000 for x in list_latencies_ndp]
list_latencies_uec.sort()
list_latencies_ndp.sort()
df['Sender Based CC'] = list_latencies_uec
df['EQDS / NDP'] = list_latencies_ndp

print(list_latencies_uec)
print(list_latencies_ndp)
# Create the pandas DataFrame

df = pd.DataFrame({'Sender Based CC': list_latencies_uec, 'EQDS / NDP': list_latencies_ndp})
my = sns.lineplot(data=df[['Sender Based CC', 'EQDS / NDP']])

my.set_title('Flow Completion Time Permutation Workload - 9KiB Packets')
my.set_ylabel('FCT (us)')
 
# Make boxplot for one group only
plt.ylim(200, 280)
plt.savefig('foo.png', bbox_inches='tight')
plt.show()


