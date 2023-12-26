import re
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import argparse

list_fct = []
list_size = []
num_bts_warning = 0
num_bts_dropped = 0
num_ecn = 0
num_nack = 0
bw_speed_gbps = 0
incast_size = 0
min_bw  = 0
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
x_gain = 0
y_gain = 0
z_gain = 0
w_gain = 0
DisableCase3 = 0
DisableCase4 = 0
bonus_drop_value = 0
buffer_drop_value = 0
overall_completion = []
Precision = 0
ReactionDelay = 0

parser = argparse.ArgumentParser()
parser.add_argument('--input_file', dest='input_file', type=str, help='File to parse.')
parser.add_argument('--folder', dest='folder', type=str, help='Folder to parse and save')
parser.add_argument('--scaling_plot', dest='scaling_plot', type=str, help='Scaling Plot Option', default=None)
parser.add_argument('--parameter_analysis', dest='parameter_analysis', type=str, help='Parameter Analysis Option', default=None)
parser.add_argument('--complex_name', dest='complex_name', type=str, help='Adv Name', default=None)
parser.add_argument('--degree', dest='degree', type=str, help='degree incast', default="0")
parser.add_argument('--precision_reaction', dest='precision_reaction', type=str, help='Parameter Analysis Option', default=None)


args = parser.parse_args()

folder = args.folder
file_name = args.input_file

with open(folder + "/" + file_name) as file:
    for line in file:
        # Name
        if "Name Running: " in line:
            name = line.split(': ')[1]
        
        # DoJitter
        result = re.search(r"DoJitter: (\d+)", line)
        if result:
            DoJitter = int(result.group(1))
        
        # DoExpGain
        result = re.search(r"DoExpGain: (\d+)", line)
        if result:
            DoExpGain = int(result.group(1))

        # FastIncrease
        result = re.search(r"FastIncrease: (\d+)", line)
        if result:
            FastIncrease = int(result.group(1))

        # FastDrop
        result = re.search(r"FastDrop: (\d+)", line)
        if result:
            FastDrop = int(result.group(1))

        # DisableCase3
        result = re.search(r"DisableCase3: (\d+)", line)
        if result:
            DisableCase3 = int(result.group(1))

        # DisableCase4
        result = re.search(r"DisableCase4: (\d+)", line)
        if result:
            DisableCase4 = int(result.group(1))

        # ReactionDelay
        result = re.search(r"ReactionDelay: (\d+)", line)
        if result:
            ReactionDelay = int(result.group(1))

        # Precision
        result = re.search(r"Precision: (\d+)", line)
        if result:
            Precision = int(result.group(1))

        # KMin
        result = re.search(r"KMin: (\d+)", line)
        if result:
            KMin = int(result.group(1))

        # KMax
        result = re.search(r"KMax: (\d+)", line)
        if result:
            KMax = int(result.group(1))

        # GainValueMedIncrease
        result = re.search(r"GainValueMedIncrease: (\d+)", line)
        if result:
            GainValueMedIncrease = re.findall('\d+.\d+', line )[0]

        # JitterValue
        result = re.search(r"JitterValue: (\d+)", line)
        if result:
            JitterValue = re.findall('\d+.\d+', line )[0]

        # DelayGainValue
        result = re.search(r"DelayGainValue: (\d+)", line)
        if result:
            DelayGainValue = re.findall('\d+.\d+', line )[0]

        # TargetRTT
        result = re.search(r"TargetRTT: (\d+)", line)
        if result:
            TargetRTT = int(result.group(1))

        # TargetRTT
        result = re.search(r"Overall Completion at (\d+)", line)
        if result:
            overall_completion.append(int(result.group(1)))

        # BW
        result = re.search(r"Speed is (\d+)", line)
        if result:
            bw_speed_gbps = int(result.group(1)) / 1000

        # FCT
        #result = re.search(r"Host (\d+): (\d+)", line)
        result = re.search(r"Completion Time Flow is (\d+)", line)
        if result:
            fct = int(result.group(1))
            
            list_fct.append(round(fct / 1000))

        # BTS, ECN, NACK
        if "- Warning -" in line:
            num_bts_warning += 1
        if "- Queue is full -" in line:
            num_bts_dropped += 1
        if "Packet is ECN Marked 1 -" in line:
            num_ecn += 1
        if "NACK " in line:
            num_nack += 1

        # Size
        result = re.search(r"of size (\d+) from ", line)
        if result:
            size_m = int(result.group(1))
            list_size.append(round(size_m))
            incast_size = int(result.group(1))

        # X
        result = re.search(r"XGain: (\d+)", line)
        if result:
            x_gain = re.findall('\d+.\d+', line )[0]

        # Y
        result = re.search(r"YGain: (\d+)", line)
        if result:
            y_gain = re.findall('\d+.\d+', line )[0]

        # z
        result = re.search(r"ZGain: (\d+)", line)
        if result:
            z_gain = re.findall('\d+.\d+', line )[0]

        # W
        result = re.search(r"WGain: (\d+)", line)
        if result:
            w_gain = re.findall('\d+.\d+', line )[0]

        # BonusDrop
        result = re.search(r"BonusDrop: (\d+)", line)
        if result:
            bonus_drop_value = re.findall('\d+.\d+', line )[0]

        # BufferDrop
        result = re.search(r"BufferDrop: (\d+)", line)
        if result:
            buffer_drop_value = re.findall('\d+.\d+', line )[0]

    # MIN BW
    max_time = max(list_fct)
    min_bw = (incast_size * 8 + (incast_size*8*0.03)) / (max_time - 8500)


