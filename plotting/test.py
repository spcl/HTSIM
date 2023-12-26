import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# Creating the original DataFrame with the specified columns and adding the rows
columns = ["Name", "KMin", "FastDrop", "JitterValue", "CompletionTime"]
data = pd.read_csv(file_path)

df = pd.DataFrame(data, columns=columns)

# Sort the DataFrame by "CompletionTime"
df.sort_values(by="CompletionTime", inplace=True)

# Create a string combination of the other columns for x-axis
df["X_Label"] = df[["Name", "KMin", "JitterValue"]].astype(str).agg(' '.join, axis=1)

# Creating the bar plot manually, alternating colors for FastDrop==0 and FastDrop==1
fig, ax = plt.subplots()

# Get the unique values of X_Label
x_labels_unique = df["X_Label"].unique()

# Calculate the number of bars for each group (FastDrop=0 and FastDrop=1)
num_bars_per_group = df["FastDrop"].nunique()

df.sort_values(by="X_Label", inplace=True)
# Calculate the total width of each group of bars
group_width = 0.8

# Calculate the spacing between each pair of bars
spacing = 0.1

# Calculate the width of each bar within a group
bar_width = (group_width - spacing) / num_bars_per_group
print(bar_width)
print(num_bars_per_group)

# Plot the bars for FastDrop == 0
for idx, x_label in enumerate(x_labels_unique):
    df_subset = df[(df["X_Label"] == x_label) & (df["FastDrop"] == 0)]
    print(df_subset)
    ax.bar([pos + idx * (group_width + spacing) for pos in range(len(df_subset))], df_subset["CompletionTime"], width=bar_width, color="blue", label="FastDrop == 0" if idx == 0 else "")

# Plot the bars for FastDrop == 1 with a small offset
for idx, x_label in enumerate(x_labels_unique):
    df_subset = df[(df["X_Label"] == x_label) & (df["FastDrop"] == 1)]
    print(df_subset)
    ax.bar([pos + idx * (group_width + spacing) + bar_width for pos in range(len(df_subset))], df_subset["CompletionTime"], width=bar_width, color="orange", label="FastDrop == 1" if idx == 0 else "")

plt.xlabel("Rows (Alternating FastDrop=0 and FastDrop=1)")
plt.ylabel("CompletionTime")
plt.title("CompletionTime vs. Alternating Rows from DataFrames")

# Set the x-axis tick positions and labels
my_pos = []
for idx, x_label in enumerate(x_labels_unique):
    my_pos.append([pos + idx * (group_width + spacing) + bar_width for pos in range(len(df_subset))][0] - (bar_width / 2))
print(my_pos)
ax.set_xticks(my_pos)
ax.set_xticklabels(x_labels_unique, rotation=45, ha='right')

# Show the legend
ax.legend()

plt.tight_layout()
plt.show()
