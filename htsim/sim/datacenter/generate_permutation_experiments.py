import sys

def generate_experiment(messagesize,linkspeed,paths,mode,oversub,failed):
    ovs = ""
    if (oversub!=1):
        ovs = "_" + str(oversub)+"_to_1"
    
    print ("connection_matrices/perm_8192n_8192c_",messagesize,"MB.cm",sep='')
    print ("!Experiment 8K permutation, 8K leaf-spine, ",linkspeed,"Gbps, ",paths," paths, ",messagesize,"MB messages, ",mode,sep='')
    print ("!Binary ./htsim_uec")
    idealfct = int(messagesize * 8000 / linkspeed + 9) * oversub
    idealfct = int(idealfct * 64 / (64 - failed + failed / 4))
    print ("!Param -end ",max(4*idealfct,1000),sep='')
    print ("!Param -paths ",paths,sep='')
    print ("!Param -linkspeed ",linkspeed,"000",sep='')
    print ("!Param -topo topologies/leaf_spine_8192_",linkspeed,"g",ovs,".topo",sep='')

    if failure>0:
        print ("!Param -failed ",failure,sep='')
    
    if (mode.startswith("NSCC")):
        print ("!Param -sender_cc_only")
    if (mode.startswith("RCCC")):
        print ("!Param -receiver_cc_only")
    elif (mode.startswith("BOTH")):
        print ("!Param -sender_cc")
        print ("!Param -receiver_cc")

    if (mode.endswith("-SLEEK")):
        print ("!Param -sleek")
        
    print ("!tailFCT ",int(idealfct*1.2),sep='')

def generate_set(linkspeed,mode,oversub,failure):
    for msgsize in (1,2,4,8,16,32,64,100):
        for paths in (32,64,128):
            generate_experiment(msgsize,linkspeed,paths,mode,oversub,failure)

#connection_matrices/perm_8192n_8192c_YYMB.cm
#!Experiment 8K permutation, 8K leaf-spine, 200Gbps, XX paths, YYMB messages, Both.
#!Binary ./htsim_uec
#!Param -end 400
#!Param -paths XX
#!Param -linkspeed 200000
#!Param -topo topologies/leaf_spine_8192_200g.topo
#!tailFCT 60


n = len(sys.argv)
i = 1;

print(sys.argv)

if (n<3):
    print("Expected arguments not supplied.")
    print(" Please either specify linkspeed [e.g. 200] and algorithm [NSCC, RCCC or BOTH]; optional argument is oversub ratio (4 and 8 supported); another optional argument is link failure count (applied per rack).")
    print(" Or provide all <output prefix> to generate a complete set of experiments.")
    sys.exit()
elif (sys.argv[1] == "all" and len(sys.argv[2]) > 2):
    sys_stdout = sys.stdout
    filename_prefix = sys.argv[2]
    for mode in ["NSCC","RCCC","BOTH","NSCC-SLEEK","BOTH-SLEEK"]:
        for linkspeed in [200, 400, 800]:
            for oversub in [1,4,8]:
                for failure in [0, 1]:
                    os_name = ""
                    if oversub > 1:
                        os_name = f"_{oversub}_to_1"
                    fail_name = ""
                    if failure > 0:
                        fail_name = f"_fail{failure}"
                    filename = f"{filename_prefix}_{linkspeed}g_{mode.lower().replace('-','_')}{os_name}{fail_name}.txt"

                    sys.stdout = sys_stdout
                    print(f"Writing {filename}")
                    with open(filename, 'x') as sys.stdout:
                        generate_set(linkspeed,mode,oversub,failure)
                        sys.stdout.flush()
else:
    linkspeed = int(sys.argv[1])
    mode = sys.argv[2]

    oversub = 1
    if (n>3):
        oversub = int(sys.argv[3])

    failure = 0
    if (n>4):
        failure = int(sys.argv[4])

    if linkspeed not in (200,400,800):
        print ("Supported linkspeeds are 200, 400 and 800, you supplied ", linkspeed)
        sys.exit()

    if mode not in {"NSCC","RCCC","BOTH","NSCC-SLEEK","BOTH-SLEEK"}:
        print ("Supported modes are NSCC, RCCC or BOTH, but you supplied ", mode)
        sys.exit()

    if oversub not in (1,4,8):
        print ("Oversub ratio can be 1,4 or 8, but you supplied ", oversub)
        sys.exit()

    if failure<0 or failure>64:
        print ("Failure can be in interval 0-64, but you supplied ", failure)

    generate_set(linkspeed,mode,oversub,failure)