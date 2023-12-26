import plotly.express as px
import pandas as pd
from pathlib import Path
import plotly.graph_objs as go
 
# RTT Data
colnames=['Time', 'RTT'] 
df = pd.read_csv('../sim/output/rttuec_0_8(0).txt', names=colnames, header=None, index_col=False, sep=',')
name = ['0'] * df.shape[0]
df = df.assign(Node=name)

pathlist = Path('../sim/output/').glob('**/*.txt')
for files in sorted(pathlist):
    path_in_str = str(files)
    if (int(list(filter(str.isdigit, path_in_str))[0]) == 0):
        continue
    temp_df = pd.read_csv(path_in_str, names=colnames, header=None, index_col=False, sep=',')
    name = [str(int(list(filter(str.isdigit, path_in_str))[0]))] * temp_df.shape[0]
    temp_df = temp_df.assign(Node=name)
    df = pd.concat([df, temp_df])


# Plotting Now
fig = go.Figure()
fig.add_traces(
                 data=px.line(df, x='Time', y='RTT',
                              line_group="Node", color='Node', markers=True)._data)

fig.update_layout(title='RTT Over Time (8:1 incast)', showlegend=True)

# Set x-axis title
fig.update_xaxes(title_text="Time (ns)")
# Set y-axes titles
fig.update_yaxes(title_text="RTT (ns)")
fig.show()
