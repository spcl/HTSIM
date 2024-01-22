# Traffic Generator Script

The `traffic_gen.py` script is designed to simulate network traffic based on a given set of parameters. It utilizes the `custom_random_number_generator` library to generate random numbers following a specified distribution, aiding in the creation of realistic network traffic patterns based on observed data.

## Getting Started

### Prerequisites

Before running the script, ensure you have Python installed on your system. The script is compatible with Python 3.x.

### Usage

To run the `traffic_gen.py` script, navigate to the directory containing the script and execute the following command in the terminal:

```bash
python traffic_gen.py [options]
```

#### Command Line Options

The script accepts several command line options to customize the traffic generation:

- `-n, --nhost` (Required): Number of hosts. Must be an integer larger than 1.
- `-c, --cdf_file_path` (Required): Path to the file containing the traffic size Cumulative Distribution Function (CDF). The file must contain two columns, the first being the traffic size in bytes and the second being the probability of that size occurring as a percentile. The first probability must be 0 and the last must be 100. The file must be sorted in ascending order by the traffic size. **The folder `cdf_files` contains several example CDF files.**
- `-l, --load` (Optional): Percentage of the traffic load relative to network capacity. Default is set to `0.4`.
- `-b, --bandwidth` (Optional): Bandwidth of the host link, specified as 'G' (Gbps), 'M' (Mbps), or 'K' (Kbps) or bits if no unit specified. Default is `100G`.
- `-t, --base_time_s` (Optional): Base time in seconds for the flows to start arriving. Default is `0`.
- `-d, --sim_duration_s` (Optional): Total run time of the simulation in seconds. Default is `1`.
- `-s, --seed` (Optional): Seed for the random number generators. If not specified, the system time is used as the seed.
- `-o, --output_file_path` (Optional): Path for the output file where the results will be saved. Default is `cdf_traffic.txt`.

### Example Command

To run the script with custom values:

```bash
python traffic_gen.py -n 10 -c path/to/cdf_file.txt -l 0.5 -b 10G -t 100 -d 60 -s 42 -o path/to/output.txt
```

This command will start a simulation with 10 hosts, using a specified CDF file, with a load of 50%, a bandwidth of 10 Gbps, starting at 100 seconds, running for 60 seconds, with a random seed of 42, and outputting results to the specified file.

## Output

The script will export the generated traffix to the `output_file_path`` in the following format:

```text
$number_of_nodes
$number_of_connections
$src_node->$dst_node id $flow_id start $start_time_in_seconds size $flow_size_bytes
```

Example:

```text
Nodes 3
Connections 3
1->2 id 1 start 0 size 1000000
0->2 id 2 start 0.1 size 1000000
2->3 id 3 start 0.2 size 1000000
```

## Custom Random Number Generator Library

The `custom_random_number_generator` library is an integral part of the traffic generation script. It provides functionality to generate random numbers based on a predefined CDF, ensuring the traffic pattern simulation is as realistic as possible.

### Features

- Set and test custom CDFs.
- Calculate the average of the CDF.
- Generate random numbers following the defined CDF.
- Determine percentiles and corresponding values based on the CDF.
