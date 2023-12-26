import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
import re
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import argparse

def extractNum(name):
    # Define the regex pattern to match numbers
    pattern = r'\d+'

    # Use re.findall to find all numbers in the string
    numbers = re.findall(pattern, name)

    # Check if there is at least a second number in the list
    if len(numbers) > 1:
        # Access the second number (index 1) and convert it to an integer
        second_number = int(numbers[1])
        return second_number
    else:
        return name

def extractNumMul(name):
    # Define the regex pattern to match numbers
    pattern = r'\d+'

    # Use re.findall to find all numbers in the string
    numbers = re.findall(pattern, name)

    # Check if there is at least a second number in the list
    if len(numbers) > 1:
        # Access the second number (index 1) and convert it to an integer
        mul_num = int(numbers[0])
        mul_size = int(numbers[2])
        return mul_num + mul_size
    else:
        return name

def getBestTheoretical(name, size):

    actual_link_speed = args.link_speed / args.os
    #print("Name is")
    #print(name)

    if ("incast" in name):
        if ("_4_" in name):
            return (size+size*0.0156)*8*4/actual_link_speed/1000+(args.latency/1000*12)
        elif ("_8_" in name):
            return (size+size*0.0156)*8*8/actual_link_speed/1000+(args.latency/1000*12)
        elif ("_16_" in name):
            return (size+size*0.0156)*8*16/actual_link_speed/1000+(args.latency/1000*12)
        elif ("_32_" in name):
            return (size+size*0.0156)*8*32/actual_link_speed/1000+(args.latency/1000*12)
        elif ("_100_" in name):
            return (size+size*0.0156)*8*100/actual_link_speed/1000+(args.latency/1000*12)
        elif ("_200_" in name):
            return (size+size*0.0156)*8*200/actual_link_speed/1000+(args.latency/1000*12)

    if ("multiple_3" in name):
        return (size+size*0.0156)*8*3/actual_link_speed/1000+(args.latency/1000*12)

    if ("multiple_6" in name):
        return (size+size*0.0156)*8*6/actual_link_speed/1000+(args.latency/1000*12)

    if ("permutation" in name):
        return (size+size*0.0156)*8*1/actual_link_speed/1000+(args.latency/1000*12)



parser = argparse.ArgumentParser()
parser.add_argument('--input_file', dest='input_file', type=str, help='File to parse.')
parser.add_argument('--folder', dest='folder', type=str, help='Folder to parse and save')
parser.add_argument('--name', dest='name', type=str, help='name to save', default=None)
parser.add_argument('--link_speed', dest='link_speed', type=int, help='LinkSpeed', default=None)
parser.add_argument('--latency', dest='latency', type=int, help='Best Possible Time', default=None)
parser.add_argument('--size', dest='size', type=int, help='Best Possible Time', default=None)
parser.add_argument('--os', dest='os', type=int, help='OS Ratio', default=None)



args = parser.parse_args()

# Define the path to the main folder
main_folder_path = Path(args.folder)
list_fct = []
list_fairness = []
list_category = []
list_group = []
size = 0
min_fct = 0
# Iterate through the subfolders inside the main folder
for subfolder in main_folder_path.iterdir():
    if subfolder.is_dir():
        #print(f"Subfolder: {subfolder}")
        
        # Iterate through the contents of each subfolder
        for item in subfolder.iterdir():
            if item.is_file() and "Generated" in str(item):
                #print(f"  File: {item}")
                # Open and read the file line by line
                with open(item, 'r') as file:
                    for line in file:
                        result = re.search(r"Size: (\d+)", line)
                        if result:
                            size = int(result.group(1))
                        result = re.search(r"Min FCT: (\d+)", line)
                        if result:
                            min_fct = int(result.group(1))
                        result = re.search(r"OverallCompletion: (\d+)", line)
                        if result:
                            fct = int(result.group(1))
                            str_subfolder = str(subfolder)
                            str_subfolder = str_subfolder.split('/', 1)[-1]
                            '''result = getBestTheoretical(str_subfolder, size) / (fct / 1000)  * 100
                            if result>100:
                                result = result'''
                            list_fct.append(fct)
                            if ("incast" in str_subfolder):
                                str_subfolder = "Incast_" + str(extractNum(str_subfolder))
                            elif ("multiple" in str_subfolder):
                                str_subfolder = "MultiplePerm_" + str(extractNumMul(str_subfolder))
                            elif ("permutation" in str_subfolder):
                                str_subfolder = "Perm_" + str(extractNum(str_subfolder))
                            elif ("outcast" in str_subfolder):
                                str_subfolder = "Outcast_" + str(extractNum(str_subfolder))
                            elif ("single" in str_subfolder):
                                str_subfolder = "SingleFlow_" + str(extractNum(str_subfolder))
                            list_category.append(str_subfolder)
                        if "Name: " in line:
                            line = line.split('Name: ', 1)[-1]
                            if "NDP" in line:
                                list_group.append("EQDS")
                            elif "Swift" in line:
                                list_group.append("Swift-like")
                            else:
                                list_group.append(str(line.rstrip()))
                    list_fairness.append((fct-min_fct)/min_fct*100)

#print(list_fct)
#print(list_group)
#print(list_category)
data = {
    'Category': list_category,
    'Value': list_fct,
    'Fairness': list_fairness,
    'Group': list_group
}

data_fairness = {
    'Category': list_category,
    'Value': list_fairness,
    'Group': list_group
}

# Create a DataFrame
df = pd.DataFrame(data)
df_fairness = pd.DataFrame(data_fairness)

