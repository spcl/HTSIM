import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

sns.set(style="whitegrid")


fig, ax1 = plt.subplots(figsize=(7, 3.5)) # initializes figure and plots
ax2 = ax1.twinx() # applies twinx to ax2, which is the second y axis. 


# Read the file
file_path = 'imp_values/queueNOP.txt'
df1 = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
df1['Y'] = (df1['Y'] * 50 / 8) / 1000
df1['X'] = (df1['X'] / 1000 / 1000)
sns.set(style="whitegrid")

sns.scatterplot(data=df1.iloc[::50, :], x='X', y='Y', marker='o', s=18,  ax = ax1, linewidth=0)

# Read the file
file_path = 'imp_values/queueYESP.txt'
df2 = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
df2['Y'] = (df2['Y'] * 50 / 8) / 1000
df2['X'] = (df2['X'] / 1000 / 1000)
sns.set(style="whitegrid")

sns.scatterplot(data=df2.iloc[::50, :], x='X', y='Y', marker='o', s=18,  ax = ax1, linewidth=0)


# Read the file
file_path = 'imp_values/cwndNOP.txt'
df3 = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
df3['X'] = (df3['X'] / 1000 / 1000)
df3['Y'] = (df3['Y'] / 1000 / 1)
sns.set(style="whitegrid")

sns.lineplot(data=df3.iloc[::1, :], x='X', y='Y', ax = ax2, linewidth = 2.5)

# Read the file
file_path = 'imp_values/cwndYESP.txt'
df4 = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
df4['X'] = (df4['X'] / 1000 / 1000)
df4['Y'] = (df4['Y'] / 1000 / 1)
sns.set(style="whitegrid")

sns.lineplot(data=df4.iloc[::1, :], x='X', y='Y', ax = ax2, linewidth = 2.5)


sns.set(style="whitegrid")

plt.xlabel('Time (ms)')  # Replace with appropriate label
ax1.set_xlabel('Time (ms)')
ax1.set_ylabel('Queue Size (KB)')
ax2.set_ylabel('Congestion Window (KB)')
#plt.xlim(19.5, 20.5)  # Set x-axis limits
#plt.ylim(0, 300)  # Set x-axis limits

#plt.show()
plt.savefig('phantom_pac.pdf', bbox_inches='tight')
