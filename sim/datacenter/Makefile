CC = g++
CFLAGS = -Wall -std=c++17 -g -Wsign-compare
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined
CFLAGS += -O2 -fPIC
CRT=`pwd`
INCLUDE= -I/$(CRT)/.. -I$(CRT) 
LIB=-L.. 

all: htsim_uec_entry_modern


htsim_uec_entry_modern: main_uec_entry_modern.o firstfit.o ../libhtsim.a vl2_topology.o fat_tree_topology.o fat_tree_interdc_topology.o fat_tree_switch.o fat_tree_interdc_switch.o bcube_topology.o connection_matrix.o oversubscribed_fat_tree_topology.o shortflows.o multihomed_fat_tree_topology.o star_topology.o logsim-interface.o
	$(CC)  $(CFLAGS) $^ $(LIB) -lhtsim -o htsim_uec_entry_modern

main_uec_entry_modern.o: main_uec_entry_modern.cpp ../lgs/logsim.h logsim-interface.h
	$(CC) $(INCLUDE) $(CFLAGS) -c main_uec_entry_modern.cpp 

logsim-interface.o: logsim-interface.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c $< $(LIB)

star_topology.o: star_topology.cpp star_topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c star_topology.cpp 

generic_topology.o: generic_topology.cpp generic_topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c generic_topology.cpp 

shortflows.o: shortflows.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c shortflows.cpp 

connection_matrix.o: connection_matrix.cpp bcube_topology.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c connection_matrix.cpp 

firstfit.o: firstfit.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c firstfit.cpp

vl2_topology.o: vl2_topology.cpp vl2_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c vl2_topology.cpp

bcube_topology.o: bcube_topology.cpp bcube_topology.h topology.h ../matrix.h
	$(CC) $(INCLUDE) $(CFLAGS) -c bcube_topology.cpp

camcubetopology.o: camcubetopology.cpp camcubetopology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c camcubetopology.cpp

subflow_control.o: subflow_control.cpp subflow_control.h 
	$(CC) $(INCLUDE) $(CFLAGS) -c subflow_control.cpp

fat_tree_topology.o: fat_tree_topology.cpp fat_tree_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_topology.cpp

fat_tree_interdc_topology.o: fat_tree_interdc_topology.cpp fat_tree_interdc_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_interdc_topology.cpp

main_waterfill.o: main_waterfill.cpp connection_matrix.h connection_matrix.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c main_waterfill.cpp

dragon_fly_topology.o: dragon_fly_topology.cpp dragon_fly_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c dragon_fly_topology.cpp

oversubscribed_fat_tree_topology.o: oversubscribed_fat_tree_topology.cpp oversubscribed_fat_tree_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c oversubscribed_fat_tree_topology.cpp

multihomed_fat_tree_topology.o: multihomed_fat_tree_topology.cpp multihomed_fat_tree_topology.h topology.h
	$(CC) $(INCLUDE) $(CFLAGS) -c multihomed_fat_tree_topology.cpp

fat_tree_switch.o: fat_tree_switch.h fat_tree_switch.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_switch.cpp

fat_tree_interdc_switch.o: fat_tree_interdc_switch.h fat_tree_interdc_switch.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -c fat_tree_interdc_switch.cpp

clean:	
	rm -f *.o htsim_ndp* htsim_swift* htsim_tcp* htsim_dctcp* htsim_roce* htsim_hpcc* htsim_bbr*
