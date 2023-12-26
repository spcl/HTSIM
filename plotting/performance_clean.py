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

    base_rtt  = df["base"].max()
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
    

    # Queue Data
    colnames=['Time', 'Queue', 'KMin', 'KMax'] 
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
        df3 = pd.concat([df3, temp_df3])

    kmin = df3["KMin"].max() / 1000
    kmax = df3["KMax"].max() / 1000

    print(kmin)
    print(kmax)

    if (len(df3) > 100000):
        ratio = len(df3) / 50000
        # DownScale
        df3 = df3.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df3.reset_index(drop=True, inplace=True)

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

    # Sent data
    colnames=['Time', 'Sent'] 
    df5 = pd.DataFrame(columns =colnames)
    name = ['0'] * df5.shape[0]
    df5 = df5.assign(Node=name)

    pathlist = Path('sent').glob('*')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df5 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df5.shape[0]
        temp_df5 = temp_df5.assign(Node=name)
        df5 = pd.concat([df5, temp_df5])

    if (len(df5) > 100000):
        ratio = len(df) / 50000
        # DownScale
        df5 = df5.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df5.reset_index(drop=True, inplace=True)

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

    df["Time"] = (df["Time"]/1000)
    df["RTT"] = (df["RTT"]/1000)
    df3["Queue"] = (df3["Queue"]/1000)
    df2["Time"] = (df2["Time"]/1000)
    df3["Time"] = (df3["Time"]/1000)
    df4["Time"] = (df4["Time"]/1000)
    df5["Time"] = (df5["Time"]/1000)
    df6["Time"] = (df6["Time"]/1000)
     

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
    for i in df['Node'].unique():
        sub_df = df.loc[df['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[0]), opacity=0.9, showlegend=True),
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
                go.Scatter(x=sub_df["Time"], y=sub_df['Congestion Window'], name="CWD " + str(i), line=dict(dash='dot'), showlegend=True),
                secondary_y=True,
            )


    # Queue
    print("Queue Plot")
    count = 0
    df3['Queue'] = pd.to_numeric(df3['Queue'])
    max_ele = df3[['Queue']].idxmax(1)
    for i in df3['Node'].unique():
        sub_df = df3.loc[df3['Node'] == str(i)]
        if (skip_small_value is True and sub_df['Queue'].max() < 0):
            count += 1
            continue

        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Queue'], name="Queue " + str(i),   mode="markers",  marker=dict(size=1.4), line=dict(dash='dash', color="black", width=3),  showlegend=True),
            secondary_y=False,
        )
        count += 1

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


    # Add figure title

    fig.update_layout(
        plot_bgcolor='white'
    )
    fig.update_xaxes(
        mirror=True,
        ticks='outside',
        showline=True,
        linecolor='black',
        gridcolor='lightgrey'
    )
    fig.update_yaxes(
        mirror=True,
        ticks='outside',
        showline=True,
        linecolor='black',
        gridcolor='lightgrey'
    )


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

    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=target_rtt / 1000,                          # Y-coordinate
        y1=target_rtt / 1000,                          # Y-coordinate
        line=dict(color="black", dash="dash", width=3),
    )

    print("Done Plotting")
    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=target_rtt / 1000 + 0.9,                          # Set the y-coordinate to match the dashed line's y-coordinate
        text="Target RTT",          # The text label you want to display
        showarrow=False,               # No arrow pointing to the label
        font=dict(size=20, color="black"),  # Customize the font size and color
    )

    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=base_rtt / 1000,                          # Y-coordinate
        y1=base_rtt / 1000,                          # Y-coordinate
        line=dict(color="black", dash="dash", width=3),
    )

    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=base_rtt / 1000 + 0.9,                          # Set the y-coordinate to match the dashed line's y-coordinate
        text="Base RTT",          # The text label you want to display
        showarrow=False,               # No arrow pointing to the label
        font=dict(size=20, color="black"),  # Customize the font size and color
    )


    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=kmin,                          # Y-coordinate
        y1=kmin,                          # Y-coordinate
        line=dict(color="green", dash="dash", width=3),
    )

    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=kmin + 0.7,                          # Set the y-coordinate to match the dashed line's y-coordinate
        text="KMin",          # The text label you want to display
        showarrow=False,               # No arrow pointing to the label
        font=dict(size=20, color="green"),  # Customize the font size and color
    )

    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=kmax,                          # Y-coordinate
        y1=kmax,                          # Y-coordinate
        line=dict(color="green", dash="dash", width=3),
    )

    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=kmax + 0.7,                          # Set the y-coordinate to match the dashed line's y-coordinate
        text="KMax",          # The text label you want to display
        showarrow=False,               # No arrow pointing to the label
        font=dict(size=20, color="green"),  # Customize the font size and color
    )

    fig.update_layout(
        xaxis_title="Time (ns)",
        yaxis_title='RTT || Queuing Latency (ns)',
        font=dict(
            family="Arial",
            size=20,
            color='#000000'
        )
    )

    fig.update_xaxes(title_text="Time (μs)")
    # Set y-axes titles
    fig.update_yaxes(title_text="RTT || Queuing Latency (μs)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)
    fig.update_layout(showlegend=False)

    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    print("Saving Plot")
    #fig.write_image("out/fid_simple_{}.png".format(date_time))
    #plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    print("Showing Plot")
    fig.write_image("OUT/timeseries.svg")
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
