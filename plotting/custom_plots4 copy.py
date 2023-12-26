import seaborn as sns
import matplotlib.pyplot as plt

# Your data
data_list_1 = [
    354.30,
    358.40,
    362.50,
]

data_list_2 = [
    10332,
    12970,
    27712,
]

data_list_3 = [
    354.30,
    358.40,
    362.50,
]

data_list_4 = [
    618,
    8833,
    1367,
]


data = {
    'REPS_bin': data_list_1,
    'REPS_value': data_list_2,

}

data2 = {
    'Oblivious_Packet_Spray_bin': data_list_3,
    'Oblivious_Packet_Spray_value': data_list_4
}

# Create a DataFrame
import pandas as pd
df = pd.DataFrame(data)
df2 = pd.DataFrame(data2)
# Set up the matplotlib figure
plt.figure(figsize=(10, 6))

# Plot the data
sns.lineplot(x='REPS_bin', y='REPS_value', data=df, label='REPS')
sns.lineplot(x='Oblivious_Packet_Spray_bin', y='Oblivious_Packet_Spray_value', data=df2, label='Oblivious Packet Spray')

# Set plot labels and title
plt.xlabel('Flow Completion Time (us)')
plt.ylabel('# Messages')
plt.title('Impact of REPS on FCT distribution in\noversubscribed asymmetric network')
plt.ylim(0, 600000)
# Display the legend
plt.legend()

# Show the plot
plt.show()
