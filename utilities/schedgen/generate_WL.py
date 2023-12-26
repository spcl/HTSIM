import os


data_size = [4096, 16384, 131072, 524288, 1048576, 4194304, 16777216, 33554432, 67108864, 134217728]
topology_size = [128, 1024]

folder_name_single_flow="../goal_converter/single_flow"
command="rm -r {}".format(folder_name_single_flow)
os.system(command)
command="mkdir {}".format(folder_name_single_flow)
os.system(command)

folder_name_permutation="../goal_converter/permutation"
command="rm -r {}".format(folder_name_permutation)
os.system(command)
command="mkdir {}".format(folder_name_permutation)
os.system(command)

folder_name_tmp="temp"
command="rm -r {}".format(folder_name_tmp)
os.system(command)
command="mkdir {}".format(folder_name_tmp)
os.system(command)


for my_size in data_size:
    for topo in topology_size:
        # Permutation
        os.system("./schedgen -p permutation_across -s {} -d {} -o temp/permutation_{}_{}.goal".format(topo, my_size, topo, my_size))
        os.system("../goal_converter/txt2bin -i temp/permutation_{}_{}.goal -o {}/permutation_{}_{}.bin".format(topo, my_size, folder_name_permutation, topo, my_size))

        # Single Flow
        os.system("./schedgen -p single_flow -s {} -d {} -o temp/single_flow_{}_{}.goal".format(topo, my_size, topo, my_size))
        os.system("../goal_converter/txt2bin -i temp/single_flow_{}_{}.goal -o {}/single_flow_{}_{}.bin".format(topo, my_size, folder_name_single_flow ,topo, my_size))