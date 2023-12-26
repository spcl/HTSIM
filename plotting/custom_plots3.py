import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.markers import MarkerStyle

# Data
techniques = ['REPS', 'Oblivious Packet Spraying']
goodput_avg = [730422 / 1000000, 3076269 / 1000000]
goodput_min = [641808 / 1000000, 1929632 / 1000000]
goodput_max = [804550 / 1000000, 4206441 / 1000000]

# Create a bar plot
plt.figure(figsize=(7, 5))
plt.grid()  #just add this

# Bar plot for Goodput Avg
colors = ["#3274a1", "#3a923a"]
ax = sns.barplot(x=techniques, y=goodput_avg, label='Goodput Avg', palette=colors)

# Add black diamond symbols for Goodput Max
# Add black diamond symbols for Goodput Max
for i in range(len(techniques)):
    plt.scatter([i], [goodput_max[i]], marker='D', color='black', s=75, edgecolors='black', linewidths=1)

# Add black dash symbols for Goodput Min
for i in range(len(techniques)):
    plt.scatter([i], [goodput_min[i]], marker='*', color='black', s=75, edgecolors='black', linewidths=1)

ax.set_axisbelow(True)

ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = 14)
ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 14)

ax.set_xticklabels(techniques)

# Set labels and title
plt.xlabel('Load Balancer', fontsize=15)
plt.ylabel('# Packet Drops (Millions)', fontsize=15)
plt.title('Impact of REPS on packet drops during link failure recovery', fontsize=15.7)


plt.text(1.10, 2, 'Min', color='black', ha='center', va='center', fontsize=11)
plt.text(0.1, 0.55, 'Min', color='black', ha='center', va='center', fontsize=11)
plt.text(1.10, 4.2, 'Max', color='black', ha='center', va='center', fontsize=11)
plt.text(0.1, 0.8, 'Max', color='black', ha='center', va='center', fontsize=11)
# Set y-axis limit to finish at 100 Gbps
#plt.ylim(0, 100)

# Hide legend
#ax.get_legend().remove()

# Show the plot
#plt.show()
plt.tight_layout()
plt.savefig("custom3.png", bbox_inches='tight')
plt.savefig("custom3.pdf", bbox_inches='tight')