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

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
EXP_FOLDER = WORKING_DIR + "/../" + "sim/output/"

# Parameters
skip_small_value = True
ECN = True

def get_df(path, colnames, max_values=None):
    df = pd.DataFrame(columns=colnames)
    name = ['0'] * df.shape[0]
    df = df.assign(Node=name)

    pathlist = Path(EXP_FOLDER + path).absolute().glob('**/*.txt')
    for file in sorted(pathlist):
        path_in_str = str(file)
        temp_df = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        file_name = str(os.path.basename(os.path.normpath(path_in_str)))
        name = [file_name] * temp_df.shape[0]
        temp_df = temp_df.assign(Node=name)
        df = pd.concat([df, temp_df])

    df.drop_duplicates('Time', inplace = True)

    if max_values and (len(df) > max_values):
        ratio = len(df) / 20000
        # DownScale
        df = df.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df.reset_index(drop=True, inplace=True)
    return df

def plot_line(fig, df, x_col, y_col, name, color=None, secondary_y=False):
    color = color if color else None
    fig.add_trace(
        go.Scatter(x=df[x_col], y=df[y_col], mode='markers', marker=dict(size=2), name=name, line=dict(color=color), opacity=0.9, showlegend=True),
        secondary_y=secondary_y,
    )

def main(args):
    df = get_df('rtt', ['Time', 'RTT', 'seqno', 'ackno', 'base', 'target'], max_values=100000)
    base_rtt = df["base"].max()
    target_rtt = df["target"].max()

    # Cwd data.
    df2 = get_df('cwd', ['Time', 'Congestion Window'], max_values=100000)
    df30 = get_df('queue_phantom', ['Time', 'Queue', 'KMin', 'KMax'], max_values=100000)
    df3 = get_df('queue', ['Time', 'Queue', 'KMin', 'KMax'], max_values=100000)

    kmin = df3["KMin"].max()
    kmax = df3["KMax"].max()

    df4 = get_df('ecn', ['Time', 'ECN'], max_values=100000)

    df5 = get_df('sent', ['Time', 'Sent'], max_values=100000)

    if (args.show_case):
        # Case1 Data
        df30 = get_df('case1', ['Time', 'Case1'], max_values=100000)
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
        df31 = get_df('case2', ['Time', 'Case2'], max_values=100000)

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
        saved_old_sum = 0
        df32 = get_df('case3', ['Time', 'Case3'], max_values=100000)

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
        df33 = pd.DataFrame(columns=colnames)

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
    df6 = get_df('nack', ['Time', 'Nack'], max_values=100000)

    # Acked Bytes Data
    df8 = get_df('acked', ['Time', 'AckedBytes'], max_values=100000)

    # ECN in RTT Data
    df13 = get_df('ecn_rtt', ['Time', 'ECNRTT'], max_values=100000)

    # Trimming in RTT Data
    df14 = get_df('trimmed_rtt', ['Time', 'TrimmedRTT'], max_values=100000)

    # FastI data
    df9 = get_df('fasti', ['Time', 'FastI'], max_values=100000)

    # FastD data
    df10 = get_df('fastd', ['Time', 'FastD'], max_values=100000)

    # MediumI data
    df11 = get_df('mediumi', ['Time', 'MediumI'], max_values=100000)

    # Target RTT low data
    df_target_rtt_low = get_df('target_rtt_low', ['Time', 'target_rtt_low'])

    # Target RTT high data
    df_target_rtt_high = get_df('target_rtt_high', ['Time', 'target_rtt_high'])

    # Baremetal RTT data.
    df_baremetal_latency = get_df('baremetal_latency', ['Time', 'BaremetalLatency'])

    # Consecutive epochs below.
    df_consecutive_epochs_below = get_df('consecutive_epochs_below', ['Time', 'consecutive_epochs_below'])

    # Current RTT EWMA
    df_current_rtt_ewma = get_df('current_rtt_ewma', ['Time', 'current_rtt_ewma'])

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
    count = 0
    for i in df['Node'].unique():
        sub_df = df.loc[df['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(), opacity=0.9, showlegend=True),
            secondary_y=False,
        )
        '''
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode='markers', marker=dict(size=10), name=str(i), line=dict(color=color[0]), opacity=0.9, showlegend=True, marker_symbol="triangle-up"),
            secondary_y=False,
        )
        '''
        if (args.show_triangles is not None):
            fig.add_trace(
                go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode="markers", marker_symbol="triangle-up", name="Mark Packet", marker=dict(size=6, color=color[1]), showlegend=True),
                secondary_y=False
            )
        if (args.num_to_show == 1):
            break

    print("Congestion Plot")
    if (args.show_case):
        fig.add_trace(
            go.Scatter(x=df30["Time"], y=df30['Case1'], name="Case1", line=dict(dash='dot', color='violet'), showlegend=True),
            secondary_y=True,
        )

        fig.add_trace(
            go.Scatter(x=df31["Time"], y=df31['Case2'], name="Case2", line=dict(dash='dot', color='orange'), showlegend=True),
            secondary_y=True,
        )

        fig.add_trace(
            go.Scatter(x=df32["Time"], y=df32['Case3'], name="Case3", line=dict(dash='dot', color='blue'), showlegend=True),
            secondary_y=True,
        )

        fig.add_trace(
            go.Scatter(x=df33["Time"], y=df33['Case4'], name="Case4", line=dict(dash='dot', color='brown'), showlegend=True),
            secondary_y=True,
        )
    else:
        for i in df2['Node'].unique():
            sub_df = df2.loc[df2['Node'] == str(i)]
            fig.add_trace(
                go.Scatter(x=sub_df["Time"], y=sub_df['Congestion Window'], name=str(i).replace("cwd/cwdEqds_", ""), line=dict(dash='dot'), showlegend=True),
                secondary_y=True,
            )
    # Trimming RTT Bytes
    '''for i in df14['Node'].unique():
        sub_df = df14.loc[df14['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['TrimmedRTT'], name="TrimmedRTT " + str(i), line=dict(dash='longdashdot'), showlegend=True),
            secondary_y=True,
        )

    # ECN RTT Bytes
    for i in df13['Node'].unique():
        sub_df = df13.loc[df13['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['ECNRTT'], name="ECNRTT " + str(i), line=dict(dash='longdashdot'), showlegend=True),
            secondary_y=True,
        )
    
    # Acked Bytes
    print(df8)
    for i in df8['Node'].unique():
        sub_df = df8.loc[df8['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['AckedBytes'], name="Acked " + str(i), line=dict(dash='longdashdot'), showlegend=True),
            secondary_y=True,
        )'''

    # Queue
    print("Queue Plot")
    count = 0
    df3['Queue'] = pd.to_numeric(df3['Queue'])
    print(df3)
    max_ele = df3[['Queue']].idxmax(1)
    for i in df3['Node'].unique():
        sub_df = df3.loc[df3['Node'] == str(i)]
        if (skip_small_value is True and sub_df['Queue'].max() < 500):
            count += 1
            continue

        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Queue'], name="Queue " + str(i),   mode="markers",  marker=dict(size=1.4), line=dict(dash='dash', color="black", width=3),  showlegend=True),
            secondary_y=False,
        )
        count += 1

    # Phantom Queue
    print("Queue Plot")
    count = 0
    df30['Queue'] = pd.to_numeric(df30['Queue'])
    max_ele = df30[['Queue']].idxmax(1)
    for i in df30['Node'].unique():
        sub_df = df30.loc[df30['Node'] == str(i)]
        if (skip_small_value is True and sub_df['Queue'].max() < 500):
            count += 1
            continue

        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Queue'], name="Queue " + str(i),   mode="markers",  marker=dict(size=1.4), line=dict(dash='dash', color="pink", width=3),  showlegend=True),
            secondary_y=False,
        )
        count += 1

    print("ECN Plot")
    # ECN
    mean_ecn = df4["Time"].mean()
    for i in df4['Node'].unique():
        df4['ECN'] = y_ecn
        sub_df4 = df4.loc[df4['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df4["Time"], y=sub_df4['ECN'], mode="markers", marker_symbol="triangle-up", name="ECN Packet", marker=dict(size=5, color="yellow"), showlegend=True),
            secondary_y=False
        )
    '''print("Sent Plot")
    # Sent
    mean_sent = df5["Time"].mean()
    df5['Sent'] = df5['Sent'].multiply(y_sent)
    for i in df5['Node'].unique():
        sub_df5 = df5.loc[df5['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df5["Time"], y=sub_df5["Sent"], mode="markers", marker_symbol="triangle-up", name="Sent Packet", marker=dict(size=5, color="green"), showlegend=True),
            secondary_y=False
        )'''

    print("NACK Plot")
    # NACK
    mean_sent = df6["Time"].mean()
    df6['Nack'] = df6['Nack'].multiply(y_nack)
    for i in df6['Node'].unique():
        sub_df6 = df6.loc[df6['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df6["Time"], y=sub_df6["Nack"], mode="markers", marker_symbol="triangle-up", name="NACK Packet", marker=dict(size=5, color="grey"), showlegend=True),
            secondary_y=False
        )

    print("FAstI Plot")
    # FastI
    mean_sent = df9["Time"].mean()
    df9['FastI'] = df9['FastI'].multiply(y_fasti)
    for i in df9['Node'].unique():
        sub_df9 = df9.loc[df9['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df9["Time"], y=sub_df9["FastI"], mode="markers", marker_symbol="triangle-up", name="FastI Packet", marker=dict(size=5, color="brown"), showlegend=True),
            secondary_y=False
        )

    print("FastD Plot")
    # FastD
    mean_sent = df10["Time"].mean()
    df10['FastD'] = df10['FastD'].multiply(y_fastd)
    for i in df10['Node'].unique():
        sub_df10 = df10.loc[df10['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df10["Time"], y=sub_df10["FastD"], mode="markers", marker_symbol="triangle-up", name="FastD Packet", marker=dict(size=5, color="black"), showlegend=True),
            secondary_y=False
        )

    print("MediumI Plot")
    # MediumI
    mean_sent = df11["Time"].mean()
    df11['MediumI'] = df11['MediumI'].multiply(y_mediumi)
    for i in df11['Node'].unique():
        sub_df11 = df11.loc[df11['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df11["Time"], y=sub_df11["MediumI"], mode="markers", marker_symbol="triangle-up", name="MediumI Packet", marker=dict(size=5, color="white"), showlegend=True),
            secondary_y=False
        )


    # Target RTT low.
    # df_target_rtt_low['target_rtt_low'] = df_target_rtt_low['target_rtt_low'].multiply(y_target_rtt_low)
    fig.add_trace(
        go.Scatter(x=df_target_rtt_low["Time"], y=df_target_rtt_low['target_rtt_low'], name="Target RTT Low", line=dict(dash='dot', color='blue'), showlegend=True),
        secondary_y=False,
    )


    # Target RTT high.
    # df_target_rtt_high['target_rtt_high'] = df_target_rtt_high['target_rtt_high'].multiply(y_target_rtt_high)
    fig.add_trace(
        go.Scatter(x=df_target_rtt_high["Time"], y=df_target_rtt_high['target_rtt_high'], name="Target RTT High", line=dict(dash='dot', color='red'), showlegend=True),
        secondary_y=False,
    )

    # Baremetal RTT.
    # df_baremetal_latency['BaremetalLatency'] = df_baremetal_latency['BaremetalLatency'].multiply(y_baremetal_latency)
    fig.add_trace(
        go.Scatter(x=df_baremetal_latency["Time"], y=df_baremetal_latency['BaremetalLatency'], name="Baremetal RTT", line=dict(dash='dot', color='green'), showlegend=True),
        secondary_y=False,
    )

    # # Consecutive epochs below.
    # df_consecutive_epochs_below['consecutive_epochs_below'] = df_consecutive_epochs_below['consecutive_epochs_below'].multiply(y_consecutive_epochs_below)
    # for i in df_consecutive_epochs_below['Node'].unique():
    #     sub_df = df_consecutive_epochs_below.loc[df_consecutive_epochs_below['Node'] == str(i)]
    #     fig.add_trace(
    #         go.Scatter(x=sub_df["Time"], y=sub_df['consecutive_epochs_below'], name="Consecutive epochs below", line=dict(dash='dot'), showlegend=True),
    #         secondary_y=False,
    #     )

    # df_current_rtt_ewma['current_rtt_ewma'] = df_current_rtt_ewma['current_rtt_ewma'].multiply(y_current_rtt_ewma)

    for i in df_current_rtt_ewma['Node'].unique():
        sub_df = df_current_rtt_ewma.loc[df_current_rtt_ewma['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['current_rtt_ewma'], name=str(i), line=dict(dash='dot'), showlegend=True),
            secondary_y=False,
        )

    if args.name is not None:
        my_title=args.name
    else:
        my_title="<b>Incast 2:1 – 1:1 FT – 800Gbps – 4KiB MTU – 128MiB Flows - LoadBalancing ON</b>"

    # Add figure title
    fig.update_layout(title_text=my_title)


    if (args.x_limit is not None):
        fig.update_layout(xaxis_range=[0,args.x_limit])

    if (args.annotations is not None):
        fig.add_annotation(
        xref="x domain",
        yref="y domain",
        # The arrow head will be 25% along the x axis, starting from the left
        x=0.04,
        # The arrow head will be 40% along the y axis, starting from the bottom
        y=0.99,
        showarrow=False,
        text="Congestion Window",
        font=dict(color=color[2], size=13),
        )

        fig.add_annotation(
        xref="x domain",
        yref="y domain",
        # The arrow head will be 25% along the x axis, starting from the left
        x=0.12,
        # The arrow head will be 40% along the y axis, starting from the bottom
        y=0.8,
        showarrow=False,
        text="RTT",
        font=dict(color=color[1], size=13),
        )

        fig.add_annotation(
        xref="x domain",
        yref="y domain",
        # The arrow head will be 25% along the x axis, starting from the left
        x=0.03,
        # The arrow head will be 40% along the y axis, starting from the bottom
        y=0.47,
        showarrow=False,
        text="Queuing<br>Latency",
        font=dict(color=color[3], size=13),
        )

    # fig.add_shape(
    #     type="line",
    #     x0=0,  # Start x-coordinate
    #     x1=max_x,  # End x-coordinate
    #     y0=target_rtt,                          # Y-coordinate
    #     y1=target_rtt,                          # Y-coordinate
    #     line=dict(color="black", dash="dash"),
    # )

    print("Done Plotting")
    # # Add a text label over the dashed line
    # fig.add_annotation(
    #     x=max_x,  # You can adjust the x-coordinate as needed
    #     y=target_rtt + 250,                          # Set the y-coordinate to match the dashed line's y-coordinate
    #     text="Target RTT",          # The text label you want to display
    #     showarrow=False,               # No arrow pointing to the label
    #     font=dict(size=12, color="black"),  # Customize the font size and color
    # )

    # fig.add_shape(
    #     type="line",
    #     x0=0,  # Start x-coordinatep
    #     x1=max_x,  # End x-coordinate
    #     y0=base_rtt,                          # Y-coordinate
    #     y1=base_rtt,                          # Y-coordinate
    #     line=dict(color="black", dash="dash"),
    # )

    # # Add a text label over the dashed line
    # fig.add_annotation(
    #     x=max_x,  # You can adjust the x-coordinate as needed
    #     y=base_rtt + 250,                          # Set the y-coordinate to match the dashed line's y-coordinate
    #     text="Base RTT",          # The text label you want to display
    #     showarrow=False,               # No arrow pointing to the label
    #     font=dict(size=12, color="black"),  # Customize the font size and color
    # )


    # fig.add_shape(
    #     type="line",
    #     x0=0,  # Start x-coordinate
    #     x1=max_x,  # End x-coordinate
    #     y0=kmin,                          # Y-coordinate
    #     y1=kmin,                          # Y-coordinate
    #     line=dict(color="green", dash="dash"),
    # )

    # # Add a text label over the dashed line
    # fig.add_annotation(
    #     x=max_x,  # You can adjust the x-coordinate as needed
    #     y=kmin + 250,                          # Set the y-coordinate to match the dashed line's y-coordinate
    #     text="KMin",          # The text label you want to display
    #     showarrow=False,               # No arrow pointing to the label
    #     font=dict(size=12, color="green"),  # Customize the font size and color
    # )

    # fig.add_shape(
    #     type="line",
    #     x0=0,  # Start x-coordinate
    #     x1=max_x,  # End x-coordinate
    #     y0=kmax,                          # Y-coordinate
    #     y1=kmax,                          # Y-coordinate
    #     line=dict(color="green", dash="dash"),
    # )

    # # Add a text label over the dashed line
    # fig.add_annotation(
    #     x=max_x,  # You can adjust the x-coordinate as needed
    #     y=kmax + 250,                          # Set the y-coordinate to match the dashed line's y-coordinate
    #     text="KMax",          # The text label you want to display
    #     showarrow=False,               # No arrow pointing to the label
    #     font=dict(size=12, color="green"),  # Customize the font size and color
    # )

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
    fig.update_yaxes(title_text="RTT || Queuing Latency (ns)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)

    # Set font size for axes and legend.
    fig.update_layout(
        font=dict(
            family="Courier New, monospace",
            size=20
        )
    )
    
    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    print("Saving Plot")
    #fig.write_image("out/fid_simple_{}.png".format(date_time))
    #plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    print("Showing Plot")
    if (args.output_folder is not None):
        plotly.offline.plot(fig, filename=args.output_folder + "/{}.html".format(args.name))
        fig.write_image(args.output_folder + "/{}.png".format(args.name), height=809, width=1600, scale=1.0)
    elif (args.no_show is None):
        if (args.output_folder is not None):
            plotly.offline.plot(fig, filename=args.output_folder + "/{}.html".format(args.name))
            # # Save to png as well with a particular size.
            # fig.write_image(args.output_folder + "/{}.png".format(args.name), height=550, width=1000, scale=4)
        else:
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