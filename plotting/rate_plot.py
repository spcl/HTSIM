import plotly.express as px
import pandas as pd
from pathlib import Path
import plotly.graph_objs as go
import plotly
from plotly.subplots import make_subplots
from datetime import datetime
import os
import re
import natsort 
from argparse import ArgumentParser


# Parameters
skip_small_value = True
ECN = True

def main(args):
    # Clean Data and Copy Data
    os.system("rm -r queue_size_normalized/")
    os.system("rm -r queue_phantom/")
    os.system("rm -r rtt/")
    os.system("rm -r cwd/")
    os.system("rm -r ecn/")
    os.system("rm -r sent/")
    os.system("rm -r nack/")
    os.system("rm -r acked/")
    os.system("rm -r fasti/")
    os.system("rm -r fastd/")
    os.system("rm -r mediumi/")
    os.system("rm -r trimmed_rtt/")
    os.system("rm -r ecn_rtt/")
    os.system("rm -r case1/")
    os.system("rm -r case2/")
    os.system("rm -r case3/")
    os.system("rm -r case4/")
    os.system("rm -r ecn_rate/")
    os.system("rm -r sending_rate/")

    os.system("cp -a ../sim/output/cwd/. cwd/")
    os.system("cp -a ../sim/output/rtt/. rtt/")
    os.system("cp -a ../sim/output/queue/. queue_size_normalized/")
    os.system("cp -a ../sim/output/queue_phantom/. queue_phantom/")
    os.system("cp -a ../sim/output/sent/. sent/")
    os.system("cp -a ../sim/output/ecn/. ecn/")
    os.system("cp -a ../sim/output/nack/. nack/")
    os.system("cp -a ../sim/output/fasti/. fasti/")
    os.system("cp -a ../sim/output/fastd/. fastd/")
    os.system("cp -a ../sim/output/mediumi/. mediumi/")
    os.system("cp -a ../sim/output/acked/. acked/")
    os.system("cp -a ../sim/output/trimmed_rtt/. trimmed_rtt/")
    os.system("cp -a ../sim/output/ecn_rtt/. ecn_rtt/")
    os.system("cp -a ../sim/output/case1/. case1/")
    os.system("cp -a ../sim/output/case2/. case2/")
    os.system("cp -a ../sim/output/case3/. case3/")
    os.system("cp -a ../sim/output/case4/. case4/")
    os.system("cp -a ../sim/output/ecn_rate/. ecn_rate/")
    os.system("cp -a ../sim/output/sending_rate/. sending_rate/")

    # RTT Data
    colnames=['Time', 'RTT', 'seqno', 'ackno', 'base', 'target']
    df = pd.DataFrame(columns =['Time','RTT', 'seqno', 'ackno', 'base', 'target'])
    name = ['0'] * df.shape[0]
    df = df.assign(Node=name)

    pathlist = Path('rtt').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df.shape[0]
        temp_df = temp_df.assign(Node=name)
        df = pd.concat([df, temp_df])

    base_rtt = df["base"].max()
    target_rtt = df["target"].max()

    if (len(df) > 100000):
        ratio = len(df) / 50000
        # DownScale
        df = df.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df.reset_index(drop=True, inplace=True)

    # Cwd Data
    colnames=['Time', 'Congestion Window'] 
    df2 = pd.DataFrame(columns =colnames)
    name = ['0'] * df2.shape[0]
    df2 = df2.assign(Node=name)
    df2.drop_duplicates('Time', inplace = True)

    pathlist = Path('cwd').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df2 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df2.shape[0]
        temp_df2 = temp_df2.assign(Node=name)
        temp_df2.drop_duplicates('Time', inplace = True)
        df2 = pd.concat([df2, temp_df2])
    if (len(df2) > 100000):
        ratio = len(df2) / 50000
        # DownScale
        df2 = df2.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df2.reset_index(drop=True, inplace=True)
    
    # Queue Phantom Data
    colnames=['Time', 'Queue', 'KMin', 'KMax'] 
    df30= pd.DataFrame(columns =colnames)
    name = ['0'] * df30.shape[0]
    df30 = df30.assign(Node=name)
    df30.drop_duplicates('Time', inplace = True)

    pathlist = Path('queue_phantom').glob('**/*.txt')
    for files in natsort.natsorted(pathlist,reverse=False):
        path_in_str = str(files)
        temp_df30 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df30.shape[0]
        temp_df30 = temp_df30.assign(Node=name)
        temp_df30.drop_duplicates('Time', inplace = True)
        df30 = pd.concat([df30, temp_df30])

    kmin = df30["KMin"].max()
    kmax = df30["KMax"].max()

    if (len(df30) > 100000):
        ratio = len(df30) / 50000
        # DownScale
        df30 = df30.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df30.reset_index(drop=True, inplace=True)

    # Queue Data
    colnames=['Time', 'Queue', 'KMin', 'KMax', 'MaxQueueSize'] 
    df3= pd.DataFrame(columns =colnames)
    name = ['0'] * df3.shape[0]
    df3 = df3.assign(Node=name)
    df3.drop_duplicates('Time', inplace = True)

    pathlist = Path('queue_size_normalized').glob('**/*.txt')
    for files in natsort.natsorted(pathlist,reverse=False):
        '''pattern = r'^queue_size_normalized/queueUS_\d+-CS_\d+\.txt$'
        if not re.match(pattern, str(files)):
            continue'''

        path_in_str = str(files)
        temp_df3 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df3.shape[0]
        temp_df3 = temp_df3.assign(Node=name)
        temp_df3.drop_duplicates('Time', inplace = True)
        temp_df3['Queue'] = temp_df3['Queue']/temp_df3['MaxQueueSize']*100

        df3 = pd.concat([df3, temp_df3])

    kmin = df3["KMin"].max()
    kmax = df3["KMax"].max()

    if (len(df3) > 100000):
        ratio = len(df3) / 50000
        # DownScale
        df3 = df3.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df3.reset_index(drop=True, inplace=True)

    # Sending Rate Data
    colnames=['Time', 'SendingRate'] 
    df_sending= pd.DataFrame(columns =colnames)
    name = ['0'] * df_sending.shape[0]
    df_sending = df_sending.assign(Node=name)
    df_sending.drop_duplicates('Time', inplace = True)

    pathlist = Path('sending_rate').glob('**/*.txt')
    for files in natsort.natsorted(pathlist,reverse=False):
        path_in_str = str(files)
        temp_df_sending = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df_sending.shape[0]
        temp_df_sending = temp_df_sending.assign(Node=name)
        temp_df_sending.drop_duplicates('Time', inplace = True)
        temp_df_sending['SendingRate'] = temp_df_sending['SendingRate']*2
        df_sending = pd.concat([df_sending, temp_df_sending])

    if (len(df_sending) > 100000):
        ratio = len(df_sending) / 50000
        # DownScale
        df_sending = df_sending.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df_sending.reset_index(drop=True, inplace=True)

     # Queue Data
    colnames=['Time', 'ECN_Rate'] 
    df_ecn_rate= pd.DataFrame(columns =colnames)
    name = ['0'] * df_ecn_rate.shape[0]
    df_ecn_rate = df_ecn_rate.assign(Node=name)
    df_ecn_rate.drop_duplicates('Time', inplace = True)

    pathlist = Path('ecn_rate').glob('**/*.txt')
    for files in natsort.natsorted(pathlist,reverse=False):

        path_in_str = str(files)
        temp_df_ecn_rate = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df_ecn_rate.shape[0]
        temp_df_ecn_rate = temp_df_ecn_rate.assign(Node=name)
        temp_df_ecn_rate.drop_duplicates('Time', inplace = True)
        df_ecn_rate = pd.concat([df_ecn_rate, temp_df_ecn_rate])

    if (len(df_ecn_rate) > 100000):
        ratio = len(df_ecn_rate) / 50000
        # DownScale
        df_ecn_rate = df_ecn_rate.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df_ecn_rate.reset_index(drop=True, inplace=True)

    # ECN Data
    colnames=['Time', 'ECN'] 
    df4 = pd.DataFrame(columns =colnames)
    name = ['0'] * df4.shape[0]
    df4 = df4.assign(Node=name)

    pathlist = Path('ecn').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df4 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df4.shape[0]
        temp_df4 = temp_df4.assign(Node=name)
        df4 = pd.concat([df4, temp_df4])
    if (len(df4) > 100000):
        ratio = len(df) / 50000
        
        # DownScale
        df4 = df4.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df4.reset_index(drop=True, inplace=True)

    if (args.show_case):
        # Case1 Data
        colnames=['Time', 'Case1'] 
        df30= pd.DataFrame(columns =colnames)
        name = ['0'] * df30.shape[0]
        df30 = df30.assign(Node=name)
        df30.drop_duplicates('Time', inplace = True)

        pathlist = Path('case1').glob('**/*.txt')
        for files in natsort.natsorted(pathlist,reverse=False):
            path_in_str = str(files)
            temp_df30 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
            name = [str(path_in_str)] * temp_df30.shape[0]
            temp_df30 = temp_df30.assign(Node=name)
            temp_df30.drop_duplicates('Time', inplace = True)
            df30 = pd.concat([df30, temp_df30])

        df30 = df30.sort_values('Time')
        # Define the sampling_time variable
        sampling_time = base_rtt
        # Initialize new lists to store the sampled times and aggregated values
        sampled_times = []
        aggregated_values = []
        current_time = None
        current_value_sum = 0

        max_time_while = df30["Time"].max()
        df30["Time"] = df30["Time"].astype(int)
        saved_old_sum = 0

        # Iterate through the rows
        for curr_ti in range(base_rtt, max_time_while + base_rtt, base_rtt):
            tmp_sum = 0
            sub_df = df30.query("{} <= Time <= {}".format( curr_ti - base_rtt, curr_ti))
            for index, row in sub_df.iterrows():
                time = row['Time']
                value = row['Case1']
                tmp_sum += value

            sampled_times.append(curr_ti)
            if (args.cumulative_case):
                aggregated_values.append(tmp_sum + saved_old_sum)
                saved_old_sum = tmp_sum + saved_old_sum
            else:
                aggregated_values.append(tmp_sum)

        # Create a new DataFrame from the sampled data
        df30 = pd.DataFrame({'Time': sampled_times, 'Case1': aggregated_values})

        print("Case2")
        # Case2 Data
        colnames=['Time', 'Case2'] 
        df31= pd.DataFrame(columns =colnames)
        name = ['0'] * df31.shape[0]
        df31 = df31.assign(Node=name)
        df31.drop_duplicates('Time', inplace = True)

        pathlist = Path('case2').glob('**/*.txt')
        for files in natsort.natsorted(pathlist,reverse=False):
            path_in_str = str(files)
            temp_df31 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
            name = [str(path_in_str)] * temp_df31.shape[0]
            temp_df31 = temp_df31.assign(Node=name)
            temp_df31.drop_duplicates('Time', inplace = True)
            df31 = pd.concat([df31, temp_df31])

        df31 = df31.sort_values('Time')
        # Define the sampling_time variable
        sampling_time = base_rtt
        # Initialize new lists to store the sampled times and aggregated values
        sampled_times = []
        aggregated_values = []
        current_time = None
        current_value_sum = 0
        # Iterate through the rows
        max_time_while = df31["Time"].max()
        df31["Time"] = df31["Time"].astype(int)
        saved_old_sum = 0
        # Iterate through the rows
        for curr_ti in range(base_rtt, max_time_while + base_rtt, base_rtt):
            tmp_sum = 0
            sub_df = df31.query("{} <= Time <= {}".format( curr_ti - base_rtt, curr_ti))
            for index, row in sub_df.iterrows():
                time = row['Time']
                value = row['Case2']
                tmp_sum += value
            sampled_times.append(curr_ti)
            if (args.cumulative_case):
                aggregated_values.append(tmp_sum + saved_old_sum)
                saved_old_sum = tmp_sum + saved_old_sum
            else:
                aggregated_values.append(tmp_sum)
        # Create a new DataFrame from the sampled data

        df31 = pd.DataFrame({'Time': sampled_times, 'Case2': aggregated_values})

        print("Case3")
        # Case3 Data
        colnames=['Time', 'Case3'] 
        df32= pd.DataFrame(columns =colnames)
        name = ['0'] * df32.shape[0]
        df32 = df32.assign(Node=name)
        df32.drop_duplicates('Time', inplace = True)
        saved_old_sum = 0
        pathlist = Path('case3').glob('**/*.txt')
        for files in natsort.natsorted(pathlist,reverse=False):
            path_in_str = str(files)
            temp_df32 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
            name = [str(path_in_str)] * temp_df32.shape[0]
            temp_df32 = temp_df32.assign(Node=name)
            temp_df32.drop_duplicates('Time', inplace = True)
            df32 = pd.concat([df32, temp_df32])

        df32 = df32.sort_values('Time')
        # Define the sampling_time variable
        sampling_time = base_rtt
        # Initialize new lists to store the sampled times and aggregated values
        sampled_times = []
        aggregated_values = []
        current_time = None
        current_value_sum = 0
        # Iterate through the rows
        max_time_while = df32["Time"].max()
        df32["Time"] = df32["Time"].astype(int)
        # Iterate through the rows
        if (len(df32) > 0):
            for curr_ti in range(base_rtt, max_time_while + base_rtt, base_rtt):
                tmp_sum = 0

                sub_df = df32.query("{} <= Time <= {}".format( curr_ti - base_rtt, curr_ti))
                for index, row in sub_df.iterrows():
                    time = row['Time']
                    value = row['Case3']
                    tmp_sum += value
                sampled_times.append(curr_ti)
                if (args.cumulative_case):
                    aggregated_values.append(tmp_sum + saved_old_sum)
                    saved_old_sum = tmp_sum + saved_old_sum
                else:
                    aggregated_values.append(tmp_sum)
            # Create a new DataFrame from the sampled data
            df32 = pd.DataFrame({'Time': sampled_times, 'Case3': aggregated_values})

        print("Case4")
        # Case4 Data
        saved_old_sum = 0
        colnames=['Time', 'Case4'] 
        df33= pd.DataFrame(columns =colnames)
        name = ['0'] * df33.shape[0]
        df33 = df33.assign(Node=name)
        df33.drop_duplicates('Time', inplace = True)

        pathlist = Path('case4').glob('**/*.txt')
        for files in natsort.natsorted(pathlist,reverse=False):
            path_in_str = str(files)
            temp_df33 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
            name = [str(path_in_str)] * temp_df33.shape[0]
            temp_df33 = temp_df33.assign(Node=name)
            temp_df33.drop_duplicates('Time', inplace = True)
            df33 = pd.concat([df33, temp_df33])

        df33 = df33.sort_values('Time')
        # Define the sampling_time variable
        sampling_time = base_rtt
        # Initialize new lists to store the sampled times and aggregated values
        sampled_times = []
        aggregated_values = []
        current_time = None
        current_value_sum = 0
        # Iterate through the rows
        max_time_while = df33["Time"].max()
        df33["Time"] = df33["Time"].astype(int)
        # Iterate through the rows
        if (len(df33) > 0):
            for curr_ti in range(base_rtt, max_time_while + base_rtt, base_rtt):
                tmp_sum = 0

                sub_df = df33.query("{} <= Time <= {}".format( curr_ti - base_rtt, curr_ti))
                for index, row in sub_df.iterrows():
                    time = row['Time']
                    value = row['Case4']
                    tmp_sum += value
                sampled_times.append(curr_ti)
                if (args.cumulative_case):
                    aggregated_values.append(tmp_sum + saved_old_sum)
                    saved_old_sum = tmp_sum + saved_old_sum
                else:
                    aggregated_values.append(tmp_sum)
            # Create a new DataFrame from the sampled data
            df33 = pd.DataFrame({'Time': sampled_times, 'Case4': aggregated_values})

    # Nack data
    colnames=['Time', 'Nack'] 
    df6 = pd.DataFrame(columns =colnames)
    name = ['0'] * df6.shape[0]
    df6 = df6.assign(Node=name)
    

    pathlist = Path('nack').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df6 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df6.shape[0]
        temp_df6 = temp_df6.assign(Node=name)
        df6 = pd.concat([df6, temp_df6])


    # Acked Bytes Data
    colnames=['Time', 'AckedBytes'] 
    df8 = pd.DataFrame(columns =colnames)
    name = ['0'] * df8.shape[0]
    df8 = df8.assign(Node=name)
    df8.drop_duplicates('Time', inplace = True)

    pathlist = Path('acked').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df8 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df8.shape[0]
        temp_df8 = temp_df8.assign(Node=name)
        temp_df8.drop_duplicates('Time', inplace = True)
        df8 = pd.concat([df8, temp_df8])

    # ECN in RTT Data
    colnames=['Time', 'ECNRTT'] 
    df13 = pd.DataFrame(columns =colnames)
    name = ['0'] * df13.shape[0]
    df13 = df13.assign(Node=name)
    df13.drop_duplicates('Time', inplace = True)

    pathlist = Path('ecn_rtt').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df13 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df13.shape[0]
        temp_df13 = temp_df13.assign(Node=name)
        temp_df13.drop_duplicates('Time', inplace = True)
        df13 = pd.concat([df13, temp_df13])

    # Trimming in RTT Data
    colnames=['Time', 'TrimmedRTT'] 
    df14 = pd.DataFrame(columns =colnames)
    name = ['0'] * df14.shape[0]
    df14 = df14.assign(Node=name)
    df14.drop_duplicates('Time', inplace = True)

    pathlist = Path('trimmed_rtt').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df14 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df14.shape[0]
        temp_df14 = temp_df14.assign(Node=name)
        temp_df14.drop_duplicates('Time', inplace = True)
        df14 = pd.concat([df14, temp_df14])


    # FastI data
    colnames=['Time', 'FastI'] 
    df9 = pd.DataFrame(columns =colnames)
    name = ['0'] * df9.shape[0]
    df9 = df9.assign(Node=name)
    

    pathlist = Path('fasti').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df9 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df9.shape[0]
        temp_df9 = temp_df9.assign(Node=name)
        df9 = pd.concat([df9, temp_df9])

    # FastD data
    colnames=['Time', 'FastD'] 
    df10 = pd.DataFrame(columns =colnames)
    name = ['0'] * df10.shape[0]
    df10 = df10.assign(Node=name)
    

    pathlist = Path('fastd').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df10 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df10.shape[0]
        temp_df10 = temp_df10.assign(Node=name)
        df10 = pd.concat([df10, temp_df10])

    # MediumI data
    colnames=['Time', 'MediumI'] 
    df11 = pd.DataFrame(columns =colnames)
    name = ['0'] * df11.shape[0]
    df11 = df11.assign(Node=name)
    

    pathlist = Path('mediumi').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df11 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df11.shape[0]
        temp_df11 = temp_df11.assign(Node=name)
        df11 = pd.concat([df11, temp_df11])
    if (len(df11) > 100000):
        ratio = len(df) / 50000
        # DownScale
        df11 = df11.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df11.reset_index(drop=True, inplace=True)

    print("Finished Parsing")
    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])
    color = ['#636EFA', '#0511a9', '#EF553B', '#00CC96', '#AB63FA', '#FFA15A', '#19D3F3', '#FF6692', '#B6E880', '#FF97FF', '#FECB52']
    # Add traces
    mean_rtt = df["RTT"].mean()
    max_rtt = df["RTT"].max()
    max_x = df["Time"].max()
    y_sent = max_rtt * 0.9
    y_ecn = max_rtt * 0.85
    y_nack =max_rtt * 0.80
    y_fasti =max_rtt * 0.75
    y_fastd =max_rtt * 0.70
    y_mediumi =max_rtt * 0.65
    mean_rtt = 10000
    count = 0

    # Sending Rate
    print("Sending Rate Plot")
    count = 0
    df_sending['SendingRate'] = pd.to_numeric(df_sending['SendingRate'])
    max_ele = df_sending[['SendingRate']].idxmax(1)
    for i in df_sending['Node'].unique():
        sub_df = df_sending.loc[df_sending['Node'] == str(i)]
        if (skip_small_value is True and sub_df['SendingRate'].max() < 1):
            count += 1
            continue

        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['SendingRate'], name="SendingRate " + str(i),   marker=dict(size=1.4), line=dict(dash='solid', width=3),  showlegend=True),
            secondary_y=False,
        )
        count += 1

    if args.name is not None:
        my_title=args.name
    else:
        my_title="<b>Incast 2:1 – 1:1 FT – 800Gbps – 4KiB MTU – 128MiB Flows - LoadBalancing ON</b>"
        my_title="Overall Timeline - Parallel Phantom - Gentle Decrease when Rate Increasing"

    # Add figure title
    fig.update_layout(title_text=my_title)

    fig.update_layout(yaxis_range=[0,100])

    if (args.x_limit is not None):
        fig.update_layout(xaxis_range=[0,args.x_limit])

    config = {
    'toImageButtonOptions': {
        'format': 'png', # one of png, svg, jpeg, webp
        'filename': 'custom_image',
        'height': 550,
        'width': 1000,
        'scale':4 # Multiply title/legend/axis/canvas sizes by this factor
    }
    }

    # Set x-axis title
    fig.update_xaxes(title_text="Time (ns)")
    # Set y-axes titles
    fig.update_yaxes(title_text="Sending Throughput (Gb/s)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)
    
    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    print("Saving Plot")
    #fig.write_image("out/fid_simple_{}.png".format(date_time))
    #plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    print("Showing Plot")
    if (args.output_folder is not None):
        plotly.offline.plot(fig, filename=args.output_folder + "/{}.html".format(args.name))
    if (args.no_show is None):
        fig.show()

if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--x_limit", type=int, help="Max X Value showed", default=None)
    parser.add_argument("--y_limit", type=int, help="Max Y value showed", default=None)
    parser.add_argument("--show_ecn", type=str, help="Show ECN points", default=None)
    parser.add_argument("--show_sent", type=str, help="Show Sent Points", default=None) 
    parser.add_argument("--show_triangles", type=str, help="Show RTT triangles", default=None) 
    parser.add_argument("--num_to_show", type=int, help="Number of lines to show", default=None) 
    parser.add_argument("--annotations", type=str, help="Number of lines to show", default=None) 
    parser.add_argument("--output_folder", type=str, help="OutFold", default=None) 
    parser.add_argument("--input_file", type=str, help="InFold", default=None) 
    parser.add_argument("--name", type=str, help="Name Algo", default=None) 
    parser.add_argument("--no_show", type=int, help="Don't show plot, just save", default=None) 
    parser.add_argument("--show_case", type=int, help="ShowCases", default=None) 
    parser.add_argument("--cumulative_case", type=int, help="Do it cumulative", default=None) 
    args = parser.parse_args()
    main(args)
