""" Utility functions for the traffic generator. """

import math
import random


def translate_bandwidth(bandwidth_string: str) -> float:
    """
    Converts a bandwidth string with units (K, M, G) to its value in bits per second.

    Args:
        bandwidth_string (str): The bandwidth string to convert.
                                Should end with 'K', 'M', or 'G' to denote the units.

    Returns:
        float: The bandwidth value in bits per second.

    Raises:
        TypeError: If the input is not a string.
        ValueError: If the input string is not a valid bandwidth format.
    """

    if not isinstance(bandwidth_string, str):
        raise TypeError("Input must be a string")

    unit_multiplier = {"K": 1e3, "M": 1e6, "G": 1e9}
    unit = bandwidth_string[-1]
    if unit.isnumeric():
        multiplier = 1
    elif unit in unit_multiplier:
        multiplier = unit_multiplier[unit]
    else:
        raise ValueError(f"Invalid unit format: {unit}. Must be K, M, or G.")

    value = bandwidth_string[:-1]
    try:
        return float(value) * multiplier
    except ValueError as e:
        raise ValueError(f"Invalid bandwidth format: {bandwidth_string}") from e


def exponential_dist_sample(mean: float) -> float:
    """
    Generates a random number from an exponential distribution with a given mean
    (mean = 1 / lambda).

    Args:
        mean (float): The mean of the exponential distribution (mean = 1 / lambda).
        This mean signifies the average time between events in a Poisson process.

    Returns:
        float: A random number sampled from an exponential distribution with the given mean.
    """
    return -math.log(1 - random.random()) * mean


def get_dst(src_idx: int, number_hosts: int) -> int:
    """Get a random destination host index that is not the same as the source host index.

    Args:
        src_idx (int): The source host index.
        number_hosts (int): The number of hosts.
    Returns:
        int: The destination host index.
    """
    dst_idx = random.randint(0, number_hosts - 1)
    # Ensure sender is not the same as receiver.
    while dst_idx == src_idx:
        dst_idx = random.randint(0, number_hosts - 1)
    return dst_idx
