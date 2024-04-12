import os

to_plot1 = "python3 performance_complete.py"
to_plot2 = "python3 performance_rate.py"


os_ratio = 1
exp_name = "incast_inter_dc_large"

to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm intersmartt -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 2 -kmax 80 -use_fast_increase 1 -use_super_fast_increase 1 -fast_drop 1 -linkspeed 20000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data 1 -use_pacing 1 -use_phantom 1 -phantom_slowdown 1 -phantom_size 2600000 -decrease_on_nack 0 -topology interdc -max_queue_size 175000 -interdc_delay 500000 -phantom_both_queues -stop_after_quick > out3.tmp".format(os_ratio, exp_name)
os.system(to_run)
print(to_run)
os.system(to_plot1)
os.system(to_plot2) 

to_run = "../sim/datacenter/htsim_uec_entry_modern -o uec_entry -k 1 -algorithm mprdma -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -number_entropies 1024 -kmin 20 -kmax 80 -use_fast_increase 0 -use_super_fast_increase 1 -fast_drop 0 -linkspeed 20000 -mtu 4096 -seed 15 -queue_type composite -hop_latency 1000 -switch_latency 0 -reuse_entropy 1 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -x_gain 4 -y_gain 0 -w_gain 0 -z_gain 4 -bonus_drop 1.0 -collect_data 1 -use_pacing 0 -use_phantom 0 -phantom_slowdown 3 -phantom_size 2600000 -decrease_on_nack 0 -topology interdc -max_queue_size 175000 -interdc_delay 500000 > out3.tmp".format(os_ratio, exp_name)
print(to_run)
os.system(to_run)
os.system(to_plot1)

to_run = "../sim/datacenter/htsim_ndp_entry_modern -o uec_entry -k 1 -nodes 16 -q 4452000 -strat ecmp_host_random2_ecn -linkspeed 20000 -mtu 4096 -seed 15 -hop_latency 1000 -switch_latency 0 -os_border {} -tm ../sim/datacenter/connection_matrices/{} -collect_data 1 -topology interdc  -max_queue_size 175000 -interdc_delay 500000 -cwnd 2500000 > out3.tmp".format(os_ratio, exp_name)
os.system(to_run)
os.system(to_plot1)