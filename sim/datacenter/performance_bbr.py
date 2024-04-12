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
    colnames=['Time', 'RTT']
    df = pd.DataFrame(columns =['Time','RTT'])
    
    name = ['0'] * df.shape[0]
    df = df.assign(Node=name)

    print("Processing RTT")
    i = 0
    pathlist = Path('../output/out_bw').glob('**/*.txt')
    for files in sorted(pathlist):
        i += 1
        path_in_str = str(files)
        temp_df = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df.shape[0]
        temp_df = temp_df.assign(Node=name)
        df = pd.concat([df, temp_df])
    print("Len is {} RTT\n".format(len(df)))


    # Cwd Data
    print("Processing CWD")
    colnames=['Time', 'Congestion Window'] 
    df2 = pd.DataFrame(columns =colnames)
    name = ['0'] * df2.shape[0]
    df2 = df2.assign(Node=name)
    df2.drop_duplicates('Time', inplace = True)

    pathlist = Path('../output/out_bw_paced').glob('**/*.txt')
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

    max_y_v = max(df2['Congestion Window'])
    print(max_y_v)

    colnames=['Time', 'Status']
    df3 = pd.DataFrame(columns =['Time','Status'])
    
    name = ['0'] * df3.shape[0]
    df3 = df3.assign(Node=name)
    print("Processing Status")
    i = 0
    pathlist = Path('../output/status').glob('**/*.txt')
    for files in sorted(pathlist):
        i += 1
        path_in_str = str(files)
        temp_df3 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df3.shape[0]
        temp_df3 = temp_df3.assign(Node=name)
        df3 = pd.concat([df3, temp_df3])
    print("Len is {} Status\n".format(len(df3)))


    print("Finished Parsing")
    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])
    color = ['#636EFA', '#0511a9', '#EF553B', '#00CC96', '#AB63FA', '#FFA15A', '#19D3F3', '#FF6692', '#B6E880', '#FF97FF', '#FECB52']
    # Add traces
    for i in df2['Node'].unique():
        sub_df = df2.loc[df2['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Congestion Window'], name="CWD " + str(i), line=dict(dash='dot'), showlegend=True),
            secondary_y=False,
        )

    count = 0
    for i in df['Node'].unique():
        sub_df = df.loc[df['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], name=str(i), showlegend=True),
            secondary_y=False,
        )

        if (args.num_to_show == 1):
            break
    

    color_line = "yellow"
    '''for i in df3['Node'].unique():
        sub_df3 = df3.loc[df3['Node'] == str(i)]

        for idx, num in (enumerate(sub_df3['Status'])):
            if (int(num) == 0):
                color_line = "green"
            elif (int(num) == 1):
                color_line = "blue"
            elif (int(num) == 2):
                color_line = "grey"

            fig.add_shape(
                type="line",
                x0=sub_df3["Time"][idx],  # Start x-coordinate
                x1=sub_df3["Time"][idx],  # End x-coordinate
                y0=0,                          # Y-coordinate
                y1=max_y_v + 15,                          # Y-coordinate
                opacity=0.4,
                line=dict(color=color_line, dash="dash"),
            )
    '''
    fig.update_yaxes(rangemode="tozero")


    print("Displaying Plot")
    if args.name is not None:
        my_title=args.name
    else:
        my_title="<b>Permutation Across - 4:1 FT - 400Gbps - 2 MiB - UEC</b>"

    # Add figure title
    fig.update_layout(title_text=my_title)


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
    fig.update_yaxes(title_text="RTT || Queuing Latency (ns)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)

    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
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
