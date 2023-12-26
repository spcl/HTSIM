import re
import numpy as np
import matplotlib.pyplot as plt

num_hosts = 16
list_hosts = [None] * num_hosts
list_hosts_completion_time = [None] * num_hosts

with open("input/out.tmp") as file:
    for line in file:
        result = re.search(r"Time at Step (\d+): (\d+)", line)
        if result:
            id_host = int(result.group(1))
            time_value = int(result.group(2))
            if (list_hosts[id_host] is None):
                list_hosts[id_host] = []
                list_hosts_completion_time[id_host] = []
                list_hosts[id_host].append(time_value)
            else:
                list_hosts[id_host].append(time_value)

print(list_hosts)
for idx, val in enumerate(list_hosts):
    for idx_ind, val_ind in enumerate(val):
        if (idx_ind == 0):
            list_hosts_completion_time[idx].append(int(val_ind / 1000/1000))
            continue
        list_hosts_completion_time[idx].append(int((list_hosts[idx][idx_ind] - list_hosts[idx][idx_ind - 1])/1000/1000))
print()
print(list_hosts_completion_time)


category_names = ['Strongly disagree', 'Disagree',
                  'Neither agree nor disagree', 'Agree', 'Strongly agree']
results = {}

for idx, val in enumerate(list_hosts_completion_time):
    results['Rank {}'.format(idx)] = val



labels = list(results.keys())
data = np.array(list(results.values()))
data_cum = data.cumsum(axis=1)
category_colors = plt.get_cmap('RdYlGn')(
    np.linspace(0.15, 0.85, data.shape[1]))

fig, ax = plt.subplots(figsize=(9.2, 5))
ax.invert_yaxis()
ax.xaxis.set_visible(False)
ax.set_xlim(0, np.sum(data, axis=1).max())

for i, color in enumerate(category_colors):
    widths = data[:, i]
    starts = data_cum[:, i] - widths
    ax.barh(labels, widths, left=starts, height=0.5,
            label="", color=color)
    xcenters = starts + widths / 2

    r, g, b, _ = color
    text_color = 'white' if r * g * b < 0.5 else 'darkgrey'
    for y, (x, c) in enumerate(zip(xcenters, widths)):
        ax.text(x, y, str(int(c)), ha='center', va='center',
                color=text_color)

plt.savefig('foo.png', bbox_inches='tight')
plt.show()

