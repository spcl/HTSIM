import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import matplotlib as mpl
from matplotlib import rcParams
import argparse


# Parser
my_exp_list_small = ["Parameter_Analysis_Incast_8_64_100G", 
"Parameter_Analysis_Incast_32_64_100G", 
"Parameter_Analysis_Incast_64_64_100G",
"Parameter_Analysis_Incast_8_64_400G", 
"Parameter_Analysis_Incast_32_64_400G", 
"Parameter_Analysis_Incast_64_64_400G",
"Parameter_Analysis_Incast_8_64_800G", 
"Parameter_Analysis_Incast_32_64_800G", 
"Parameter_Analysis_Incast_64_64_800G"
]

my_exp_list_large = [
"Parameter_Analysis_Incast_8_512_100G", 
"Parameter_Analysis_Incast_8_2000_100G",
"Parameter_Analysis_Incast_32_512_100G",
"Parameter_Analysis_Incast_32_2000_100G", 
"Parameter_Analysis_Incast_64_512_100G", 
"Parameter_Analysis_Incast_64_2000_100G",
"Parameter_Analysis_Incast_8_512_400G", 
"Parameter_Analysis_Incast_8_2000_400G",
"Parameter_Analysis_Incast_32_512_400G",
"Parameter_Analysis_Incast_32_2000_400G", 
"Parameter_Analysis_Incast_64_512_400G",
"Parameter_Analysis_Incast_64_2000_400G",
"Parameter_Analysis_Incast_8_512_800G", 
"Parameter_Analysis_Incast_8_2000_800G",
"Parameter_Analysis_Incast_32_512_800G",
"Parameter_Analysis_Incast_32_2000_800G"]

using_list = my_exp_list_small + my_exp_list_large
possible_value_3 = None
threshold_filter = 0.06
file_name_filtered = "filtered_raw.txt"
file_name_filtered_no_dup = "filtered_no_duplicates.txt"
open(file_name_filtered, 'w').close()

for element in using_list:

    #if ("100G" in str(element)):
    #    continue
    # Creating the original DataFrame with the specified columns and adding the rows
    df = pd.read_csv(element + "/Summary.csv")
    #df = df[~((df["KMin"] == 20) & (df["DelayGain"] == 2))]
    #df = df[~((df["KMin"] == 50) & (df["DelayGain"] == 5))]
    # Input
    filtering_on = "FastDrop"
    possible_value_0 = 0
    possible_value_1 = 1


    # Columns
    all_options = ["Algo", "KMin", "FastDrop", "FastIncrease", "DoJitter",  "DoExpGain", "DelayGain"]
    all_options.remove(filtering_on)

    # Remove Other Algos
    removed_row = df[df["Algo"] == "NDP"].iloc[0]
    completion_time_ndp = removed_row["CompletionTime(us)"]

    lowest_completion_time = df["CompletionTime(us)"].min()
    max_completion_time = df["CompletionTime(us)"].max()

    upper_limit = completion_time_ndp + completion_time_ndp * threshold_filter
    for index, row in df.iterrows():
        if int(df["CompletionTime(us)"][index]) > upper_limit:
            #print(row.loc["CompletionTime(us)"])
            my_str = "df = df[~((df['KMin'] == {}) & (df['FastDrop'] == {})  & (df['FastIncrease'] == {})  & (df['Algo'] == \'{}\') & (df['DoJitter'] == {})  & (df['DoExpGain'] == {})  & (df['DelayGain'] == {}))]\n".format(row.loc["KMin"], row.loc["FastDrop"], row.loc["FastIncrease"], row.loc["Algo"], row.loc["DoJitter"], row.loc["DoExpGain"], row.loc["DelayGain"])
            with open(file_name_filtered, "a") as myfile:
                myfile.write(my_str)


lines_seen = set() # holds lines already seen
outfile = open(file_name_filtered_no_dup, "w")
for line in open(file_name_filtered, "r"):
    if line not in lines_seen:
        outfile.write(line)
        lines_seen.add(line)
outfile.close()