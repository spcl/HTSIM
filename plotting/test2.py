import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import argparse

# Parser
parser = argparse.ArgumentParser()
parser.add_argument('--folder', dest='folder', type=str, help='Folder to parse and save')
args = parser.parse_args()

# Creating the original DataFrame with the specified columns and adding the rows
df = pd.read_csv(args.folder + "/Summary.csv")

# Add Better Legend
df["X_Label"] = df[["Algo", "KMin", "FastIncrease", "DoJitter",  "DoExpGain", "DelayGain"]].astype(str).agg(' '.join, axis=1)

df["KMin"] = "K:" + df["KMin"].astype(str)
df["FastIncrease"] = "FI:" + df["FastIncrease"].astype(str)
df["DoJitter"] = "J:" + df["DoJitter"].astype(str)
df["DoExpGain"] = "E:" + df["DoExpGain"].astype(str)
df["DelayGain"] = "D:" + df["DelayGain"].astype(str)
#df["FastDrop"] = "Fast:" + df["FastDrop"].astype(str)


lowest_completion_time = df["CompletionTime(us)"].min()
max_completion_time = df["CompletionTime(us)"].max()

# Sort the DataFrame by "CompletionTime(us)"
df.sort_values(by="CompletionTime(us)", inplace=True)

removed_row = df[df["Algo"] == "NDP"].iloc[0]
completion_time_ndp = removed_row["CompletionTime(us)"]

removed_row = df[df["Algo"] == "UEC BTS"].iloc[0]
completion_time_bts = removed_row["CompletionTime(us)"]

removed_row = df[df["Algo"] == "STrack"].iloc[0]
completion_time_strack = removed_row["CompletionTime(us)"]

df = df[df["Algo"] != "NDP"]
df = df[df["Algo"] != "UEC BTS"]
df = df[df["Algo"] != "STrack"]

df["Algo"] = df["Algo"].replace("UEC Version A", "VersA")
df["Algo"] = df["Algo"].replace("UEC Version D", "VersD")

print(len(df.index))
# Create a string combination of the other columns for x-axis
df["X_Label"] = df[["Algo", "KMin", "FastIncrease", "DoJitter",  "DoExpGain", "DelayGain"]].astype(str).agg(' '.join, axis=1)

# Creating the bar plot manually, alternating colors for FastDrop==0 and FastDrop==1
fig, ax = plt.subplots()

# Get the unique values of X_Label
x_labels_unique = df["X_Label"].unique()
print(x_labels_unique)

# Calculate the number of bars for each group (FastDrop=0 and FastDrop=1)
num_bars_per_group = df["FastDrop"].nunique()

df.sort_values(by="X_Label", inplace=True)
# Calculate the total width of each group of bars
group_width = 0.8

# Calculate the spacing between each pair of bars
spacing = 0.1

# Calculate the width of each bar within a group
bar_width = (group_width - spacing) / num_bars_per_group

count = 0

print(len(x_labels_unique))
# Plot the bars for FastDrop == 0
for idx, x_label in enumerate(x_labels_unique):
    df_subset = df[(df["X_Label"] == x_label) & (df["FastDrop"] == 0)]
    ax.bar([pos + idx * (group_width + spacing) for pos in range(len(df_subset))], df_subset["CompletionTime(us)"], width=bar_width, color="blue", label="FastDrop == 0" if idx == 0 else "")
    count += 1
# Plot the bars for FastDrop == 1 with a small offset
for idx, x_label in enumerate(x_labels_unique):
    df_subset = df[(df["X_Label"] == x_label) & (df["FastDrop"] == 1)]
    ax.bar([pos + idx * (group_width + spacing) + bar_width for pos in range(len(df_subset))], df_subset["CompletionTime(us)"], width=bar_width, color="orange", label="FastDrop == 1" if idx == 0 else "")

print(count)
plt.axhline(y=completion_time_ndp, color="red", linestyle="--", label="NDP")
plt.axhline(y=completion_time_bts, color="black", linestyle="--", label="BTS")
plt.axhline(y=completion_time_strack, color="green", linestyle="--", label="STrack")


plt.ylim(lowest_completion_time - (lowest_completion_time * 0.05), max_completion_time + (max_completion_time * 0.05))
plt.xlabel("Configuration (K = KMIn, FI = FastIncrease, J = DoJitter, E = DoExpGain, D = DelayGainValue, FD = FastDrop)")
plt.ylabel("CompletionTime(us)")
plt.title("CompletionTime(us) comparing Feature")

# Set the x-axis tick positions and labels
my_pos = []
for idx, x_label in enumerate(x_labels_unique):
    my_pos.append([pos + idx * (group_width + spacing) + bar_width for pos in range(len(df_subset))][0] - (bar_width / 2))
ax.set_xticks(my_pos)
ax.set_xticklabels(x_labels_unique, rotation=90, ha='right')

# Show the legend
ax.legend()

plt.tight_layout()
plt.show()
