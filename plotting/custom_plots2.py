import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.markers import MarkerStyle

# Data
techniques = ['REPS', 'Oblivious Packet Spraying']
goodput_avg = [73.90, 49.14]
goodput_min = [70.76, 48.56]
goodput_max = [78.64, 51.29]

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



# Add a horizontal line at 75 Gbps with annotation
plt.axhline(y=75, color='gray', linestyle='--', label='Ideal Fair Share', linewidth=2.5)
plt.text(1, 72, 'Ideal fair share of goodput', color='black', ha='center', va='center', fontsize=13)

ax.set_axisbelow(True)

ax.set_xticklabels([str(i) for i in ax.get_xticks()], fontsize = 14)
ax.set_yticklabels([str(round(i,1)) for i in ax.get_yticks()], fontsize = 14)
ax.set_xticklabels(techniques)
ax.set_yticklabels( ax.get_yticks().astype(int))

# Set labels and title
plt.xlabel('Load Balancer', fontsize=15)
plt.ylabel('Per-Flow Goodput (Gb/s)', fontsize=15)
plt.title('Impact of REPS in asymmetric networks', fontsize=15.7)


plt.text(1.10, 46.5, 'Min', color='black', ha='center', va='center', fontsize=11)
plt.text(0.1, 69, 'Min', color='black', ha='center', va='center', fontsize=11)
plt.text(1.10, 53, 'Max', color='black', ha='center', va='center', fontsize=11)
plt.text(0.1, 78, 'Max', color='black', ha='center', va='center', fontsize=11)
# Set y-axis limit to finish at 100 Gbps
#plt.ylim(0, 100)

# Hide legend
#ax.get_legend().remove()

# Show the plot
plt.tight_layout()
plt.savefig("custom2.png", bbox_inches='tight')
plt.savefig("custom2.pdf", bbox_inches='tight')
