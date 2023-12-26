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
    os.system("rm -r ls_to_us/")
    os.system("rm -r us_to_cs/")

    os.system("cp -a ../sim/output/cwd/. cwd/")
    os.system("cp -a ../sim/output/rtt/. rtt/")
    os.system("cp -a ../sim/output/queue/. queue_size_normalized/")
    os.system("cp -a ../sim/output/sent/. sent/")
    os.system("cp -a ../sim/output/ecn/. ecn/")
    os.system("cp -a ../sim/output/nack/. nack/")
    os.system("cp -a ../sim/output/ls_to_us/. ls_to_us/")
    os.system("cp -a ../sim/output/us_to_cs/. us_to_cs/")

    # Queue Data
    colnames=['Time', 'Queue'] 
    df3= pd.DataFrame(columns =colnames)
    name = ['0'] * df3.shape[0]
    df3 = df3.assign(Node=name)
    df3.drop_duplicates('Time', inplace = True)

    pathlist = Path('queue_size_normalized').glob('**/*.txt')
    for files in natsort.natsorted(pathlist,reverse=False):
    #for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df3 = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df3.shape[0]
        temp_df3 = temp_df3.assign(Node=name)
        temp_df3.drop_duplicates('Time', inplace = True)
        df3 = pd.concat([df3, temp_df3])

    # LS TO US 
    colnames=['Time', 'Link Used']
    df_ls_ = pd.DataFrame(columns =['Time','Link Used'])
    name = ['0'] * df_ls_.shape[0]
    df_ls_ = df_ls_.assign(Node=name)

    pathlist = Path('ls_to_us').glob('**/*.txt')
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df_ls_ = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df_ls_.shape[0]
        temp_df_ls_ = temp_df_ls_.assign(Node=name)
        df_ls_ = pd.concat([df_ls_, temp_df_ls_])

    # Generate Througput 
    df_ls_ = df_ls_.sort_values('Time')
    df_ls_.to_csv("test.csv", encoding='utf-8', index=False)
    list_unique_links = len(df_ls_['Link Used'].unique())
    packet_size = args.mtu
    interval_width = args.interval_width
    link_speed = 400

    for link in range(list_unique_links):
        list_throughput = []
        interval_start = args.interval_start
        interval_end = interval_start + interval_width
        list_pkts = []
        count_pkts = 0
        for index, row in df_ls_.iterrows():
            if (link == int(row['Link Used']) and int(row['Time']) > args.interval_start and int(row['Time']) < args.interval_end):
                if (int(row['Time']) < interval_end):
                    count_pkts += 1
                else:
                    list_pkts.append(count_pkts)
                    list_throughput.append({"Link":link, "Time":interval_end, "Throughput":count_pkts * packet_size * 8 / interval_width})

                    interval_end = interval_end + interval_width
                    count_pkts = 1
        # Now save to dataframe
        list_throughput = list_throughput[1:]
        list_pkts = list_pkts[1:]
        if (link == 0):
            df_ls_2 = pd.DataFrame(list_throughput)
        else:
            temp_df_ls_ = pd.DataFrame(list_throughput)
            df_ls_2 = pd.concat([df_ls_2, temp_df_ls_])

    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])
    color = ['#0511a9', '#EF553B', '#00CC96', '#AB63FA', '#FFA15A', '#19D3F3', '#FF6692', '#B6E880', '#FF97FF','#636EFA', '#FECB52']


    # Queue
    count = 0
    df3['Queue'] = pd.to_numeric(df3['Queue'])
    max_ele = df3[['Queue']].idxmax(1)
    for i in df3['Node'].unique():
        sub_df = df3.loc[df3['Node'] == str(i)]
        if not "LS0" in str(i):
            continue
        color_idx = count % len(color)
        fig.add_trace(
            go.Scatter(x=sub_df["Time"], y=sub_df['Queue'],  opacity=0.55,  name=str(i).split('-')[1],  line=dict(dash='dash', color=color[color_idx], width=1.5),  showlegend=True),
            secondary_y=True,
        )
        count += 1

    # Add traces LS TO US
    print(df_ls_2)
    count = 0
    for i in df_ls_2['Link'].unique():
        sub_df_ls_2 = df_ls_2.loc[df_ls_2['Link'] == i]
        color_idx = i % len(color)
        fig.add_trace(
            go.Scatter(x=sub_df_ls_2["Time"], y=sub_df_ls_2['Throughput'], marker=dict(size=10), name="LS_US_" + str(i), line=dict(color=color[color_idx], width=2), opacity=1, showlegend=True, marker_symbol="triangle-up"),
            secondary_y=False,
        )

        if (args.show_triangles is not None):
            fig.add_trace(
                go.Scatter(x=sub_df_ls_2["Time"], y=sub_df_ls_2['Throughput'], mode="markers",name="Lower Switch to Upper Switch, link: " + str(i), marker_symbol="triangle-up", marker=dict(size=6, color=color[i]), showlegend=False),
                secondary_y=False
            )


    # Add figure title
    fig.update_layout(
        #title_text="<b>Incast 64:1 - ECN based congestion control - 100Gbps</b>"
        title_text="<b>Permutation Across - 4:1 FT - Link Throughput - 400Gbps - 2 MiB Flows - LoadBalancing OFF - UEC</b>"
    )


    if (args.x_limit is not None):
        fig.update_layout(xaxis_range=[0,args.x_limit])

    if (args.y_limit is not None):
        fig.update_layout(yaxis_range=[0,args.y_limit])

    fig.update_yaxes(range=[0,21000], secondary_y=True)

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
    fig.update_yaxes(title_text="Link Throughput (Gb/s)", secondary_y=False)
    fig.update_yaxes(title_text="Queue Size (ns)", secondary_y=True)

    now = datetime.now() # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    fig.write_image("out/fid_simple_{}.png".format(date_time),  width=2560, height=1440)
    plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    fig.show()



if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--x_limit", type=int, help="Max X Value showed", default=None)
    parser.add_argument("--y_limit", type=int, help="Max Y value showed", default=None)
    parser.add_argument("--mtu", type=int, help="MTU Size (B)", default=None)
    parser.add_argument("--interval_start", type=int, help="Interval X from when to start tracking", default=None)
    parser.add_argument("--interval_end", type=int, help="Interval X from when to stop tracking", default=None)
    parser.add_argument("--interval_width", type=int, help="Interval X width", default=None)
    parser.add_argument("--show_ecn", type=str, help="Show ECN points", default=None)
    parser.add_argument("--show_sent", type=str, help="Show Sent Points", default=None) 
    parser.add_argument("--show_triangles", type=str, help="Show RTT triangles", default=None) 
    parser.add_argument("--num_to_show", type=int, help="Number of lines to show", default=None) 
    parser.add_argument("--annotations", type=str, help="Number of lines to show", default=None) 
    args = parser.parse_args()
    main(args)
