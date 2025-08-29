#!/usr/bin/env python
# Sample Python script to read filenames from an input file and launch a process for each filename to count the number of lines in that file.

import subprocess
import sys
import os

def run_experiments(input_filename):
    # Read the filenames from the input file
    with open(input_filename, 'r') as file:
        inputlines = file.readlines()


    # Remove any whitespace or newlines from the filenames
    i = 0
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
        binary = "./htsim_eqds"

        #figure out parameters.
        while i<len(inputlines):
            #print(str(inputlines[i]))
            if (not str(inputlines[i]).startswith("!")):
                #print ("Stopping param parsing")
                break;

            p = str(inputlines[i])
            i = i + 1

            if ("Binary" in p):
                binary = (p.split(" ",1)[1]).rstrip()
                print ("Using binary:", binary)
            elif ("Param" in p):
                params.append(p.split(" ",1)[1])
                #print ("Found param",p.split(" ",1)[1])
            elif ("tailFCT" in p):
                targetTailFCT = int(p.split(" ",1)[1])
                #print ("Found targetTailFCT",targetTailFCT)
            elif ("FCT" in p):
                q = p.split()
                targetFCT[q[1]] = int(q[2])
                #print ("Found targetTailFCT",targetFCT[q[1]],"for flow",q[1])                
            elif ("Experiment" in p):
                experiment_name = p.split(" ",1)[1]

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

        if (dryrun):
            continue
        
        process = subprocess.Popen(conncountcmd,shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        # Get the output and errors from the process
        output, errors = process.communicate()

        connection_count = 0;

        if process.returncode == 0:
            connection_count = int(output.decode('utf-8'));
            if (debug):
                print("Connections in CM file:", connection_count)
        else:
            print(f"Error getting connection count for file '{filename}': {errors.decode()}")


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
            fctmin = 0
            actual_connection_count = 0
            for x in lines:
                if "finished" in str(x):
                    a = x.decode('utf-8')
                    if (debug):
                        print (a)

                    items = a.split()
                    
                    if "total messages" in str(x):
                        assert items[9] == 'total'
                        assert items[10] == 'messages'
                        actual_connection_count = actual_connection_count + int(items[11])
                    else:
                        actual_connection_count = actual_connection_count + 1

                    flowname = a[1]
                    fct = float(items[8])
                    fcttail = fct

                    if (fctmin == 0):
                        fctmin = fct


                    if items[1] in targetFCT:
                        if fct <= targetFCT[items[1]]:
                            print ("[PASS] FCT",fct,"us for flow ",items[1], "which is below the target of",targetFCT[items[1]],"us")
                        else:
                            print ("[FAIL] FCT",fct,"us for flow ",items[1], "which is higher than the target of",targetFCT[items[1]],"us")
                elif "New:" in str(x) and "Rtx:" in str(x):
                    last = x


            if (fcttail > targetTailFCT and targetTailFCT >0):
                print ("[FAIL] Tail FCT",fcttail, "us above the target of",targetTailFCT,"us")
            else:
                print ("[PASS] Tail FCT",fcttail, "us below the target of",targetTailFCT,"us")

            if (actual_connection_count != connection_count):
                print("[FAIL] Total connections in connection matrix was ",connection_count," but only ",actual_connection_count,"finished")
            else:
                print ("[PASS] Connection count",actual_connection_count)

            print ("FCT Spread",fctmin,"->",fcttail, "ratio",fcttail/fctmin)
            print ("Summary:",x.decode('utf-8'))

        else:
            # Print any errors that occurred
            print("Error processing file ",filename,errors.decode())

debug = False
dryrun = False

# total arguments
n = len(sys.argv)
i = 1;

filename='validate.txt'

while (i<n):
    if (sys.argv[i]=="-debug"):
        debug = True;
    elif (sys.argv[i]=="-dryrun"):
        dryrun = True;
    else:
        filename = sys.argv[i]
        print ("Using " + filename +" as experiment plan")
        
    i = i + 1

run_experiments(filename)
