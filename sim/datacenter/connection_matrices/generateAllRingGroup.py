import os

# Incast Normal
data_size = [200000, 40000]
locality = [0, 1]
topology_size = [128]

folder_name="ring_group_tm_128"
command="rm -r {}".format(folder_name)
os.system(command)
command="mkdir {}".format(folder_name)
os.system(command)

for loc in locality:
    for topology_element in topology_size:
        for datasize in data_size:
            file_name = "ring_{}_{}_{}.bin".format(topology_element, datasize, loc)

            string_run = "python3 gen_allreduce.py {}/{} 128 128 32 {} {} 0".format(folder_name, file_name, datasize, loc)
            print(string_run)
            os.system(string_run)