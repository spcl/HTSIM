""" Unit tests for traffic_gen_utils.py """

import unittest

from traffic_gen_utils import (
    exponential_dist_sample,
    get_dst,
    translate_bandwidth,
)


class TestTranslateBandwidth(unittest.TestCase):
    """
    A test case for the translate_bandwidth function.
    """

    def test_valid_bandwidth_string(self):
        """
        Test the translate_bandwidth function with different valid bandwidth strings.
        """
        self.assertEqual(translate_bandwidth("100K"), 100000)
        self.assertEqual(translate_bandwidth("1M"), 1000000)
        self.assertEqual(translate_bandwidth("2.5G"), 2500000000)
        self.assertEqual(translate_bandwidth("1.3"), 1, 3)

    def test_invalid_bandwidth_string(self):
        """
        Test the translate_bandwidth function with invalid bandwidth strings.
        """
        with self.assertRaises(ValueError):
            translate_bandwidth("10X")  # Invalid unit
        with self.assertRaises(ValueError):
            translate_bandwidth("1.5KB")  # Unsupported unit

    def test_non_string_input(self):
        """
        Test the translate_bandwidth function with non-string input.
        """
        with self.assertRaises(TypeError):
            translate_bandwidth(100)  # Integer input
        with self.assertRaises(TypeError):
            translate_bandwidth(["1M"])  # List input


class TestExponentialDistSample(unittest.TestCase):
    """
    Unit tests for the exponential_dist_sample function.
    """

    def test_exponential_dist_sample(self):
        """
        Test the exponential_dist_sample function outputs samples that are positive and
        with the correct mean.
        """
        TOTAL_SAMPLES = 1000
        mean = 1.3
        observed_sum = 0
        for _ in range(TOTAL_SAMPLES):
            sample = exponential_dist_sample(mean)
            self.assertGreater(sample, 0.0)
            observed_sum += sample
        self.assertAlmostEqual(observed_sum / TOTAL_SAMPLES, mean, delta=0.1)


class TestGetDst(unittest.TestCase):
    """
    Test case for the get_dst function.
    """

    def test_get_dst(self):
        """
        Test the get_dst function with different source idx values
        and make sure the destination idx is not the same as the source idx
        and is within the [0, number_hosts) range.
        """
        NUMBER_OF_SAMPLES = 100
        number_hosts = 10
        for src_idx in range(number_hosts):
            for _ in range(NUMBER_OF_SAMPLES):
                dst_idx = get_dst(src_idx, number_hosts)
                self.assertIsInstance(dst_idx, int)
                self.assertNotEqual(dst_idx, src_idx)
                self.assertGreaterEqual(dst_idx, 0)
                self.assertLess(dst_idx, number_hosts)


if __name__ == "__main__":
    unittest.main()
