import csv

input_file = "out.txt"
output_file = "OUTPH.csv"

# Open input and output files
with open(input_file, "r") as f_input, open(output_file, "w", newline="") as f_output:
    # Create CSV writer
    csv_writer = csv.writer(f_output)
    # Write header to CSV file
    csv_writer.writerow(["From", "To", "Size Finished Flow", "Flow Completion time"])
    
    # Iterate through each line in the input file
    for line in f_input:
        # Check if the line contains the desired pattern
        if "Flow Completion time" in line:
            # Split the line into fields based on "-"
            fields = line.strip().split(" - ")
            # Extract the required fields
            from_value = fields[-2].split(" ")[1]
            to_value = fields[-1].split(" ")[1]
            size_value = fields[-3].split(" ")[3]
            time_value = fields[0].split(" ")[-1]
            # Write the extracted values to the CSV file
            csv_writer.writerow([from_value, to_value, size_value, time_value])

print("CSV file has been created successfully!")
