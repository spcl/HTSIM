import os

to_plot1 = "python3 performance_complete.py"
to_plot2 = "python3 performance_rate.py"
exp_name = "perm32_both"
list_exp = ["paper_incast_inter_large", "paper_incast_inter_small", "paper_incast_intra_large", "paper_incast_mixed_large"]
list_exp = ["perm_128_both", "perm_128_8"]
list_exp = ["paper_incast_inter_large"]


os_ratio = 1
eqds_cwnd = 100000
inter_dc_delay = 500000
phantom_size = 6500000
collect_data_flag = 1
custom_queue_size = 200000

for exp_name in list_exp:
    to_run = "../sim/datacenter/htsim_uec_entry_modern -end_time 1 -o uec_entry -topo ../sim/datacenter/topologies/fat_tree_128_4os.topo -algorithm intersmartt_test -nodes 128 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 50000 -mtu 4096 -seed 125 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data {} -use_pacing 1 -use_phantom 1 -phantom_slowdown 5 -phantom_size 6500000 -decrease_on_nack 0 -topology interdc -max_queue_size {} -interdc_delay {} -phantom_both_queues > out3.tmp".format(os_ratio, exp_name, collect_data_flag, custom_queue_size, inter_dc_delay)
    print(to_run)
    os.system(to_run)
    os.system("python3 performance_complete.py --name {}_PhantomCC".format(exp_name, collect_data_flag, custom_queue_size, inter_dc_delay))
    os.system("python3 rate_plot.py")
    """ to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -topo ../sim/datacenter/topologies/fat_tree_128_4os.topo -algorithm intersmartt_test -enable_bts -nodes 128 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 50000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data {} -use_pacing 1 -use_phantom 1 -phantom_slowdown 5 -phantom_size 6500000 -decrease_on_nack 1 -topology interdc -max_queue_size {} -interdc_delay {} -phantom_both_queues -enable_bts > out3.tmp".format(os_ratio, exp_name, collect_data_flag, custom_queue_size, inter_dc_delay)
    print(to_run)
    os.system(to_run)
    os.system("python3 performance_complete.py --name {}_PhantomCCBTS".format(exp_name, collect_data_flag, custom_queue_size, inter_dc_delay))
    

    to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -topo ../sim/datacenter/topologies/fat_tree_128_4os.topo -algorithm mprdma -nodes 128 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 20 -kmax 80 -use_fast_increase 0 -use_super_fast_increase 1 -fast_drop 0 -linkspeed 50000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data {} -use_pacing 0 -use_phantom 0 -phantom_slowdown 3 -phantom_size 6500000 -decrease_on_nack 1 -topology interdc -max_queue_size {} -interdc_delay {} > out3.tmp".format(os_ratio, exp_name, collect_data_flag, custom_queue_size, inter_dc_delay)
    print(to_run)
    os.system(to_run)
    os.system("python3 performance_complete.py --name {}_MPRDMA".format(exp_name, collect_data_flag, custom_queue_size, inter_dc_delay))

    to_run = "../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -topo ../sim/datacenter/topologies/fat_tree_128_4os.topo -nodes 128 -q 4452000 -strat ecmp_host_random2_ecn -linkspeed 50000 -mtu 4096 -seed 15 -hop_latency 1000 -switch_latency 0 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -collect_data {} -topology interdc  -max_queue_size {} -interdc_delay {} -cwnd 2500000 > out3.tmp".format(os_ratio, exp_name, collect_data_flag, custom_queue_size, inter_dc_delay)
    os.system(to_run)
    os.system("python3 performance_complete.py --name {}_EQDS".format(exp_name, collect_data_flag, custom_queue_size, inter_dc_delay)) """
