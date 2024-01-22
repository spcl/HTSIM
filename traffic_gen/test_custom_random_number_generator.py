""" Unit tests for the CustomRandomNumberGenerator class. """

import unittest

from custom_random_number_generator import (
    CustomRandomNumberGenerator,
    CdfDataPoint,
)


class TestCustomRandomNumberGenerator(unittest.TestCase):
    """
    A test case for the CustomRandomNumberGenerator class.

    This test case includes various test methods to test the functionality
    of the CustomRandomNumberGenerator class.
    """

    def setUp(self):
        """
        Set up the test case by creating an instance of CustomRandomNumberGenerator.
        """
        self.generator = CustomRandomNumberGenerator()

    def test_is_valid_cdf(self):
        """
        Test the is_valid_cdf method of CustomRandomNumberGenerator.

        This method tests the functionality of the is_valid_cdf method by passing
        different CDFs and checking the expected results.
        """
        # Test a valid CDF.
        valid_cdf = [
            CdfDataPoint(0, 0),
            CdfDataPoint(1, 50),
            CdfDataPoint(2, 100),
        ]
        self.assertTrue(self.generator.is_valid_cdf(valid_cdf))

        # Test an invalid CDF with non-monotonically increasing x values.
        invalid_cdf = [
            CdfDataPoint(0, 0),
            CdfDataPoint(2, 50),
            CdfDataPoint(1, 100),
        ]
        self.assertFalse(self.generator.is_valid_cdf(invalid_cdf))

        # Test an invalid CDF with non-monotonically increasing y values.
        invalid_cdf = [
            CdfDataPoint(0, 0),
            CdfDataPoint(1, 50),
            CdfDataPoint(2, 50),
        ]
        self.assertFalse(self.generator.is_valid_cdf(invalid_cdf))

        # Test an invalid CDF with a start probability that is not 0.
        invalid_cdf = [
            CdfDataPoint(0, 10),
            CdfDataPoint(1, 50),
            CdfDataPoint(2, 100),
        ]
        self.assertFalse(self.generator.is_valid_cdf(invalid_cdf))

        # Test an invalid CDF with an end probability that is not 100.
        invalid_cdf = [
            CdfDataPoint(0, 0),
            CdfDataPoint(1, 50),
            CdfDataPoint(2, 60),
        ]
        self.assertFalse(self.generator.is_valid_cdf(invalid_cdf))

    def test_set_cdf(self):
        """
        Test the set_cdf method of CustomRandomNumberGenerator.

        This method tests the functionality of the set_cdf method by setting
        different CDFs and checking the expected results.
        """
        # Test setting a valid CDF.
        valid_cdf = [
            CdfDataPoint(0, 0),
            CdfDataPoint(1, 50),
            CdfDataPoint(2, 100),
        ]
        self.assertTrue(self.generator.set_cdf(valid_cdf))
        self.assertEqual(self.generator.cdf, valid_cdf)

        # Test setting an invalid CDF.
        invalid_cdf = [
            CdfDataPoint(0, 0),
            CdfDataPoint(2, 50),
            CdfDataPoint(1, 100),
        ]
        self.assertFalse(self.generator.set_cdf(invalid_cdf))
        self.assertNotEqual(self.generator.cdf, invalid_cdf)

    def test_calculate_average_value(self):
        """
        Test the calculate_average_value method of CustomRandomNumberGenerator.

        This method tests the functionality of the calculate_average_value method
        by setting a CDF and checking the expected average value.
        """
        # Test calculating the average value of a CDF with avg = 1.
        cdf = [CdfDataPoint(0, 0), CdfDataPoint(1, 50), CdfDataPoint(2, 100)]
        self.generator.set_cdf(cdf)
        self.assertEqual(self.generator.calculate_average_value(), 1.0)

        # Test calculating the average value of a CDF with avg = 2.
        cdf = [
            CdfDataPoint(0, 0),
            CdfDataPoint(1, 25),
            CdfDataPoint(2, 50),
            CdfDataPoint(3, 75),
            CdfDataPoint(4, 100),
        ]
        self.generator.set_cdf(cdf)
        self.assertEqual(self.generator.calculate_average_value(), 2.0)

    def test_generate_random_number(self):
        """
        Test the generate_random_number method of CustomRandomNumberGenerator.

        This method tests the functionality of the generate_random_number method
        by setting a CDF and generating a random number, then checking if it falls
        within the expected range.
        """
        NUMBER_OF_TRIALS = 1000
        cdf = [CdfDataPoint(0, 0), CdfDataPoint(1, 50), CdfDataPoint(2, 100)]
        self.generator.set_cdf(cdf)
        observed_sum = 0
        for _ in range(NUMBER_OF_TRIALS):
            random_number = self.generator.generate_random_number()
            self.assertGreaterEqual(random_number, 0)
            self.assertLessEqual(random_number, 2)
            observed_sum += random_number
        self.assertAlmostEqual(observed_sum / NUMBER_OF_TRIALS, 1.0, delta=0.1)

    def test_get_value_from_percentile(self):
        """
        Test the get_value_from_percentile method of CustomRandomNumberGenerator.

        This method tests the functionality of the get_value_from_percentile method
        by setting a CDF and checking the expected value for a given percentile.
        """
        # Test getting the value from a percentile.
        cdf = [CdfDataPoint(0, 0), CdfDataPoint(1, 50), CdfDataPoint(2, 100)]
        self.generator.set_cdf(cdf)
        value = self.generator.get_value_from_percentile(75)
        self.assertEqual(value, 1.5)
        value = self.generator.get_value_from_percentile(10)
        self.assertEqual(value, 0.2)

    def test_get_percentile_from_value(self):
        """
        Test the get_percentile_from_value method of CustomRandomNumberGenerator.

        This method tests the functionality of the get_percentile_from_value method
        by setting a CDF and checking the expected percentile for a given value.
        """
        cdf = [CdfDataPoint(0, 0), CdfDataPoint(1, 50), CdfDataPoint(2, 100)]
        self.generator.set_cdf(cdf)
        percentile = self.generator.get_percentile_from_value(1.5)
        self.assertEqual(percentile, 75)
        percentile = self.generator.get_percentile_from_value(0.2)
        self.assertEqual(percentile, 10)


if __name__ == "__main__":
    unittest.main()