'''minimum_value = df['Value'].min()
#print(minimum_value)

limit = 80
if (minimum_value > 80):
    limit = 80
elif (minimum_value > 70):
    limit = 70
else:
    limit = minimum_value - 2'''

# Function to deduplicate a list while preserving order
def deduplicate_list(lst):
    seen = set()
    result = []
    for item in lst:
        if item not in seen:
            seen.add(item)
            result.append(item)
    return result


# Create a bar chart with multiple bars grouped by 'Group' within each 'Category'
list_rep = ["512KiB\nk=1", "512KiB\nk=1", "512KiB\nk=1", "1MiB\nk=1", "1MiB\nk=1", "1MiB\nk=1", "1MiB\nk=16", "1MiB\nk=16", "1MiB\nk=16", "512KiB\nk=8", "512KiB\nk=8", "512KiB\nk=8", "512KiB\nk=2", "512KiB\nk=2", "512KiB\nk=2", "512KiB\nk=16", "512KiB\nk=16", "512KiB\nk=16", "1MiB\nk=8", "1MiB\nk=8", "1MiB\nk=8", "1MiB\nk=2", "1MiB\nk=2", "1MiB\nk=2"]
# Step 3: Plot the CDFs using Seaborn
plt.figure(figsize=(10, 6.5))
# Extract the numeric part of the 'Category' column and convert it to integers
def extract_numeric(category):
    match = re.search(r'\d+', category)
    if match:
        return int(match.group())
    return 0  # Return 0 if no numeric part is found

#print(df)
df['Alternative'] = list_rep
df.drop("Category", axis=1, inplace=True)

df['Category_Order'] = df['Alternative'].apply(extract_numeric)
print(df)

df = df.sort_values(by=['Category_Order', 'Group'], ignore_index=True)
#print()
#print(df)

best_performer = df.groupby('Alternative')['Value'].transform('min')
df['Relative_Performance'] = (best_performer / df['Value']) * 100

#print(best_performer)
#print(df)

# Define the custom sorting order
custom_order = deduplicate_list([
    "512KiB\nk=1", "512KiB\nk=1", "512KiB\nk=1", "512KiB\nk=2", "512KiB\nk=2", "512KiB\nk=2",
    "512KiB\nk=8", "512KiB\nk=8", "512KiB\nk=8", "512KiB\nk=16", "512KiB\nk=16", "512KiB\nk=16",
    "1MiB\nk=1", "1MiB\nk=1", "1MiB\nk=1", "1MiB\nk=2", "1MiB\nk=2", "1MiB\nk=2",
    "1MiB\nk=8", "1MiB\nk=8", "1MiB\nk=8", "1MiB\nk=16", "1MiB\nk=16", "1MiB\nk=16"
])
# Create a categorical data type with the custom order
custom_order_dtype = pd.CategoricalDtype(categories=custom_order, ordered=True)
print(custom_order)
# Apply the custom order to the 'Strings' column
df['Alternative'] = df['Alternative'].astype(custom_order_dtype)

# Sort the DataFrame based on the custom order
df = df.sort_values(by='Alternative').reset_index(drop=True)

custom_colors = ["#ff7f0e","#2fa234", "#1f77b4"] * 10 # Customize the colors as needed

ax = sns.barplot(data=df, x='Alternative', y='Relative_Performance', hue='Group', palette=custom_colors)
plt.ylim(65, 110)

ax.set_yticklabels(ax.get_yticks(), size = 15)
_, xlabels = plt.xticks()
ax.set_xticklabels(xlabels, size=15)

ax.set_axisbelow(True)

#for idx, p in enumerate(ax.patches):
    ##print("1Exp {} - Algo {} - Value {}\n".format(str(df_fairness['Category'][idx]), str(df_fairness['Group'][idx]), int(df_fairness['Value'][idx])))
    ##print("2Exp {} - Algo {} - Value {} - Fairness {}\n".format(str(df['Category'][idx]), str(df['Group'][idx]), int(df['Value'][idx]), int(df['Fairness'][idx])))
    #ax.annotate("{}%".format(int(df['Fairness'][idx])), (p.get_x() + p.get_width() / 2., p.get_height() + (p.get_height() * 0.005)), ha='center', va='center', fontsize=14, color='black')


'''#print(df)
for idx, value in enumerate(df['Fairness']):
    #print("1Exp {} - Algo {} - Value {} - Fairness {}\n".format(str(df['Category'][idx]), str(df['Group'][idx]), int(df['Value'][idx]), value))
    
for p, val in zip(ax.patches, df['Fairness']):
    #print("2Exp {} - Algo {} - Value {}\n".format(str(df['Category'][idx]), str(df['Group'][idx]), int(df['Value'][idx])))
    ax.annotate("{}%".format(int(val)), (p.get_x() + p.get_width() / 2., p.get_height()), ha='center', va='center', fontsize=12, color='black')
'''
plt.xlabel('Flow Completion Time (us)',fontsize=17)
plt.ylabel('% of Best Performing within each experiment',fontsize=17)
plt.title('Performance in different all-to-all scenarios 4:1 FT\nLink Speed 800Gbps - 4KiB MTU',fontsize=17)
plt.grid()  #just add this

# Show the plot
plt.tight_layout()
plt.savefig(args.folder + "/scaling.svg", bbox_inches='tight')
plt.savefig(args.folder + "/scaling.png", bbox_inches='tight')
plt.savefig(args.folder + "/scaling.pdf", bbox_inches='tight')
