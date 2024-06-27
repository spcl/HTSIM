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
import warnings

warnings.filterwarnings("ignore")

# Parameters
skip_small_value = True
ECN = True


def run(string_to_run, title):
    # Clean Data and Copy Data
    os.chdir("../../sim/datacenter/")
    os.system(string_to_run)
    os.chdir("../../plotting/")
    os.system("rm -r queue_size_normalized/")
    os.system("rm -r rtt/")
    os.system("rm -r cwd/")
    os.system("rm -r switch_failures/")
    os.system("rm -r cable_failures/")
    os.system("rm -r switch_drops/")
    os.system("rm -r cable_drops/")
    os.system("rm -r routing_failed_switch/")
    os.system("rm -r routing_failed_cable/")
    os.system("rm -r switch_degradations/")
    os.system("rm -r cable_degradations/")
    os.system("rm -r random_packet_drops/")

    os.system("cp -a ../sim/output/cwd/. cwd/")
    os.system("cp -a ../sim/output/rtt/. rtt/")
    os.system("cp -a ../sim/output/queue/. queue_size_normalized/")
    os.system("cp -a ../sim/output/switch_drops/. switch_drops/")
    os.system("cp -a ../sim/output/cable_drops/. cable_drops/")
    os.system("cp -a ../sim/output/switch_failures/. switch_failures/")
    os.system("cp -a ../sim/output/cable_failures/. cable_failures/")
    os.system("cp -a ../sim/output/routing_failed_switch/. routing_failed_switch/")
    os.system("cp -a ../sim/output/routing_failed_cable/. routing_failed_cable/")
    os.system("cp -a ../sim/output/switch_degradations/. switch_degradations/")
    os.system("cp -a ../sim/output/cable_degradations/. cable_degradations/")
    os.system("cp -a ../sim/output/random_packet_drops/. random_packet_drops/")

    # RTT Data
    colnames = ["Time", "RTT", "seqno", "ackno", "base", "target"]
    df = pd.DataFrame(columns=["Time", "RTT", "seqno", "ackno", "base", "target"])
    name = ["0"] * df.shape[0]
    df = df.assign(Node=name)

    pathlist = Path("rtt").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        name = [str(path_in_str)] * temp_df.shape[0]
        temp_df = temp_df.assign(Node=name)
        df = pd.concat([df, temp_df])

    base_rtt = df["base"].max()
    target_rtt = df["target"].max()

    if len(df) > 10001:
        ratio = len(df) / 10000
        # DownScale
        df = df.iloc[:: int(ratio)]
        # Reset the index of the new dataframe
        df.reset_index(drop=True, inplace=True)

    # Cwd Data
    colnames = ["Time", "Congestion Window"]
    df2 = pd.DataFrame(columns=colnames)
    name = ["0"] * df2.shape[0]
    df2 = df2.assign(Node=name)
    df2.drop_duplicates("Time", inplace=True)

    pathlist = Path("cwd").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp_df2 = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        name = [str(path_in_str)] * temp_df2.shape[0]
        temp_df2 = temp_df2.assign(Node=name)
        temp_df2.drop_duplicates("Time", inplace=True)
        df2 = pd.concat([df2, temp_df2])
    if len(df2) > 10000:
        ratio = len(df2) / 10000
        # DownScale
        df2 = df2.iloc[:: int(ratio)]
        # Reset the index of the new dataframe
        df2.reset_index(drop=True, inplace=True)

    # Queue Data
    colnames = ["Time", "Queue", "KMin", "KMax"]
    df3 = pd.DataFrame(columns=colnames)
    name = ["0"] * df3.shape[0]
    df3 = df3.assign(Node=name)
    df3.drop_duplicates("Time", inplace=True)

    pathlist = Path("queue_size_normalized").glob("**/*.txt")
    for files in natsort.natsorted(pathlist, reverse=False):
        """pattern = r'^queue_size_normalized/queueUS_\d+-CS_\d+\.txt$'
        if not re.match(pattern, str(files)):
            continue"""

        path_in_str = str(files)
        temp_df3 = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        name = [str(path_in_str)] * temp_df3.shape[0]
        temp_df3 = temp_df3.assign(Node=name)
        temp_df3.drop_duplicates("Time", inplace=True)
        df3 = pd.concat([df3, temp_df3])

    if len(df3) > 10000:
        ratio = len(df3) / 10000
        # DownScale
        df3 = df3.iloc[:: int(ratio)]
        # Reset the index of the new dataframe
        df3.reset_index(drop=True, inplace=True)

    # Switch Failures data
    colnames = ["FailTime", "RecoveryTime"]
    dfSwitchFail = pd.DataFrame(columns=colnames)

    pathlist = Path("switch_failures").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        dfSwitchFail = pd.concat([dfSwitchFail, temp])

    # Cable Failures data
    colnames = ["FailTime", "RecoveryTime"]
    dfCableFail = pd.DataFrame(columns=colnames)

    pathlist = Path("cable_failures").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        dfCableFail = pd.concat([dfCableFail, temp])

    # Packet drops Switch data
    colnames = ["DropTime"]
    dfSwitchDrops = pd.DataFrame(columns=colnames)

    pathlist = Path("switch_drops").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        dfSwitchDrops = pd.concat([dfSwitchDrops, temp])

    # Packet drops Cable data
    colnames = ["DropTime"]
    dfCableDrops = pd.DataFrame(columns=colnames)

    pathlist = Path("cable_drops").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        dfCableDrops = pd.concat([dfCableDrops, temp])

    # Switch Degradations
    colnames = ["Time"]
    dfSwitchDegradations = pd.DataFrame(columns=colnames)

    pathlist = Path("switch_degradations").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        dfSwitchDegradations = pd.concat([dfSwitchDegradations, temp])

    # Switch Degradations
    colnames = ["Time"]
    dfCableDegradations = pd.DataFrame(columns=colnames)

    pathlist = Path("cable_degradations").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        dfCableDegradations = pd.concat([dfCableDegradations, temp])

    # Random Packet Drops
    colnames = ["Time"]
    dfRandomPacketDrops = pd.DataFrame(columns=colnames)

    pathlist = Path("random_packet_drops").glob("**/*.txt")
    for files in sorted(pathlist):
        path_in_str = str(files)
        temp = pd.read_csv(
            path_in_str, names=colnames, header=None, index_col=False, sep=","
        )
        dfRandomPacketDrops = pd.concat([dfRandomPacketDrops, temp])

    print("Finished Parsing")
    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])
    color = [
        "#636EFA",
        "#0511a9",
        "#EF553B",
        "#00CC96",
        "#AB63FA",
        "#FFA15A",
        "#19D3F3",
        "#FF6692",
        "#B6E880",
        "#FF97FF",
        "#FECB52",
    ]
    # Add traces
    max_x = df["Time"].max()

    count = 0
    for i in df["Node"].unique():
        sub_df = df.loc[df["Node"] == str(i)]
        fig.add_trace(
            go.Scatter(
                x=sub_df["Time"],
                y=sub_df["RTT"],
                mode="markers",
                marker=dict(size=2),
                name=str(i),
                line=dict(color=color[0]),
                opacity=0.9,
                showlegend=True,
            ),
            secondary_y=False,
        )

    print("Congestion Plot")
    for i in df2["Node"].unique():
        sub_df = df2.loc[df2["Node"] == str(i)]
        fig.add_trace(
            go.Scatter(
                x=sub_df["Time"],
                y=sub_df["Congestion Window"],
                name="CWD " + str(i),
                line=dict(dash="dot"),
                showlegend=True,
            ),
            secondary_y=True,
        )

    # Queue
    print("Queue Plot")
    count = 0
    df3["Queue"] = pd.to_numeric(df3["Queue"])
    max_ele = df3[["Queue"]].idxmax(1)
    for i in df3["Node"].unique():
        sub_df = df3.loc[df3["Node"] == str(i)]
        if skip_small_value is True and sub_df["Queue"].max() < 500:
            count += 1
            continue

        fig.add_trace(
            go.Scatter(
                x=sub_df["Time"],
                y=sub_df["Queue"],
                name="Queue " + str(i),
                mode="markers",
                marker=dict(size=1.4),
                line=dict(dash="dash", color="black", width=3),
                showlegend=True,
            ),
            secondary_y=False,
        )
        count += 1

    fig.add_trace(
        go.Scatter(
            x=dfSwitchFail["FailTime"] / 1000,
            y=[7000] * len(dfSwitchFail),
            mode="markers",
            marker_symbol="cross",
            name="Switch Fail",
            marker=dict(size=2, color="red"),
            showlegend=True,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=dfSwitchDegradations["Time"] / 1000,
            y=[6500] * len(dfSwitchDegradations),
            mode="markers",
            marker_symbol="diamond",
            name="Switch Degradation",
            marker=dict(size=2, color="red"),
            showlegend=True,
        ),
        secondary_y=False,
    )

    fig.add_trace(
        go.Scatter(
            x=dfSwitchDrops["DropTime"] / 1000,
            y=[6000] * len(dfSwitchDrops),
            mode="markers",
            marker_symbol="circle",
            name="Switch Packet drop",
            marker=dict(size=2, color="red"),
            showlegend=True,
        ),
        secondary_y=False,
    )

    fig.add_trace(
        go.Scatter(
            x=dfCableFail["FailTime"] / 1000,
            y=[5000] * len(dfCableFail),
            mode="markers",
            marker_symbol="cross",
            name="Cable Fail",
            marker=dict(size=2, color="blue"),
            showlegend=True,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=dfCableDegradations["Time"] / 1000,
            y=[6500] * len(dfCableDegradations),
            mode="markers",
            marker_symbol="diamond",
            name="Cable Degradation",
            marker=dict(size=2, color="blue"),
            showlegend=True,
        ),
        secondary_y=False,
    )

    fig.add_trace(
        go.Scatter(
            x=dfCableDrops["DropTime"] / 1000,
            y=[4500] * len(dfCableDrops),
            mode="markers",
            marker_symbol="circle",
            name="Cable Packet drop",
            marker=dict(size=2, color="blue"),
            showlegend=True,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=dfRandomPacketDrops["Time"] / 1000,
            y=[4500] * len(dfRandomPacketDrops),
            mode="markers",
            marker_symbol="circle",
            name="Random Packet drop",
            marker=dict(size=2, color="pink"),
            showlegend=True,
        ),
        secondary_y=False,
    )

    # Add figure title
    fig.update_layout(title_text=title)

    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=target_rtt,  # Y-coordinate
        y1=target_rtt,  # Y-coordinate
        line=dict(color="black", dash="dash"),
    )

    print("Done Plotting")
    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=target_rtt
        + 250,  # Set the y-coordinate to match the dashed line's y-coordinate
        text="Target RTT",  # The text label you want to display
        showarrow=False,  # No arrow pointing to the label
        font=dict(size=12, color="black"),  # Customize the font size and color
    )

    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=base_rtt,  # Y-coordinate
        y1=base_rtt,  # Y-coordinate
        line=dict(color="black", dash="dash"),
    )

    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=base_rtt
        + 250,  # Set the y-coordinate to match the dashed line's y-coordinate
        text="Base RTT",  # The text label you want to display
        showarrow=False,  # No arrow pointing to the label
        font=dict(size=12, color="black"),  # Customize the font size and color
    )

    config = {
        "toImageButtonOptions": {
            "format": "pdf",  # one of png, svg, jpeg, webp
            "filename": "custom_image",
            "height": 550,
            "width": 1000,
            "scale": 4,  # Multiply title/legend/axis/canvas sizes by this factor
        }
    }

    # Set x-axis title
    fig.update_xaxes(title_text="Time (ns)")
    # Set y-axes titles
    fig.update_yaxes(title_text="RTT || Queuing Latency (ns)", secondary_y=False)
    fig.update_yaxes(title_text="Congestion Window (B)", secondary_y=True)

    now = datetime.now()  # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    print("Saving Plot")
    # fig.write_image("out/fid_simple_{}.png".format(date_time))
    # plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    print("Showing Plot")
    # save plot as pdf file without legend
    return fig


