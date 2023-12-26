import re
import numpy as np
import pandas as pd
import seaborn as sns
from pathlib import Path
import matplotlib.pyplot as plt
import argparse

# Parser
parser = argparse.ArgumentParser()
parser.add_argument('--folder', dest='folder', type=str, help='Folder to parse and save')
args = parser.parse_args()

# Paramters
folder = args.folder
name = ""
completion_time = 0
min_fct = 0
ComplexFull = ""
max_fct = 0
name = ""
DoJitter = 0
DoExpGain = 0
FastIncrease = 0
FastDrop = 0
GainValueMedIncrease = 0
JitterValue = 0
DelayGainValue = 0
TargetRTT = 0
KMin = 0
KMax = 0
num_trimmed = 0
data = pd.DataFrame(columns=["Algo", "KMin", "KMax", "TargetRTT", "FastDrop", "FastIncrease", "DoJitter",  "DoExpGain", "DelayGain", "CompletionTime(us)", "MinFCT(us)", "MaxFCT(us)", "MinvsMaxFCT%", "NumTrimmed", "Score (Equally Importance to Fairness and RunTime)", "FullName"])
i = 0

# Go Through Results files
pathlist = Path(folder).glob('**/*.tmp')
for files in sorted(pathlist):
    # Skip Any Possilbe Noise
    if ("GeneratedReport" not in str(files)):
        continue
    with open(files) as file:
        for line in file:
            # Max FCT 
            if "Max FCT: " in line:
                completion_time = re.findall('\d+', line )[0]
            # Min FCT 
            if "Min FCT: " in line:
                min_fct = re.findall('\d+', line )[0]
            # NACK 
            if "NACK: " in line:
                num_trimmed = re.findall('\d+', line )[0]

            # Name
            if "Name: " in line:
                name = line.split(': ')[1]

            # Name
            if "ComplexFull: " in line:
                ComplexFull = line.split(': ')[1].rstrip()
            
            # DoJitter
            result = re.search(r"DoJitter: (\d+)", line)
            if result:
                DoJitter = int(result.group(1))
            
            # DoExpGain
            result = re.search(r"DoExpGain: (\d+)", line)
            if result:
                DoExpGain = int(result.group(1))

            # kMin
            result = re.search(r"KMin: (\d+)", line)
            if result:
                KMin = int(result.group(1))

            # KMax
            result = re.search(r"KMax: (\d+)", line)
            if result:
                KMax = int(result.group(1))

            # FastIncrease
            result = re.search(r"FastIncrease: (\d+)", line)
            if result:
                FastIncrease = int(result.group(1))

            # FastDrop
            result = re.search(r"FastDrop: (\d+)", line)
            if result:
                FastDrop = int(result.group(1))

            # GainValueMedIncrease
            result = re.search(r"GainValueMedIncrease: (\d+.\d+)", line)
            if result:
                GainValueMedIncrease = re.findall('\d+.\d+', line )[0]

            # JitterValue
            result = re.search(r"JitterValue: (\d+.\d+)", line)
            if result:
                JitterValue = re.findall('\d+.\d+', line )[0]

            # DelayGainValue
            result = re.search(r"DelayGainValue: (\d+.\d+)", line)
            if result:
                DelayGainValue = re.findall('\d+.\d+', line )[0]

            # TargetRTT
            result = re.search(r"TargetRTT: (\d+)", line)
            if result:
                TargetRTT = int(result.group(1))

        # Values for the new row
        min_vs_max_fct = (int(completion_time) - int(min_fct)) / int(min_fct) * 100
        new_row = [str(name.rstrip()), KMin, KMax, TargetRTT, FastDrop, FastIncrease, DoJitter, DoExpGain, DelayGainValue, round(int(completion_time) / 1000), round(int(min_fct) / 1000), round(int(completion_time) / 1000), min_vs_max_fct, num_trimmed, 0, ComplexFull]
        # Append the new row to the DataFrame
        new_row_df = pd.DataFrame([new_row], columns=data.columns)
        data = pd.concat([data, new_row_df], ignore_index=True)


lowest_completion_time = data["CompletionTime(us)"].min()
for index, row in data.iterrows():
    my_completion_time = row["CompletionTime(us)"]
    data.at[index, "Score (Equally Importance to Fairness and RunTime)"] = (((float(lowest_completion_time) / float(my_completion_time) * 100)) / 2) + (50 - (50 * float(row["MinvsMaxFCT%"]) / 100))

# Sort the DataFrame by "CompletionTime(us)"
data.sort_values(by="CompletionTime(us)", inplace=True, ascending=True)
data.to_csv(args.folder + "/Summary.csv", index=False)

html = data.to_html()
title = "<center>Incast 32:1 - 128KiB (Med Message) - 100Gb/s - 300ns Link Latency - Ordered by CompletionTime(us)</center>"
title = """
    <html>
    <head>
    <style>
    thead {color: green;}
    tbody {color: black;}
    tfoot {color: red;}

    table, th, td {
      border: 1px solid black;
    }
    </style>
    </head>
    <body>

    <h4>
    """ + title + "</h4>"

end_html = """
        </body>
        </html>
        """

html = title + html + end_html
text_file = open(args.folder + "/Summary.html", "w")
text_file.write(html)
text_file.close()