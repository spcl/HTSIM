import os

# Incast Normal
data_size = [131072, 524288, 1048576]
window_size = [1, 2, 8, 16]
topology_size = [128]

folder_name="alltoall_normal_tm_16"
command="rm -r {}".format(folder_name)
os.system(command)
command="mkdir {}".format(folder_name)
os.system(command)

for window_alltoall in window_size:
    for topology_element in topology_size:
        for datasize in data_size:
            file_name = "alltoall_{}_{}_{}.bin".format(topology_element, datasize, window_alltoall)

            string_run = "python3 gen_serialn_alltoall.py {}/{} {} {} {} {} {} 0 0".format(folder_name, file_name, topology_element,topology_element,topology_element,window_alltoall, datasize)
            print(string_run)
            os.system(string_run)


'''
# AllToAll Unbalanced
folder_name="alltoall_unbalanced_16"
command="rm -r {}".format(folder_name)
os.system(command)
command="mkdir {}".format(folder_name)
os.system(command)
window_size = [1, 8, 16]
data_size = [4000, 512000]
for window_alltoall in window_size:
    for topology_element in topology_size:
        for datasize in data_size:
            file_name = "alltoallu_{}_{}_{}.bin".format(topology_element, datasize, window_alltoall)

            string_run = "python3 schedgen.py alltoall --unbalanced --algorithm=windowed --window_size={} --datasize={} --comm_size={}  --output={}/{}".format(window_alltoall, datasize, topology_element,folder_name, file_name)
            print(string_run)
            os.system(string_run)


# Multi AllToAll
folder_name="multi_alltoall"
command="rm -r {}".format(folder_name)
os.system(command)
command="mkdir {}".format(folder_name)
os.system(command)
window_size = [8]
data_size = [4000, 512000]
for window_alltoall in window_size:
    for topology_element in topology_size:
        for datasize in data_size:
            file_name = "alltoallmulti_{}_{}_{}.bin".format(topology_element, datasize, window_alltoall)

            num_groups = int(topology_element / 16)

            string_run = "python3 schedgen.py multi_alltoall --num_comm_groups=3 --algorithm=windowed --window_size={} --datasize={} --comm_size={}  --output={}/{}".format(num_groups, window_alltoall, datasize,topology_element, folder_name, file_name)
            print(string_run)
            os.system(string_run)

# Multi AllToAll Unbalanced
folder_name="alltoall_unbalanced"
command="rm -r {}".format(folder_name)
os.system(command)
command="mkdir {}".format(folder_name)
os.system(command)
window_size = [8]
data_size = [4000, 512000]
for window_alltoall in window_size:
    for topology_element in topology_size:
        for datasize in data_size:
            file_name = "alltoallu_{}_{}_{}.bin".format(topology_element, datasize, window_alltoall)

            string_run = "python3 schedgen.py  multi_alltoall --num_comm_groups={}  --unbalanced --algorithm=windowed --window_size={} --datasize={} --comm_size=128  --output={}/{}".format(window_alltoall, datasize, folder_name, file_name)
            print(string_run)
            os.system(string_run)'''