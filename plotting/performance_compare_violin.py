import plotly.express as px
import pandas as pd
from pathlib import Path
import plotly.graph_objs as go
import plotly
from plotly.subplots import make_subplots
from datetime import datetime
import os
import re
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
import numpy as np
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

    # RTT Reps
    colnames=['Time', 'RTT', 'seqno', 'ackno', 'base', 'target']
    df_ecmp_classic = pd.DataFrame(columns =['Time','RTT', 'seqno', 'ackno', 'base', 'target'])
    
    name = ['0'] * df_ecmp_classic.shape[0]
    df_ecmp_classic = df_ecmp_classic.assign(Node=name)

    print("Processing RTT")
    i = 0
    pathlist = Path('rtt_ecmp_classic').glob('**/*.txt')
    for files in sorted(pathlist):
        print(i)
        i += 1
        path_in_str = str(files)
        temp_df_ecmp_classic = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
        name = [str(path_in_str)] * temp_df_ecmp_classic.shape[0]
        temp_df_ecmp_classic = temp_df_ecmp_classic.assign(Node=name)
        df_ecmp_classic = pd.concat([df_ecmp_classic, temp_df_ecmp_classic])
    base_rtt = df_ecmp_classic["base"].max()
    target_rtt = df_ecmp_classic["target"].max()
    print("Len is {} RTT\n".format(len(df_ecmp_classic)))
    if (len(df_ecmp_classic) > 500000):
        print("Downscaling")
        # DownScale
        ratio = int(len(df_ecmp_classic) / 50000)
        df_ecmp_classic = df_ecmp_classic.iloc[::ratio]
        # Reset the index of the new dataframe
        df_ecmp_classic.reset_index(drop=True, inplace=True)

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
            go.Scatter(x=sub_df["Time"], y=sub_df['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[0]), opacity=1, showlegend=True),
            secondary_y=False,
        )
        if (args.num_to_show == 1):
            break

    for i in df_ecmp_classic['Node'].unique():
        sub_df_ecmp_classic = df_ecmp_classic.loc[df_ecmp_classic['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df_ecmp_classic["Time"], y=sub_df_ecmp_classic['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[5]), opacity=0.80, showlegend=False),
            secondary_y=False,
        )

    for i in df_reps['Node'].unique():
        sub_df_reps = df_reps.loc[df_reps['Node'] == str(i)]
        fig.add_trace(
            go.Scatter(x=sub_df_reps["Time"], y=sub_df_reps['RTT'], mode='markers', marker=dict(size=2), name=str(i), line=dict(color=color[2]), opacity=0.60, showlegend=False),
            secondary_y=False,
        )

    # Create a violin plot
    plt.figure(figsize=(7, 5))
    plt.grid()  #just add this


    sub_df_reps = sub_df_reps[sub_df_reps['Time'] > 140000]
    sub_df_ecmp_classic = sub_df_ecmp_classic[sub_df_ecmp_classic['Time'] > 140000]
    sub_df = sub_df[sub_df['Time'] > 140000]

    sub_df_reps["RTT"] = (sub_df_reps["RTT"]/1000)
    sub_df_ecmp_classic["RTT"] = (sub_df_ecmp_classic["RTT"]/1000)
    sub_df["RTT"] = (sub_df["RTT"]/1000)

    ax = sns.violinplot(data=[sub_df_reps['RTT'], sub_df_ecmp_classic['RTT'], sub_df['RTT']], cut=0)
    ax.set_axisbelow(True)

    ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = 14)
    ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 14)

    # Add a vertical dashed line at x=1000 with lower opacity
    plt.axhline(y=8680/1000, linestyle='--', color='black', alpha=1)
    plt.axhline(y=13086/1000, linestyle='--', color='black', alpha=1)


    # Add text next to the vertical line
    plt.text(0.3, 9000/1000, 'Base RTT', rotation=0, va='center', color='black',fontsize=12)
    plt.text(0.3, 13586/1000, 'Target RTT', rotation=0, va='center', color='black',fontsize=12)

    # Set labels and title
    plt.xlabel('Load Balancing Scheme',fontsize=15)
    plt.ylabel('Packet RTT (μs)',fontsize=15)
    plt.title('Comparing load balancers - 4:1 oversubscribed FT',fontsize=15.2)
    ax.set_xticklabels(['REPS', 'Per-Flow ECMP', 'Oblivious Spraying'])


    # Show the plot
    plt.tight_layout()
    plt.savefig("perf_violin.png", bbox_inches='tight')
    plt.savefig("perf_violin.pdf", bbox_inches='tight')


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
