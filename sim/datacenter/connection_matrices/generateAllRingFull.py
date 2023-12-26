import os

# Incast Normal
data_size = [200000, 40000]
window_size = [1, 8, 16]
topology_size = [128]

folder_name="ring_full_tm_128"
command="rm -r {}".format(folder_name)
os.system(command)
command="mkdir {}".format(folder_name)
os.system(command)


for topology_element in topology_size:
    for datasize in data_size:
        file_name = "ring_{}_{}.bin".format(topology_element, datasize)

        string_run = "python3 gen_allreduce.py {}/{} 128 128 128 {} 0 0".format(folder_name, file_name, datasize)
        print(string_run)
        os.system(string_run)