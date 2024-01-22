""" A library defining a custom random number generator based on a 
user-defined cumulative distribution function (CDF). """

import random
from collections import namedtuple


class CdfDataPoint(
    namedtuple("CdfDataPoint", ["value", "cumulative_prob_perc"])
):
    """
    A data point in a cumulative distribution function (CDF), which includes
    the value and the cumulative probability (as a percentage).
    """


class CustomRandomNumberGenerator:
    """
    A custom random number generator based on a user-defined cumulative distribution function (CDF).
    Provides functionalities to set and validate the CDF, calculate the average value of the CDF,
    generate random numbers according to the CDF, and determine percentiles and corresponding
    values.
    """

    def __init__(self):
        """
        Initialize the CustomRandomNumberGenerator object.

        Args:
            seed (int, optional): Seed value for the random number generator.
            Defaults to None, which uses the system time as the seed.
        """
        self.cdf: list[CdfDataPoint] = []

    def is_valid_cdf(self, input_cdf: list[CdfDataPoint]) -> bool:
        """
        Validates the provided CDF. A valid CDF should start at 0% and end at 100%,
        with both x (value) and y (cumulative probability percentage) values monotonically
        increasing.

        Args:
            input_cdf (list of tuple): The cumulative distribution function (CDF) to validate.

        Returns:
            bool: True if the CDF is valid, False otherwise.
        """
        first_cumulative_prob = input_cdf[0].cumulative_prob_perc
        last_cumulative_prob = input_cdf[-1].cumulative_prob_perc
        if (
            not input_cdf
            or first_cumulative_prob != 0
            or last_cumulative_prob != 100
        ):
            return False

        for i in range(1, len(input_cdf)):
            curr_val, curr_cumulative_prob_perc = input_cdf[i]
            prev_val, prev_cumulative_prob_perc = input_cdf[i - 1]
            if (
                curr_cumulative_prob_perc <= prev_cumulative_prob_perc
                or curr_val <= prev_val
            ):
                return False

        return True

    def set_cdf(self, input_cdf: list[tuple[float, float]]) -> bool:
        """
        Sets the CDF for the random number generator if the provided CDF is valid.

        Args:
            input_cdf (list of tuple): The cumulative distribution function (CDF) to set.

        Returns:
            bool: True if the CDF is valid and successfully set, False otherwise.
        """
        if self.is_valid_cdf(input_cdf):
            self.cdf = input_cdf
            return True
        return False

    def set_cdf_from_file(self, cdf_file_path: str) -> bool:
        """
        Read the CDF from a file and set it for the random number generator
        if the provided CDF is valid. Expected format of the CDF file:

        <value> 0\n
        <value> <cumulative probability percentage>\n
        ...\n
        <value> 100\n


        Args:
            cdf_file_path (str): The path of the CDF file.

        Returns:
            bool: True if the CDF is valid and successfully set, False otherwise.
        """
        with open(cdf_file_path, "r", encoding="utf-8") as cdf_file:
            cdf_from_file = [
                CdfDataPoint(float(val), float(cumulative_prob_perc))
                for val, cumulative_prob_perc in (
                    line.strip().split() for line in cdf_file
                )
            ]
        return self.set_cdf(cdf_from_file)

    def calculate_average_value(self) -> float:
        """
        Calculates and returns the average value of the CDF.

        Returns:
            float: The average value of the CDF.
        """
        total_average = 0
        prev_value, prev_cumulative_prob = self.cdf[0]

        for curr_value, curr_cumulative_prob in self.cdf[1:]:
            total_average += (
                (curr_value + prev_value)
                / 2.0
                * (curr_cumulative_prob - prev_cumulative_prob)
            )
            prev_value, prev_cumulative_prob = (
                curr_value,
                curr_cumulative_prob,
            )

        return total_average / 100

    def generate_random_number(self) -> float:
        """
        Generates a random number based on the CDF.

        Returns:
            float: A random number generated according to the CDF.
        """
        random_percentage = random.random() * 100
        return self.get_value_from_percentile(random_percentage)

    def get_value_from_percentile(self, percentile: float) -> float:
        """
        Determines the value corresponding to a given percentile based on the CDF.
        Uses linear interpolation to determine the value.

        Args:
            percentile (float): The percentile to find the value for.

        Returns:
            float: The value corresponding to the given percentile, or None if out of range.
        """
        for i in range(1, len(self.cdf)):
            if percentile <= self.cdf[i].cumulative_prob_perc:
                lower_value, lower_percentile = self.cdf[i - 1]
                upper_value, upper_percentile = self.cdf[i]
                return lower_value + (percentile - lower_percentile) * (
                    (upper_value - lower_value)
                    / (upper_percentile - lower_percentile)
                )

    def get_percentile_from_value(self, value: float) -> float:
        """
        Determines the percentile of a given value based on the CDF.

        Args:
            value (float): The value to find the percentile for.

        Returns:
            float: The percentile of the given value based on the CDF, or -1 if out of range.
        """
        if value < 0 or value > self.cdf[-1].value:
            return -1

        for i in range(1, len(self.cdf)):
            if value <= self.cdf[i].value:
                lower_value, lower_percentile = self.cdf[i - 1]
                upper_value, upper_percentile = self.cdf[i]
                return lower_percentile + (
                    (upper_percentile - lower_percentile)
                    / (upper_value - lower_value)
                    * (value - lower_value)
                )
