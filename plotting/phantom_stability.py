import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

plt.figure(figsize=(7, 3.5))  # Adjust the width and height as needed


# Read the file
file_path = 'imp_values/queueREAL.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
df['Y'] = (df['Y'] * 50 / 8) / 1000
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.scatter(df.iloc[::50, 0], df.iloc[::50, 1], marker='o', s=5)

# Read the file
file_path = 'imp_values/queuePHSTAB.txt'
df = pd.read_csv(file_path, header=None, usecols=[0,1], names=['X', 'Y'])
df['Y'] = (df['Y'] * 50 / 8) / 1000
df['X'] = (df['X'] / 1000 / 1000)
sns.set(style="whitegrid")
plt.scatter(df.iloc[::50, 0], df.iloc[::50, 1], marker='o', s=5)

plt.axhline(y=1950, color='k', linestyle='--')
plt.axhline(y=130, color='g', linestyle='--')
plt.axhline(y=5200, color='g', linestyle='--')


plt.xlabel('Time (ms)')  # Replace with appropriate label
plt.ylabel('Queue Size (KB)')  # Replace with appropriate label
#plt.xlim(19.5, 20.5)  # Set x-axis limits
#plt.ylim(0, 300)  # Set x-axis limits
#plt.show()
plt.savefig('phantom_stab.pdf', bbox_inches='tight')
