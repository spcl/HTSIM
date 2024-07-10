import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from scipy.integrate import cumtrapz
import re

# Sample data list
data = []
with open("../sim/datacenter/out2.tmp") as file:
    for line in file:
        result = re.search(r"Flow Completion time is (\d+)", line)
        if result:
            rtt = int(result.group(1))
            data.append(rtt)

# Use seaborn to get the KDE without plotting
kde = sns.kdeplot(data, bw_adjust=0.5, fill=False, legend=False)
x, y = kde.get_lines()[0].get_data()
plt.close()  # Close the plot to not display the KDE plot

# Normalize the y values to get the PDF
y_pdf = y / np.trapz(y, x)

# Integrate the PDF to get the CDF
y_cdf = cumtrapz(y_pdf, x, initial=0)

# Plot only the CDF
plt.plot(x, y_cdf, label='CDF')

# Add labels and title
plt.xlabel('Flow Completion Time (us)')
plt.ylabel('Cumulative Probability')
plt.title('Smoothed CDF Plot')
plt.legend()

# Show the plot
plt.show()
