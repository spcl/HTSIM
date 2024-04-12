import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

plt.figure(figsize=(7, 3.5))  # Adjust the width and height as needed


# Read the file
file_path = 'imp_values/queueDCTCP.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
print(df['X'])
df['Y'] = (df['Y'] * 50 / 8) / 1000
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.scatter(df.iloc[::10, 0], df.iloc[::10, 1], marker='o')

# Read the file
file_path = 'imp_values/queuePH.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
print(df['X'])
df['Y'] = (df['Y'] * 50 / 8) / 1000
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.scatter(df.iloc[::10, 0], df.iloc[::10, 1], marker='o')


plt.xlabel('Time (ms)')  # Replace with appropriate label
plt.ylabel('Queue Size (KB)')  # Replace with appropriate label
plt.xlim(19.5, 20.5)  # Set x-axis limits
plt.ylim(0, 300)  # Set x-axis limits
plt.savefig('phantom_avg.pdf', bbox_inches='tight')