if __name__ == "__main__":

    os.system("rm -rf plots/")
    os.system("mkdir plots/")

    strings_to_run = [
        "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -strat reps -use_freezing_reps  -end_time 10 -bonus_drop 1.5 -nodes 1024 -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 700 -reuse_entropy 1 -topo topologies/fat_tree_1024_8os_800.topo -tm connection_matrices/incast_1024_32_4MiB -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/degrade_one_cable.txt > out.tmp",
        "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -strat reps_circular -use_freezing_reps -end_time 10 -bonus_drop 1.5 -nodes 1024 -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 700 -reuse_entropy 1 -topo topologies/fat_tree_1024_8os_800.topo -tm connection_matrices/incast_1024_32_4MiB -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/degrade_one_cable.txt > out.tmp",
        "./htsim_uec_entry_modern -o uec_entry -algorithm smartt -strat spraying  -use_freezing_reps -end_time 10 -bonus_drop 1.5 -nodes 1024 -number_entropies 256 -q 294 -cwnd 353 -ecn 58 235 -target_rtt_percentage_over_base 50 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 800000 -mtu 4096 -seed 919 -queue_type composite -hop_latency 700 -reuse_entropy 1 -topo topologies/fat_tree_1024_8os_800.topo -tm connection_matrices/incast_1024_32_4MiB -x_gain 1.6 -y_gain 8 -topology normal -w_gain 2 -z_gain 0.8  -collect_data 1 -failures_input ../failures_input/degrade_one_cable.txt > out.tmp",
    ]
    algo = [
        "REPS (With one degraded Cable)",
        "REPS Circular (With one degraded Cable)",
        "Spraying (With one degraded Cable)",
    ]
    experiment = [
        "Incast 32:1",
        "Incast 32:1",
        "Incast 32:1",
        "Incast 32:1",
    ]
    data = [
        "800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows",
        "800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows",
        "800Gpbs - 4KiB MTU - 8:1 Oversubscription - 4MiB Flows",
    ]

    for i in range(0, len(strings_to_run)):
        print("Running: " + algo[i])
        title = algo[i] + " - " + experiment[i] + " - " + data[i]
        fig = run(strings_to_run[i], title)
        fig.update_layout(showlegend=False, width=1024, height=512)
        os.chdir("evaluation_failures/")
        fig.write_image("plots/{}.svg".format(title))
