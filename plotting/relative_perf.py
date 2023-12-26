import sys
import plotly.express as px
import pandas as pd
from pathlib import Path
import plotly.graph_objs as go
import re

completion_times = []
best_time = int(sys.argv[2])
pathlist = Path('result/' + str(sys.argv[1]) + "/").glob('**/*.tmp')
for files in sorted(pathlist):
    path_in_str = str(files)
    with open(files) as file:
        for line in file:
            result = re.search(r"Maximum finishing time at host (\d+): (\d+)", line)
            if result:
                runtime = int(result.group(2))
                string = path_in_str
                pattern = r'[-/](-?\d+)\.tmp'
                result = re.search(pattern, string)
                if result:
                    entropies = int(result.group(1))
                    completion_times.append({str(entropies):(runtime / 1000) / best_time})
                    
completion_times.append({str(-2):(best_time / best_time)})
print(completion_times)
df = pd.DataFrame([(key, value) for d in completion_times for key, value in d.items()], columns=['Entropies', 'Time'])
df = df.sort_values('Entropies', key=lambda x: pd.to_numeric(x, errors='coerce'))
df = df.reset_index(drop=True)
df.at[0,'Entropies']='Theoretical\nBest'
df.at[1,'Entropies']='Random\nPath'

df['category'] = [str(i) for i in df.index]
color_discrete_sequence = ['#609cd4']*len(df)
color_discrete_sequence[0] = '#ec7c34'
color_discrete_sequence[1] = '#800000'
print(df)
print(df.columns[1])
print(sys.argv[2])
fig = px.bar(df, y = 'Time', x = 'Entropies', color = 'category', color_discrete_sequence=color_discrete_sequence, text_auto=True, labels={
                     "Time": "Relative Performance compared to Best",
                     "Entropies": "Number of Entropies Considered",
                 }, title = "Performance with different amount of entropies")
fig.show()
fig.write_image("output{}.png".format(str(sys.argv[1])))