# Use re.search to find the first match
pattern = r'\d+'
matches = re.findall(pattern, args.degree)

extracted_number = 0
# Check if a match was found
if len(matches) >= 2:
# Extract the second matched number
    extracted_number = matches[1]
if (DisableCase3 == 1):
        name = name.rstrip()
        name += " No Case 3"
if (DisableCase4 == 1):
        name = name.rstrip()
        name += " No Case 4"

if (args.precision_reaction is not None and int(args.precision_reaction) >= 1):
    file_name = folder + '/GeneratedReport{}_{}_{}.tmp'.format(name.rstrip(),args.precision_reaction, ReactionDelay)
elif (args.parameter_analysis is not None and int(args.parameter_analysis) == 1):
    file_name = folder + '/GeneratedReport{}.tmp'.format(args.complex_name)
elif (args.scaling_plot is not None and int(args.scaling_plot) == 1):
    file_name = folder + '/GeneratedReport{}_{}.tmp'.format(name.rstrip(), size_m)
elif (args.scaling_plot is not None and int(args.scaling_plot) == 2):
    file_name = folder + '/GeneratedReport{}_{}.tmp'.format(name.rstrip(), extracted_number)
elif (args.scaling_plot is not None and int(args.scaling_plot) == 3):
    file_name = folder + '/GeneratedReport{}_{}_{}.tmp'.format(name.rstrip(), extracted_number, size_m)
else:
    file_name = folder + '/GeneratedReport{}.tmp'.format(name.rstrip())

with open(file_name, 'w') as f:
    # Nam
    f.write('Name: {}\n'.format(name)) 

    # BW
    f.write('Theo BW: {}\n'.format(bw_speed_gbps)) 

    # KMax
    pattern = r'\d+'
    # Use re.search to find the first match
    matches = re.findall(pattern, args.degree)

    # Check if a match was found
    if len(matches) >= 2:
    # Extract the second matched number
        extracted_number = matches[1]
        f.write('IncastDegree: {}\n'.format(extracted_number)) 

    # Size Messages
    b = [0] * (len(list_fct) - len(list_size))
    list_size.extend(b)
    for size_m in list_size:
        f.write('Size: {}\n'.format(size_m)) 

    # FCT
    for fct_ele in list_fct:
        f.write('FCT: {}\n'.format(fct_ele)) 
    
    # BTS, ECN, NACK
    f.write('BTS Warning: {}\n'.format(num_bts_warning)) 
    f.write('BTS Dropped: {}\n'.format(num_bts_dropped)) 
    f.write('ECN: {}\n'.format(num_ecn)) 
    f.write('NACK: {}\n'.format(num_nack)) 

    # Precision
    f.write('Precision: {}\n'.format(Precision)) 

    # ReactionDelay
    f.write('ReactionDelay: {}\n'.format(ReactionDelay)) 

    # Effective BW
    for idx, item in enumerate(list_fct):
        if (list_fct[idx] == 0):
            eff_bw = 0
        else:
            eff_bw = (list_size[idx] * 8 / list_fct[idx])
        f.write('Effective BW: {}\n'.format(eff_bw)) 

    # Incast Size, Scalign Plot
    f.write('Incast Size: {}\n'.format(incast_size)) 

    # Min BW
    f.write('Min BW: {}\n'.format(min_bw))     

    # Max FCT
    f.write('Max FCT: {}\n'.format(max(list_fct)))  

    # Min FCT
    f.write('Min FCT: {}\n'.format(min(list_fct))) 

    # DoJitter
    f.write('DoJitter: {}\n'.format(DoJitter)) 
    
    # DoExpGain
    f.write('DoExpGain: {}\n'.format(DoExpGain)) 

    # DoExpGain
    f.write('OverallCompletion: {}\n'.format(max(overall_completion)))

    # FastIncrease
    f.write('FastIncrease: {}\n'.format(FastIncrease)) 

    # FastDrop
    f.write('FastDrop: {}\n'.format(FastDrop)) 

    # GainValueMedIncrease
    f.write('GainValueMedIncrease: {}\n'.format(GainValueMedIncrease)) 

    # JitterValue
    f.write('JitterValue: {}\n'.format(JitterValue)) 

    # DelayGainValue
    f.write('DelayGainValue: {}\n'.format(DelayGainValue)) 

    # TargetRTT
    f.write('TargetRTT: {}\n'.format(TargetRTT)) 

    # KMin
    f.write('KMin: {}\n'.format(KMin)) 

    # KMax
    f.write('KMax: {}\n'.format(KMax)) 

    # ComplexName
    f.write('ComplexFull: {}\n'.format(args.complex_name)) 

    # X
    f.write('XGain: {}\n'.format(x_gain)) 

    # Y
    f.write('YGain: {}\n'.format(y_gain)) 

    # Z
    f.write('ZGain: {}\n'.format(z_gain)) 

    # W
    f.write('WGain: {}\n'.format(w_gain)) 

    # BonusDrop
    f.write('BonusDrop: {}\n'.format(bonus_drop_value)) 

    # BufferDrop
    f.write('BufferDrop: {}\n'.format(buffer_drop_value)) 