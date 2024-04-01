import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

plt.figure(figsize=(6, 4.5))  # Adjust the width and height as needed


# Read the file
file_path = 'imp_values/cwnd1.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
print(df['X'])
df['Y'] = (df['Y'] * 2)
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.plot(df.iloc[::10, 0], df.iloc[::10, 1], linewidth=3.5,alpha=0.7)
# Read the file
file_path = 'imp_values/cwnd2.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
print(df['X'])
df['Y'] = (df['Y'] * 2)
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.plot(df.iloc[::10, 0], df.iloc[::10, 1], linewidth=3.5,alpha=0.7)
file_path = 'imp_values/cwnd3.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
print(df['X'])
df['Y'] = (df['Y'] * 2)
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.plot(df.iloc[::10, 0], df.iloc[::10, 1], linewidth=3.5,alpha=0.7)

file_path = 'imp_values/cwnd4.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
print(df['X'])
df['Y'] = (df['Y'] * 2)
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.plot(df.iloc[::10, 0], df.iloc[::10, 1], linewidth=3.5,alpha=0.7)

plt.axhline(y=25, color='k', linestyle='--', linewidth=2, alpha=0.5)


plt.xlabel('Time (ms)')  # Replace with appropriate label
plt.ylabel('Sending Throughput (Gbps)')  # Replace with appropriate label
plt.ylim(0, 100)  # Set x-axis limits
#plt.show()
plt.savefig('incast1.pdf', bbox_inches='tight')
