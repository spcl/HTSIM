# Sample Python script to read filenames from an input file and launch a process for each filename to count the number of lines in that file.
import os
import numpy as np
import matplotlib.pyplot as plt

import subprocess
import sys
import os
do_process = True
save_file = False

if save_file:
    outpu_dir = './figures/'
    if not os.path.exists(outpu_dir):
        os.makedirs(outpu_dir)
def run_experiments(input_filename):
    # Read the filenames from the input file
    with open(input_filename, 'r') as file:
        inputlines = file.readlines()


    # Remove any whitespace or newlines from the filenames
    i = 0
    new_pkts = {}
    rtx = {}
    rts = {}
    acks = {}

    # Iterate over each filename and launch a process to count its lines
    while i < len(inputlines):
        filename = str(inputlines[i]).rstrip();
        i  = i+1

        if (filename.startswith("#")):
            continue;
        elif (filename.startswith("!")):
            print ("Found parameters when not processing a file!",filename)
            continue;

        experiment_name = ""
        targetTailFCT = 0
        params = []
        targetFCT = {}
        fcts = []
        total_pkts = {}
        throughputs = []
        hold = False
        binary = "./htsim_uec"


        #figure out parameters.
        while i<len(inputlines):
            #print(str(inputlines[i]))
            if (not str(inputlines[i]).startswith("!")):
                #print ("Stopping param parsing")
                break;

            p = str(inputlines[i])
            i = i + 1

            if ("Param" in p):
                params.append(p.split(" ",1)[1])
                #print ("Found param",p.split(" ",1)[1])
            elif ("tailFCT" in p):
                targetTailFCT = int(p.split(" ",1)[1])
                #print ("Found targetTailFCT",targetTailFCT)
            elif ("FCT" in p):
                q = p.split()
                targetFCT[q[1]] = int(q[2])
            elif ("Experiment" in p):
                experiment_name = p.split(" ",1)[1]
            elif 'continue' in p:
                hold = True

        if not os.path.isfile(filename) :
            print ("\n=================================\n!!!!Cannot find traffic matrix file ", filename, "- skipping to next experiment\n================================")
            continue

        cmdline = binary + " -tm "+filename+" "

        for p in params:
            cmdline = cmdline + p.rstrip() + " "

        if (debug):
            print("Cmdline\n",cmdline,"\nTargetTailFCT",targetTailFCT,"\nTargetFCT",targetFCT)

        #finding out number of connections:
        conncountcmd = "grep Connections " + filename + "| awk '{print $2;}'"

        if (debug):
            print (conncountcmd)
        process = subprocess.Popen(conncountcmd,shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Get the output and errors from the process
        output, errors = process.communicate()

        connection_count = 0;

        if process.returncode == 0:
            connection_count = int(output.decode('utf-8'));
            if (debug):
                print("Connections in CM file:", connection_count)
        else:
            print("Error getting connection count for file '{filename}': {errors.decode()}")


        print ("\n\nExperiment:",experiment_name.rstrip("\n"),"\n==========================================")
        print ("Running",cmdline)

        process = subprocess.Popen(cmdline,shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Get the output and errors from the process
        output, errors = process.communicate()

        if process.returncode == 0:
            # Extract the line count from the output
            #line_count = output.decode().split()[0]
            #print(f"File '{filename}' has {line_count} lines.")
            lines = output.splitlines()

            fcttail = 0
            actual_connection_count = 0
            for x in lines:
                if "finished" in str(x):
                    a = x.decode('utf-8')
                    if (debug):
                        print (a)

                    items = a.split()

                    actual_connection_count = actual_connection_count + 1

                    flowname = a[1]
                    fct = float(items[8])
                    total_pkts[items[1]]= int(items[11])
                    flow_size = float(items[16])
                    thr = flow_size*8/(fct*10**-6) / (10**9)
                    throughputs.append(thr)
                    fcttail = fct
                    fcts.append(fct)
                    if items[1] in targetFCT:
                        if fct <= targetFCT[items[1]]:
                            print ("[PASS] FCT",fct,"us for flow ",items[1], "which is below the target of",targetFCT[items[1]],"us")
                        else:
                            print ("[FAIL] FCT",fct,"us for flow ",items[1], "which is higher than the target of",targetFCT[items[1]],"us")
                elif "New:" in str(x) and "Rtx:" in str(x):
                    last = x
                    a = x.decode('utf-8')
                    if (debug):
                        print (a)

                    items = a.split()
                    new_pkts[experiment_name] = int(items[1])
                    rtx[experiment_name] = int(items[3])
                    rts[experiment_name] = int(items[5])
                    acks[experiment_name] = int(items[9])
  

            if (fcttail > targetTailFCT and targetTailFCT >0):
                print ("[FAIL] Tail FCT",fcttail, "us above the target of",targetTailFCT,"us")
            else:
                print ("[PASS] Tail FCT",fcttail, "us below the target of",targetTailFCT,"us")

            if (actual_connection_count != connection_count):
                print("[FAIL] Total connections in connection matrix was ",connection_count," but only ",actual_connection_count,"finished")
            else:
                print ("[PASS] Connection count",actual_connection_count)

            print ("Summary:",x.decode('utf-8'))
            if do_process:
                subprocess.call("parse_output " + 'logout.dat' + " -ascii > " + "./datacenter/logs/test.asc", shell=True)#+filename.split('/')[-1].split('.')[0]+".asc"
        else:
            # Print any errors that occurred
            print("Error processing file ",filename,errors.decode())
  
        throughputs_mbps = throughputs 
        #Calculate the CDF
        fcts_sorted = sorted(fcts)
        cdf = np.arange(1, len(fcts_sorted) +   1) / len(fcts_sorted)

        mean_fct = np.mean(fcts)
        max_fct = max(fcts)
        plt.plot(fcts_sorted, cdf, marker='o', linestyle='-', label=f'{experiment_name}, tail FCT ({max_fct:.2f})')
        # plt.axvline(mean_fct, color=color, linestyle='--', label=f' {experiment_name} - Mean ({mean_fct:.2f})')
        # plt.axvline(max_fct, color= color, linestyle='-.', label=f'{experiment_name} - 99th Percentile ({max_fct:.2f})')
        if not hold:
            plt.title('ECDF for FCTs')
            plt.xlabel('FCT (us)')
            plt.ylabel('CDF')
            plt.legend()
            plt.grid(True)
            full_path = os.path.join(outpu_dir, 'fcts.png')
            if save_file:
                plt.savefig(full_path,format='png')
            else:
                plt.show()







    plt.figure()
    keys = list(new_pkts.keys())
    print(keys)
    values = list(new_pkts.values())
    plt.bar(keys, values)
    plt.title('New Packets ')
    plt.xlabel('Experiments')
    plt.ylabel('# PKTs')
    full_path = os.path.join(outpu_dir, 'new_pkts.png')
    if save_file:
        plt.savefig(full_path,format='png')
    else:
        plt.show()
 

    plt.figure()
    keys = list(rtx.keys())
    values = list(rtx.values())
    plt.bar(keys, values)
    plt.title('Total Rtx Packets ')
    plt.xlabel('Experiments')
    plt.ylabel('# Rtxs')
    full_path = os.path.join(outpu_dir, 'Rtx.png')
    if save_file:
        plt.savefig(full_path,format='png')
    else:
        plt.show()

    plt.figure()
    keys = list(rts.keys())
    values = list(rts.values())
    plt.bar(keys, values)
    plt.title('Rts Packets ')
    plt.xlabel('Experiments')
    plt.ylabel('# Rts')
    full_path = os.path.join(outpu_dir, 'Rts.png')
    if save_file:
        plt.savefig(full_path,format='png')
    else:
        plt.show()

    plt.figure()
    keys = list(acks.keys())
    values = list(acks.values())
    plt.bar(keys, values)
    plt.title('Acks ')
    plt.xlabel('Experiment')
    plt.ylabel('# Acks')
    full_path = os.path.join(outpu_dir, 'acks.png')
    if save_file:
        plt.savefig(full_path,format='png')
    else:
        plt.show()


debug = True

# total arguments
n = len(sys.argv)
i = 1
# path = './datacenter/'
path = ''


filename='validate_uec_sender.txt'

while (i<n):
    if (sys.argv[i]=="-debug"):
        debug = True
    else:
        filename = sys.argv[i]
        print ("Using " + filename +" as experiment plan")
        
    i = i + 1


run_experiments(path+filename)


# run_experiments(filename)
