#! /usr/bin/env python3

""" A script to generate traffic based on a given empirical CDF. """

import argparse
import random
import sys
from collections import namedtuple

from custom_random_number_generator import CustomRandomNumberGenerator
from traffic_gen_utils import (
    exponential_dist_sample,
    get_dst,
    translate_bandwidth,
)

# Default argument values.
DEFAULT_LOAD = 0.4
DEFAULT_BANDWIDTH = "100G"
DEFAULT_BASE_TIME_S = 0
DEFAULT_DURATION_S = 1
DEFAULT_OUTPUT_FILE_PATH = "cdf_traffic.txt"
DEFAULT_SEED = None

# Constants.
NS_IN_S = 1e9
BYTE_TO_BIT = 8.0


class Flow(
    namedtuple("Flow", ["src_idx", "dst_idx", "size_bytes", "start_time_s"])
):
    """
    A network flow, which includes the source host idx, destination host idx,
    size of the flow in bytes, and the flow start time in s.
    """


def add_commandline_options():
    """
    Create an argument parser and add command line options to the parser.

    Returns:
        argparse.ArgumentParser: The argument parser object with added command line options.
    """
    arg_parser = argparse.ArgumentParser(
        description="Command line options for the application"
    )
    # Required options.
    arg_parser.add_argument(
        "-n",
        "--nhost",
        required=True,
        type=int,
        help="(Required) The number of hosts, must be larger than 1.",
    )
    # Optional options.
    arg_parser.add_argument(
        "-c",
        "--cdf_file_path",
        required=True,
        type=str,
        help="(Required) The path of the file with the traffic size cdf.",
    )
    arg_parser.add_argument(
        "-l",
        "--load",
        default=DEFAULT_LOAD,
        type=float,
        help=(
            "The percentage of the traffic load to the network capacity, by"
            f" default {DEFAULT_LOAD}"
        ),
    )
    arg_parser.add_argument(
        "-b",
        "--bandwidth",
        default=DEFAULT_BANDWIDTH,
        type=str,
        help=(
            "The bandwidth of host link (G/M/K), by default"
            f" {DEFAULT_BANDWIDTH}."
        ),
    )
    arg_parser.add_argument(
        "-t",
        "--base_time_s",
        default=DEFAULT_BASE_TIME_S,
        type=float,
        help=(
            "The base time for the flows to start arriving (in s), by default"
            f" {DEFAULT_BASE_TIME_S}."
        ),
    )
    arg_parser.add_argument(
        "-d",
        "--sim_duration_s",
        default=DEFAULT_DURATION_S,
        type=float,
        help=f"The total run time (in s), by default {DEFAULT_DURATION_S}.",
    )
    arg_parser.add_argument(
        "-s",
        "--seed",
        default=DEFAULT_SEED,
        type=float,
        help=(
            "The seed for the random number generators, by default None which"
            " means using the system time."
        ),
    )
    arg_parser.add_argument(
        "-o",
        "--output_file_path",
        default=DEFAULT_OUTPUT_FILE_PATH,
        type=str,
        help=(
            "The path for the output file, by default"
            f" {DEFAULT_OUTPUT_FILE_PATH}."
        ),
    )
    return arg_parser


def connection_matrix_line(flow: Flow, flow_id: int):
    """
    Return a string representation of the flow. Format:

    $src_idx->$dst_idx id $id start $start size $size
    e.g. 14->9 id 3 start 0 size 1000000

    Args:
        flow (Flow): The flow to be represented.
        flow_id (int): The id of the flow. Flow id starts from 1.
    """
    return (
        f"{flow.src_idx}->{flow.dst_idx} id {flow_id} start"
        f" {flow.start_time_s} size {flow.size_bytes}"
    )


def export_flows(num_hosts: int, flow_list: list[Flow], output_file_path: str):
    """
    Export the flow list to the output_file_path with the desired format.
    Format (start time in seconds, flow size in bytes, flow id starts from 1)

    Nodes 16\n
    Connections 16\n
    1->13 id 1 start 0 size 1000000\n
    9->2 id 2 start 0.1 size 1000000\n
    14->9 id 3 start 0.2 size 1000000

    Args:
        num_hosts (int): The number of hosts.
        flow_list (list[Flow]): A list of flows to be exported.
        output_file_path (str): The path of the output file.
    """
    with open(output_file_path, "w", encoding="utf-8") as ofile:
        ofile.write(f"Nodes {num_hosts}\nConnections {len(flow_list)}\n")
        for flow_id, flow in enumerate(flow_list):
            ofile.write(connection_matrix_line(flow, flow_id + 1))
            ofile.write("\n")


def main():
    """The main function of the script generating traffic based on a given empirical CDF."""
    # Parse command line arguments.
    parser = add_commandline_options()
    args = parser.parse_args()
    nhost = args.nhost
    load = args.load
    base_time_ns = args.base_time_s * NS_IN_S
    sim_duration_ns = args.sim_duration_s * NS_IN_S
    output_file_path = args.output_file_path
    cdf_file_path = args.cdf_file_path
    seed = args.seed

    # Argument validation.
    if not args.nhost or args.nhost < 2:
        sys.exit(
            "Please use -n to enter the number of hosts and enter a number"
            " larger than 1."
        )
    try:
        bandwidth_bps = translate_bandwidth(args.bandwidth)
    except (ValueError, TypeError) as e:
        sys.exit(f"Bandwidth format incorrect: {e}")

    # Create a custom random generator and set the seed.
    custom_rand = CustomRandomNumberGenerator()
    random.seed(seed)

    # Read the CDF file.
    if not custom_rand.set_cdf_from_file(cdf_file_path):
        sys.exit("Error: Not a valid CDF.")

    # Calculate the average inter-arrival time (in ns).
    avg_msg_size_bits = custom_rand.calculate_average_value() * BYTE_TO_BIT
    avg_load_bps = bandwidth_bps * load
    avg_inter_arrival_time_ns = (avg_msg_size_bits / avg_load_bps) * NS_IN_S

    # Generate flows.
    flow_list: list[Flow] = []
    for src_idc in range(nhost):
        flow_start_time_ns = base_time_ns + int(
            exponential_dist_sample(avg_inter_arrival_time_ns)
        )
        while flow_start_time_ns < base_time_ns + sim_duration_ns:
            dst_idx_ = get_dst(src_idc, nhost)
            flow_size_bytes = max(int(custom_rand.generate_random_number()), 1)
            flow_list.append(
                Flow(
                    src_idc,
                    dst_idx_,
                    flow_size_bytes,
                    flow_start_time_ns / NS_IN_S,
                )
            )
            flow_start_time_ns += int(
                exponential_dist_sample(avg_inter_arrival_time_ns)
            )
    # Sort the flow list by increasing order of start time.
    flow_list.sort(key=lambda flow: flow.start_time_s)

    # Export flow list to file with the desired format.
    export_flows(nhost, flow_list, output_file_path)


if __name__ == "__main__":
    main()
