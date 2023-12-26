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

    os.system("cp -a ../sim/output/cwd/. cwd/")
    os.system("cp -a ../sim/output/rtt/. rtt/")
    os.system("cp -a ../sim/output/queue/. queue_size_normalized/")
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

    # RTT Data
    colnames=['Time', 'RTT', 'seqno', 'ackno', 'base', 'target']
    df = pd.DataFrame(columns =['Time','RTT', 'seqno', 'ackno', 'base', 'target'])
    
    name = ['0'] * df.shape[0]
    df = df.assign(Node=name)

    print("Processing RTT")
    i = 0
    pathlist = Path('rtt_ecmp').glob('**/*.txt')
    for files in sorted(pathlist):
        print(i)
        i += 1
        path_in_str = str(files)
        temp_df = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df.shape[0]
        temp_df = temp_df.assign(Node=name)
        df = pd.concat([df, temp_df])
    base_rtt = df["base"].max()
    target_rtt = df["target"].max()
    print("Len is {} RTT\n".format(len(df)))
    if (len(df) > 500000):
        print("Downscaling")
        # DownScale
        ratio = int(len(df) / 50000)
        df = df.iloc[::ratio]
        # Reset the index of the new dataframe
        df.reset_index(drop=True, inplace=True)

    # RTT Reps
    colnames=['Time', 'RTT', 'seqno', 'ackno', 'base', 'target']
    df_reps = pd.DataFrame(columns =['Time','RTT', 'seqno', 'ackno', 'base', 'target'])
    
    name = ['0'] * df_reps.shape[0]
    df_reps = df_reps.assign(Node=name)

    print("Processing RTT")
    i = 0
    pathlist = Path('rtt_reps').glob('**/*.txt')
    for files in sorted(pathlist):
        print(i)
        i += 1
        path_in_str = str(files)
        temp_df_reps = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df_reps.shape[0]
        temp_df_reps = temp_df_reps.assign(Node=name)
        df_reps = pd.concat([df_reps, temp_df_reps])
    base_rtt = df_reps["base"].max()
    target_rtt = df_reps["target"].max()
    print("Len is {} RTT\n".format(len(df_reps)))
    if (len(df_reps) > 500000):
        print("Downscaling")
        # DownScale
        ratio = int(len(df_reps) / 50000)
        df_reps = df_reps.iloc[::ratio]
        # Reset the index of the new dataframe
        df_reps.reset_index(drop=True, inplace=True)

    # Cwd Data
    print("Processing CWD")
    colnames=['Time', 'Congestion Window'] 
    df2 = pd.DataFrame(columns =colnames)
    name = ['0'] * df2.shape[0]
    df2 = df2.assign(Node=name)
    df2.drop_duplicates('Time', inplace = True)

    pathlist = Path('cwd').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df2 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        if (temp_df2.shape[0] < 20):
            continue
        name = [str(path_in_str)] * temp_df2.shape[0]
        temp_df2 = temp_df2.assign(Node=name)
        temp_df2.drop_duplicates('Time', inplace = True)
        df2 = pd.concat([df2, temp_df2])
    print("Len is {} CWD\n".format(len(df2)))
    if (len(df2) > 500000):
        # DownScale
        ratio = int(len(df2) / 50000)
        df2 = df2.iloc[::ratio]
        # Reset the index of the new dataframe
        df2.reset_index(drop=True, inplace=True)


    print("Finished Parsing")
    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])
    color = ['#636EFA', '#0511a9', '#EF553B', '#00CC96', '#AB63FA', '#FFA15A', '#19D3F3', '#FF6692', '#B6E880', '#FF97FF', '#FECB52']
    # Add traces
    mean_rtt = df["RTT"].mean()
    max_rtt = df["RTT"].max()
    y_sent = max_rtt * 0.9
    y_ecn = max_rtt * 0.85
    max_x = df["Time"].max()
    y_nack = max_rtt * 0.80
    y_fasti = max_rtt * 0.75
    y_fastd = max_rtt * 0.70
    y_mediumi = max_rtt * 0.65
    mean_rtt = 10000
    count = 0
    for i in df['Node'].unique():
        sub_df = df.loc[df['Node'] == str(i)]
        sub_df_reps = df_reps.loc[df_reps['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[0]), opacity=0.95, showlegend=True),
            secondary_y=False,
        )
        if (args.num_to_show == 1):
            break

    for i in df_reps['Node'].unique():
        sub_df_reps = df_reps.loc[df_reps['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df_reps["Time"], y=sub_df_reps['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[2]), opacity=0.70, showlegend=False),
            secondary_y=False,
        )

    print("Displaying Plot")
    if args.name is not None:
        my_title=args.name
    else:
        my_title="<b>Permutation Across - 4:1 FT - 400Gbps - 2 MiB - UEC</b>"

    # Add figure title
    fig.update_layout(title_text=my_title)


    if (args.x_limit is not None):
        fig.update_layout(xaxis_range=[0,args.x_limit])

    fig.update_layout(yaxis_range=[0,22000])
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
        y0=target_rtt,                          # Y-coordinate
        y1=target_rtt,                          # Y-coordinate
        line=dict(color="black", dash="dash", width=3),
    )

    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=target_rtt + 300,                          # Set the y-coordinate to match the dashed line's y-coordinate
        text="Target RTT",          # The text label you want to display
        showarrow=False,               # No arrow pointing to the label
        font=dict(size=20, color="black"),  # Customize the font size and color
    )

    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=base_rtt,                          # Y-coordinate
        y1=base_rtt,                          # Y-coordinate
        line=dict(color="black", dash="dash", width=3),
    )

    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=base_rtt + 300,                          # Set the y-coordinate to match the dashed line's y-coordinate
        text="Base RTT",          # The text label you want to display
        showarrow=False,               # No arrow pointing to the label
        font=dict(size=20, color="black"),  # Customize the font size and color
    )


    # Set x-axis title
    fig.update_xaxes(title_text="Time (ns)")
    # Set y-axes titles
    fig.update_yaxes(title_text="RTT (ns)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)


    fig.update_layout(
        title=dict(
            text='Comparing REPS and obvlivious spraying - LinkSpeed 800Gbps - MTU 4KiB',
            x=0.5,
            y=0.95,
            font=dict(
                family="Arial",
                size=22,
                color='#000000'
            )
        ),
        xaxis_title="Time (ns)",
        yaxis_title='RTT (ns)',
        font=dict(
            family="Arial",
            size=20,
            color='#000000'
        )
    )

    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    fig.write_image("out/fid_simple_{}.png".format(date_time),  width=2560, height=1440)
    plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
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
