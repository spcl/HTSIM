import re
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

list_latencies_ndp = []
with open("input/ndp_incast_16.tmp") as file:
    for line in file:
        result = re.search(r"Received ACK: (\d+)", line)
        if result:
            rtt = int(result.group(1))
            list_latencies_ndp.append(rtt)

list_latencies_uec = []
with open("input/uec_incast_16.tmp") as file:
    for line in file:
        result = re.search(r"Received ACK: (\d+)", line)
        if result:
            rtt = int(result.group(1))
            list_latencies_uec.append(rtt)


# set a grey background (use sns.set_theme() if seaborn version 0.11.0 or above) 
sns.set(style="darkgrid")

# initialize list of lists
df = pd.DataFrame()
list_latencies_uec = [x / 1000 for x in list_latencies_uec]
list_latencies_ndp = [x / 1000 for x in list_latencies_ndp]
df['Sender Based CC'] = list_latencies_uec
df['EQDS / NDP'] = list_latencies_ndp

  
# Create the pandas DataFrame

df = pd.DataFrame({'Sender Based CC': list_latencies_uec, 'EQDS / NDP': list_latencies_ndp})
my = sns.violinplot(data=df)
my.set_title('Latency Distribution')
my.set_ylabel('Packet RTT (us)')
 
# Make boxplot for one group only
plt.savefig('foo.png', bbox_inches='tight')
plt.show()


