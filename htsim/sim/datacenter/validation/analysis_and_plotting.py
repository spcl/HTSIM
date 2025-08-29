import os
import re
import seaborn as sns
import pandas as pd
import matplotlib.pyplot as plt

def get_color_map():
    """
    Returns a dictionary mapping CC Algos to specific colors.
    """
    return {
        'NSCC': '#1f77b4',  # Blue
        'RCCC+DCTCP': '#ff7f0e',  # Orange
        'RCCC+NSCC': '#2ca02c',  # Green
        'RCCC': '#d62728',  # Red
    }

def get_cc_algo_order():
    """
    Returns a list defining the order of CC Algos for plotting.
    """
    return ['NSCC', 'RCCC', 'RCCC+NSCC', 'RCCC+DCTCP']

def get_list_fct(name_file_to_use):
    """
    Extracts the finished-at runtime values from the file.
    """
    temp_list = []
    try:
        with open(name_file_to_use) as file:
            for line in file:
                pattern = r"finished at (\d+)"
                match = re.search(pattern, line)
                if match:
                    actual_fct = float(match.group(1))
                    temp_list.append(actual_fct)
    except FileNotFoundError:
        print(f"File {name_file_to_use} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    return temp_list

def get_num_rtx(name_file_to_use):
    """
    Extracts the number of retransmissions from the file.
    """
    num_rtx = 0
    try:
        with open(name_file_to_use) as file:
            for line in file:
                result = re.search(r"Rtx: (\d+)", line)
                if result:
                    num_rtx = int(result.group(1))
    except FileNotFoundError:
        print(f"File {name_file_to_use} not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    return num_rtx

def get_filenames_from_folder(folder_path, extension=".out"):
    """
    Returns a list of filenames from the folder with a specified extension.
    """
    all_files = [f for f in os.listdir(folder_path) if os.path.isfile(os.path.join(folder_path, f))]
    return [f for f in all_files if f.endswith(extension)]



def format_label(group):
    """
    Converts a raw label into a more readable format.
    Example: 'incast_8to1_1048576B' -> 'Incast 8:1 1MiB'
    """
    ratio = ""
    print(group)
    if 'permutation' in group or 'reduce' in group:
        parts = group.split('_')
        experiment = parts[0].capitalize()
        size_bytes = int(parts[1].replace('B', ''))
    elif 'incast' in group:
        parts = group.split('_')
        experiment = parts[0].capitalize()
        ratio = parts[1].replace('to', ':')
        ratio += ":1"
        size_bytes = int(parts[2].replace('size', '').replace('B', ''))
    elif 'outcast' in group:
        parts = group.split('_')
        experiment = parts[0].capitalize()
        experiment += " / Incast"
        ratio = parts[1].replace('to', ':')
        size_bytes = int(parts[2].replace('size', '').replace('B', ''))
    elif 'alltoallwindowed' in group:
        parts = group.split('_')
        experiment = parts[0].capitalize()
        ratio = parts[1]
        size_bytes = int(parts[2].replace('size', '').replace('B', ''))

    degraded = ""
    
    if ("degrade" in group):
        match = re.search(r"degrade(\d+)", str(group))
        if match:
            degraded = str(match.group(1))
    print(degraded)


    # Convert size from bytes to a human-readable format
    if size_bytes >= 1024**3:
        size = f'{size_bytes // 1024**3}GiB'
    elif size_bytes >= 1024**2:
        size = f'{size_bytes // 1024**2}MiB'
    elif size_bytes >= 1024:
        size = f'{size_bytes // 1024}KiB'
    else:
        size = f'{size_bytes}B'
    
    return f'{experiment} {ratio} {size} {degraded}'

def plot_runtimes(folder_name, folder_name_out, args):
    """
    Plots runtimes of each experiment from the files in the specified folder.
    Adds a Ratio field to the DataFrame if 'incast' is in the experiment name.
    Orders x-axis based on the Ratio field first and then by Size.
    Each unique combination of Experiment and Size will be represented as a separate group on the x-axis.
    Prints the runtime value on top of each bar with color matching the legend.
    """
    data = []
    filenames = get_filenames_from_folder(folder_name)
    
    for filename in filenames:
        file_path = os.path.join(folder_name, filename)
        runtimes = get_list_fct(file_path)
        if not runtimes:
            print(f"No valid runtimes found in file {filename}. Skipping.")
            continue
        
        runtime = max(runtimes)
        parts = filename.split('_')
        experiment = parts[0]
        # Extract the numeric part before 'to' in 'XtoY'
        match = re.search(r"size(\d+)", str(filename))

        if match:
            size = match.group(1)
        if ("nscc_" in filename):
            method = "NSCC"
        elif ("rccc+os_cc" in filename):
            method = "RCCC+DCTCP"
        elif ("nscc+rccc" in filename):
            method = "RCCC+NSCC"
        elif ("rccc" in filename):
            method = "RCCC"

        # Initialize ratio as None
        ratio = None
        window = None
        degraded = None

        
        match = re.search(r"_degrade(\d+)", str(filename))
        if match:
            degraded = str(match.group(1))
    
        if 'incast' in filename:
            # Extract the numeric part before 'to' in 'XtoY'
            match = re.search(r"(\d+)to(\d+)", str(filename))
            if match:
                ratio = int(match.group(1))

        if 'outcast' in filename:
            # Extract the numeric part before 'to' in 'XtoY'
            match = re.search(r"_incast(\d+)", str(filename))
            match2 = re.search(r"_outcast(\d+)", str(filename))
            if match:
                ratio_i = str(match.group(1))
            if match2:
                ratio_o = str(match2.group(1))
            ratio = ratio_i + ":" + ratio_o

        if 'alltoall' in filename:
            # Extract the numeric part before 'to' in 'XtoY'
            match = re.search(r"window(\d+)", str(filename))
            if match:
                ratio_i = int(match.group(1))
            window = int(ratio_i)

        data.append({
            'Experiment': experiment,
            'Size': size,
            'CC Algo': method,
            'Runtime': runtime,
            'Ratio': ratio,
            'Window': window,
            'Degraded': degraded,
        })

    df = pd.DataFrame(data)
    
    # Create a unique identifier for each group combining Experiment and Size
    if ("incast" in df['Experiment'].values):
        df['Group'] = df['Experiment'] + '_' + df['Ratio'].astype(str) + '_' + df['Size']
    elif ("permutation" in df['Experiment'].values or "allreduce" in df['Experiment'].values or "allreduceButterfly" in df['Experiment'].values):   
        df['Group'] = df['Experiment'] + '_' + df['Size']
    elif ("outcast" in df['Experiment'].values): 
        df['Group'] = df['Experiment'] +  '_' + df['Ratio'].astype(str) +'_' + df['Size']
    elif ("alltoallwindowed" in df['Experiment'].values):   
        df['Group'] = df['Experiment'] +  '_' + df['Window'].astype(str) +'_' + df['Size']
    else:
        print("Unknown experiment type. Exiting.")
        exit(1)

    if (df['Degraded'].values is not None):
        df['Group'] = df['Group'] +  '_degrade' + df['Degraded'].astype(str)
    # Ensure 'Size' is numeric for sorting
    df['Size'] = pd.to_numeric(df['Size'], errors='coerce')

    print(df)
    
    # Sort the DataFrame by 'Ratio' and then by 'Size'
    if ("outcast" in df['Experiment'].values):
        df_sorted = df.sort_values(by=['Size'], ascending=[True], na_position='last')
    elif ("alltoallwindowed" in df['Experiment'].values):
        df_sorted = df.sort_values(by=['Window', 'Size'], ascending=[True, True], na_position='last')
    else:
        df_sorted = df.sort_values(by=['Ratio', 'Size'], ascending=[True, True], na_position='last')
    
    # Remove rows with zero runtime if any
    df_sorted = df_sorted[df_sorted['Runtime'] > 0]
    
    # Create the ordered list of groups
    ordered_groups = df_sorted['Group'].unique()
    
    # Format the labels
    formatted_labels = [format_label(group) for group in ordered_groups]
    
    # Create the bar plot using Seaborn
    plt.figure(figsize=(14, 8))
    # Get the color map
    color_map = get_color_map()
    cc_algo_order = get_cc_algo_order()
    print(df_sorted)
    # Ensure 'CC Algo' is a categorical type with the specified order
    df_sorted['CC Algo'] = pd.Categorical(df_sorted['CC Algo'], categories=cc_algo_order, ordered=True)
    ax = sns.barplot(x='Group', y='Runtime', hue='CC Algo', data=df_sorted, errorbar=None, 
                     order=ordered_groups, palette=color_map)
    
    # Add the runtime value on top of each bar with color matching the legend
    for p in ax.patches:
        height = p.get_height()
        color = p.get_facecolor()
        ax.annotate(f'{height:.0f}',  # No decimal numbers
                    (p.get_x() + p.get_width() / 2., height), 
                    ha='center', va='bottom',
                    xytext=(0, 5),  # 5 points vertical offset
                    textcoords='offset points',
                    fontsize=9, color=color,
                    rotation=60)  # Rotate the text to make it less overlapping

    # Customize the plot
    plt.title(f'Runtime of {folder_name_out.replace("size", "topologySize")}')
    plt.xlabel('Experiment and Size')
    plt.ylabel('Runtime (us)')
    
    # Ensure that x-ticks match the number of bars and remove any extras
    ax.set_xticks(range(len(formatted_labels)))
    ax.set_xticklabels(formatted_labels, rotation=45, ha='right')
    
    # Tight layout to avoid clipping
    plt.tight_layout()
    
    # Save and show the plot
    plt.savefig(os.path.join(folder_name_out, "runtime_plot.png"), bbox_inches='tight')
    plt.savefig(os.path.join(folder_name_out, "runtime_plot.pdf"), bbox_inches='tight')
    if args.show_plot:
        plt.show()