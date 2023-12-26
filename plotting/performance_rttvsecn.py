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

    # RTT Data
    colnames=['Time', 'RTT', 'seqno', 'ackno', 'base', 'target']
    df = pd.DataFrame(columns =['Time','RTT', 'seqno', 'ackno', 'base', 'target'])
    name = ['0'] * df.shape[0]
    df = df.assign(Node=name)

    pathlist = Path('rtt_example_rtt').glob('**/*.txt')
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

    pathlist = Path('cwd_example_rtt').glob('**/*.txt')
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

    pathlist = Path('queue_size_normalized_example_rtt').glob('**/*.txt')
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

    kmin = df3["KMin"].max()
    kmax = df3["KMax"].max()

    if (len(df3) > 100000):
        ratio = len(df3) / 50000
        # DownScale
        df3 = df3.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df3.reset_index(drop=True, inplace=True)

    # RTT Data
    colnames=['Time', 'RTT', 'seqno', 'ackno', 'base', 'target']
    df4 = pd.DataFrame(columns =['Time','RTT', 'seqno', 'ackno', 'base', 'target'])
    name = ['0'] * df4.shape[0]
    df4 = df4.assign(Node=name)

    pathlist = Path('rtt_example_ecn').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df4 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df4.shape[0]
        temp_df4 = temp_df4.assign(Node=name)
        df4 = pd.concat([df4, temp_df4])

    base_rtt = df4["base"].max()
    target_rtt = df4["target"].max()

    if (len(df4) > 100000):
        ratio = len(df4) / 50000
        # DownScale
        df4 = df4.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df4.reset_index(drop=True, inplace=True)

    # Cwd Data
    colnames=['Time', 'Congestion Window'] 
    df5 = pd.DataFrame(columns =colnames)
    name = ['0'] * df5.shape[0]
    df5 = df5.assign(Node=name)
    df5.drop_duplicates('Time', inplace = True)

    pathlist = Path('cwd_example_ecn').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df5 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df5.shape[0]
        temp_df5 = temp_df5.assign(Node=name)
        temp_df5.drop_duplicates('Time', inplace = True)
        df5 = pd.concat([df5, temp_df5])
    if (len(df5) > 100000):
        ratio = len(df5) / 50000
        # DownScale
        df5 = df5.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df5.reset_index(drop=True, inplace=True)
    

    # Queue Data
    colnames=['Time', 'Queue', 'KMin', 'KMax'] 
    df6= pd.DataFrame(columns =colnames)
    name = ['0'] * df6.shape[0]
    df6 = df6.assign(Node=name)
    df6.drop_duplicates('Time', inplace = True)

    pathlist = Path('queue_size_normalized_example_ecn').glob('**/*.txt')
    for files in natsort.natsorted(pathlist,reverse=False):
        '''pattern = r'^queue_size_normalized/queueUS_\d+-CS_\d+\.txt$'
        if not re.match(pattern, str(files)):
            continue'''

        path_in_str = str(files)
        temp_df3 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df3.shape[0]
        temp_df3 = temp_df3.assign(Node=name)
        temp_df3.drop_duplicates('Time', inplace = True)
        df6 = pd.concat([df6, temp_df3])

    kmin = df6["KMin"].max()
    kmax = df6["KMax"].max()

    if (len(df6) > 100000):
        ratio = len(df6) / 50000
        # DownScale
        df6 = df6.iloc[::int(ratio)]
        # Reset the index of the new dataframe
        df6.reset_index(drop=True, inplace=True)

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

    df["RTT"] = (df["RTT"]/1000)
    df4["RTT"] = (df4["RTT"]/1000)
    df["Time"] = (df["Time"]/1000)
    df4["Time"] = (df4["Time"]/1000)

    df2["Time"] = (df2["Time"]/1000)
    df5["Time"] = (df5["Time"]/1000)

    df3["Time"] = (df3["Time"]/1000)
    df6["Time"] = (df6["Time"]/1000)


    for i in df['Node'].unique():
        sub_df = df.loc[df['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[0]), opacity=0.9, showlegend=True),
            secondary_y=False,
        )

        if (args.num_to_show == 1):
            break

    for i in df4['Node'].unique():
        sub_df4 = df4.loc[df4['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df4["Time"], y=sub_df4['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[2]), opacity=0.9, showlegend=True),
            secondary_y=False,
        )

        if (args.num_to_show == 1):
            break

    print("Congestion Plot")
    for i in df2['Node'].unique():
        sub_df = df2.loc[df2['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Congestion Window'], name="CWD " + str(i), line=dict(dash='dot', color=color[0]), showlegend=True),
            secondary_y=True,
        )

    for i in df5['Node'].unique():
        sub_df = df5.loc[df5['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Congestion Window'], name="CWD2 " + str(i), line=dict(dash='dot', color=color[2]), showlegend=True),
            secondary_y=True,
        )

        
    if args.name is not None:
        my_title=args.name
    else:
        my_title="<b>Permutation Across - 4:1 FT - 400Gbps - 2 MiB - UEC</b>"

    # Add figure title
    fig.update_layout(title_text=my_title)

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
        fig.update_layout(xaxis_range=[0,200000/1000])
    config = {
    'toImageButtonOptions': {
        'format': 'png', # one of png, svg, jpeg, webp
        'filename': 'custom_image',
        'height': 550,
        'width': 1000,
        'scale':4 # Multiply title/legend/axis/canvas sizes by this factor
    }
    }
    fig.update_layout(xaxis_range=[0,200000/1000])

    # Set x-axis title
    fig.update_xaxes(title_text="Time (μs)")
    # Set y-axes titles
    fig.update_yaxes(title_text="RTT || Queuing Latency (μs)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)
    fig.update_layout(showlegend=False)
    fig.update_layout(
        title=dict(
            text='Comparing ECN vs RTT reaction - Incast 8:1',
            x=0.5,
            y=0.95,
            font=dict(
                family="Arial",
                size=22,
                color='#000000'
            )
        ),
        xaxis_title="Time (μs)",
        yaxis_title='RTT (μs)',
        font=dict(
            family="Arial",
            size=20,
            color='#000000'
        )
    )


    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    print("Saving Plot")
    #fig.write_image("out/fid_simple_{}.png".format(date_time))
    #plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    print("Showing Plot")
    fig.write_image("OUT/myimg.svg")


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
