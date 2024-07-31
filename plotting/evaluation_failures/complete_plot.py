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


def run(title,experiment,mb):
    # Clean Data and Copy Data
    # RTT Data
    title = title.replace("\n", "<br>")
    os.chdir("experiments{}/".format(mb) + experiment+"/")
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

    if len(df) > 5000:
        ratio = len(df) / 5000
        # print("RTT ratio: " + str(ratio))
        df = df.iloc[:: int(ratio)]
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
    if len(df2) > 5000:
        ratio = len(df2) / 5000
        # print("CWD ratio: " + str(ratio))
        df2 = df2.iloc[:: int(ratio)]
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

    if len(df3) > 5000:
        ratio = len(df3) / 5000
        # print("Queue ratio: " + str(ratio))
        df3 = df3.iloc[:: int(ratio)]
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
    if len(dfSwitchDrops) > 5000:
        ratio = len(dfSwitchDrops) / 5000
        # print("Switch drop ratio: " + str(ratio))
        dfSwitchDrops = dfSwitchDrops.iloc[:: int(ratio)]
        dfSwitchDrops.reset_index(drop=True, inplace=True)

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
    if len(dfCableDrops) > 5000:
        ratio = len(dfCableDrops) / 5000
        # print("Cable drop ratio: " + str(ratio))
        dfCableDrops = dfCableDrops.iloc[:: int(ratio)]
        dfCableDrops.reset_index(drop=True, inplace=True)


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

    # Cable Degradations
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
    
    if len(dfRandomPacketDrops) > 5000:
        ratio = len(dfRandomPacketDrops) / 5000
        # print("Random drop ratio: " + str(ratio))
        dfRandomPacketDrops = dfRandomPacketDrops.iloc[:: int(ratio)]
        dfRandomPacketDrops.reset_index(drop=True, inplace=True)


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
                name="RTT",
                line=dict(color=color[0]),
                opacity=0.9,
                showlegend=False,
            ),
            secondary_y=False,
        )
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker=dict(size=7),
            name="RTT",
            line=dict(color=color[0]),
            opacity=0.9,
            showlegend=True,
        ),
        secondary_y=False,
    )

    legend = True
    for i in df2["Node"].unique():
        sub_df = df2.loc[df2["Node"] == str(i)]
        fig.add_trace(
            go.Scatter(
                x=sub_df["Time"],
                y=sub_df["Congestion Window"],
                name="CWD",
                line=dict(dash="dot"),
                showlegend=legend,
            ),
            secondary_y=True,
        )
        legend = False

    # Queue
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
                name="Queue",
                mode="markers",
                marker=dict(size=1.4),
                line=dict(dash="dash", color="black", width=3),
                showlegend=False,
            ),
            secondary_y=False,
        )
        count += 1
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            name="Queue",
            mode="markers",
            marker=dict(size=7),
            line=dict(dash="dash", color="black", width=3),
            showlegend=True,
        ),
        secondary_y=False,
    )

    fig.add_trace(
        go.Scatter(
            x=dfSwitchFail["FailTime"] / 1000,
            y=[7000] * len(dfSwitchFail),
            mode="markers",
            marker_symbol="cross",
            name="Switch fail",
            marker=dict(size=4, color="red"),
            showlegend=False,
        ),
        secondary_y=False,
    )
    
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker_symbol="cross",
            name="Switch fail",
            marker=dict(size=7, color="red"),
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
            name="Switch degradation",
            marker=dict(size=4, color="red"),
            showlegend=False,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker_symbol="diamond",
            name="Switch degradation",
            marker=dict(size=7, color="red"),
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
            name="Switch packet drop",
            marker=dict(size=2, color="red"),
            showlegend=False,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker_symbol="circle",
            name="Switch packet drop",
            marker=dict(size=7, color="red"),
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
            name="Cable fail",
            marker=dict(size=4, color="blue"),
            showlegend=False,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker_symbol="cross",
            name="Cable fail",
            marker=dict(size=7, color="blue"),
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
            name="Cable degradation",
            marker=dict(size=4, color="blue"),
            showlegend=False,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker_symbol="diamond",
            name="Cable degradation",
            marker=dict(size=7, color="blue"),
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
            name="Cable packet drop",
            marker=dict(size=2, color="blue"),
            showlegend=False,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker_symbol="circle",
            name="Cable packet drop",
            marker=dict(size=7, color="blue"),
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
            name="Random packet drop   ",
            marker=dict(size=2, color="pink"),
            showlegend=False,
        ),
        secondary_y=False,
    )
    fig.add_trace(
        go.Scatter(
            x=[None],
            y=[None],
            mode="markers",
            marker_symbol="circle",
            name="Random packet drop   ",
            marker=dict(size=7, color="pink"),
            showlegend=True,
        ),
        secondary_y=False,
    )

    fig.add_shape(
        type="line",
        x0=0,  # Start x-coordinate
        x1=max_x,  # End x-coordinate
        y0=target_rtt,  # Y-coordinate
        y1=target_rtt,  # Y-coordinate
        line=dict(color="black", dash="dash"),
    )

    # Add a text label over the dashed line
    fig.add_annotation(
        x=max_x,  # You can adjust the x-coordinate as needed
        y=target_rtt
        + 300,  # Set the y-coordinate to match the dashed line's y-coordinate
        text="Target RTT",  # The text label you want to display
        showarrow=False,  # No arrow pointing to the label
        font=dict(size=8, color="black"),  # Customize the font size and color
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
        + 300,  # Set the y-coordinate to match the dashed line's y-coordinate
        text="Base RTT",  # The text label you want to display
        showarrow=False,  # No arrow pointing to the label
        font=dict(size=8, color="black"),  # Customize the font size and color
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

    # fig.update_layout(title_text=title, title_x=0.5, title_xanchor='center')
    fig.update_layout(    
        legend=dict(
        orientation="h",
        font=dict(size=10),
        bordercolor='black',
        borderwidth=1,
        x=0.47,
        xanchor='center',
        y=-0.18,
        yanchor='bottom'
    ))

    now = datetime.now()  # current date and time
    date_time = now.strftime("%m:%d:%Y_%H:%M:%S")
    # fig.write_image("out/fid_simple_{}.png".format(date_time))
    # plotly.offline.plot(fig, filename='out/fid_simple_{}.html'.format(date_time))
    # save plot as pdf file without legend
    fig.update_layout(width=1300, height=550)
    fig.update_layout(margin=dict(l=80, r=0, t=40, b=40)  # Adjust as needed
)
    os.chdir("../..")    
    return fig
