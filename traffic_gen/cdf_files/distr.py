import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

plt.figure(figsize=(7, 4))  # Adjust the width and height as needed


# Read the file
file_path = 'AliStorage2019.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], delim_whitespace=True, names=['X', 'Y'])
print(df['X'])
df['Y'] = df['Y'] / 100
sns.set(style="whitegrid")
plt.plot(df.iloc[::1, 0], df.iloc[::1, 1], linewidth=3.5,alpha=1, label='AliStorage2019')
# Read the file
file_path = 'FbHdp_distribution.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], delim_whitespace=True, names=['X', 'Y'])
print(df['X'])
df['Y'] = df['Y'] / 100
sns.set(style="whitegrid")
#plt.plot(df.iloc[::1, 0], df.iloc[::1, 1], linewidth=3.5,alpha=1, label='FbHdp_distribution')
file_path = 'WebSearch_distribution.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], delim_whitespace=True, names=['X', 'Y'])
print(df['X'])
df['Y'] = df['Y'] / 100
sns.set(style="whitegrid")
plt.plot(df.iloc[::1, 0], df.iloc[::1, 1], linewidth=3.5,alpha=1, label='WebSearch_distribution')


plt.xscale('log')  # Set x-axis scale to logarithmic

plt.xlabel('Message Size (B)')  # Replace with appropriate label
plt.ylabel('CDF')  # Replace with appropriate label
plt.xlim(100, 30000000)  # Set x-axis limits
#plt.show()
plt.savefig('incast2.pdf', bbox_inches='tight')
