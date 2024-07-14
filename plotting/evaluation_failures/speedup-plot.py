import pandas as pd

# Initialize an empty list to store data
data = []

# Example data to append
experiments = ['Exp1', 'Exp2', 'Exp3']
reps = [10, 15, 20]
reps_circular = [5, 8, 12]
reps_circular_without_freezing = [2, 4, 6]
spraying = ['Yes', 'No', 'Yes']

# Populate the list with dictionaries
for i in range(len(experiments)):
    data.append({
        'ExperimentName': experiments[i],
        'REPS': reps[i],
        'REPS Circular': reps_circular[i],
        'REPS Circular\n without freezing': reps_circular_without_freezing[i],
        'Spraying': spraying[i]
    })

# Create the DataFrame from the list of dictionaries
df = pd.DataFrame(data)

print("DataFrame:")
print(df)