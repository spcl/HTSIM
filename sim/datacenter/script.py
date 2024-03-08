import argparse
import subprocess
import os
from pathlib import Path

# TODO(AG): 
# - remove deprecated params
# - for latency, aim for 1ms RTT
# - use a proper topology instead of a "fake" tree topology
# - should the phantom queue be bounded by the BDP?

# Process the output file to plot the relevant data.
def ProcessResults():
    # Specify the directory in which you want to run the command
    currPath = Path(os.path.abspath(__file__))
    plot_script_path = currPath.parent.parent.parent / "plotting"
    
    command = ["python3", "performance_complete.py"]
    
    # Print the command to be executed.
    commandStr = ' '.join(command)
    print(f"Executing command: {commandStr}")
    
    # Run the command in the specified directory.
    result = subprocess.run(command, cwd=plot_script_path, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)

    # If you need to check for errors, you can print the standard error.
    if result.stderr:
        print("Error:", result.stderr)
        
def main():
    # Create argument parser and define arguments with default values.
    parser = argparse.ArgumentParser(description="Run htsim_uec_entry_modern with specified parameters")
    # HTSIM-core parameters.
    parser.add_argument('--output', '-o', default='uec_entry', help='Output prefix for files. Default: uec_entry')
    parser.add_argument('--seed', type=int, default=920, help='Seed for random number generator. Default: 920')
    # Host/traffic parameters.
    parser.add_argument('--algorithm', default='intersmartt', help='Algorithm to use. Default: intersmartt')
    parser.add_argument('--goal', default='test8_32.bin', help='Goal input file. Default: test8_32.bin')
    parser.add_argument('--fast_drop', type=int, default=1, help='Use quick adapt or not. Default: 1')
    parser.add_argument('--use_pacing', type=int, default=1, help='Use pacing or not. Default: 1')
    parser.add_argument('--x_gain', type=float, default=5, help='Fair increase gain for SMARTT. Default: 5, i.e, increase 5 packets per RTT.')
    parser.add_argument('--y_gain', type=float, default=0, help='Multiplicative increase gain for SMARTT. Default: 0')
    parser.add_argument('--w_gain', type=float, default=0, help='Multiplicative decrease gain for SMARTT. Default: 0')
    parser.add_argument('--z_gain', type=float, default=0.125, help='Fair decrease gain for SMARTT. Default: 0.125, i.e, decrease 1/8 packets per ECN marked packet.')
    parser.add_argument('--bonus_drop', type=float, default=1.0, help='When quick adapt is used, this will be the gain (>1 or <1) for the target rate value that quick adapt is outputting. Default: 1.0')
    parser.add_argument('--explicit_starting_cwnd', type=int, default=5600000, help='Starting cwnd regardless of any network specific information (overrides those calculations). Default: 5600000=BDP in this case and needs to adapat to changes in rate or delay')
    # Network/fabric parameters.
    parser.add_argument('--oversub_ratio', '-k', type=int, default=1, help='Oversubscription ratio. Default: 1')
    parser.add_argument('--queue_size', '-q', type=int, default=4452000, help='Queue size in general. For intersmartt, it sets q=BDP/10. Default: 4452000')
    parser.add_argument('--nodes', type=int, default=1024, help='Number of hosts. Default: 1024')
    parser.add_argument('--strat', default='ecmp_host_random2_ecn', help='Routing strategy. Default: ecmp_host_random2_ecn')
    parser.add_argument('--number_entropies', type=int, default=256, help='Number of entropies. Default: 256')
    parser.add_argument('--linkspeed', type=int, default=80000, help='Link speed in Mbps. Default: 80000 (80Gbps)')
    parser.add_argument('--mtu', type=int, default=4096, help='MTU in bytes. Default: 4096')
    parser.add_argument('--queue_type', default='composite', help='Queue type. Composite means both ECN and trimming with priority (other types not important but they exist). Default: composite')
    parser.add_argument('--hop_latency', type=int, default=70000, help='Hop latency in ns. Default: 70000 (70us). There are 8 hops (4 per direction) so 560us total RTT.')
    parser.add_argument('--switch_latency', type=int, default=0, help='Switch latency in ns (does not matter in the long haul link experiments). Default: 0')
    parser.add_argument('--kmin', type=int, default=20, help='Min ratio of queue (or phantom queue) for ECN. Default: 20')
    parser.add_argument('--kmax', type=int, default=80, help='Max ratio of queue for ECN. Default: 80')
    parser.add_argument('--use_phantom', type=int, default=1, help='Use phantom queue or not. Default: 1')
    parser.add_argument('--phantom_slowdown', type=int, default=5, help='Slowdown factor (in %%) for phantom queue. Default: 5, i.e, phantom queue drains 5%% slower than the real queue.')
    parser.add_argument('--phantom_size', type=int, default=5600000, help='Size of the phantom queue. Default: 5600000=BDP in this case and needs to adapt to changes in rate or delay')
    parser.add_argument('--explicit_starting_buffer', type=int, default=560000, help='Starting buffer size regardless of any network specific information (overrides those calculations). Default: 560000=BDP/10 in this case and needs to adapt to changes in rate or delay')
    # Output/logging parameters.
    parser.add_argument('--collect_data', type=int, default=1, help='Collect data for custom logging. Default: 1')
    parser.add_argument('--output_file', default='out.tmp', help='Output file name for htsim. Default: out3.tmp')
    # DEPRECATED
    parser.add_argument('--use_fast_increase', type=int, default=0, help='Use fast increase (DEPRECATED). Default: 0')
    parser.add_argument('--reuse_entropy', type=int, default=1, help='Reuse entropy or not (DEPRECATED: functionality is subsumed in the "strat" class). Default: 1')
    parser.add_argument('--explicit_base_rtt', type=int, default=561400000, help='Base RTT (DEPRECATED: was used for plotting). Default: 561400000')
    parser.add_argument('--explicit_target_rtt', type=int, default=610000000, help='Target RTT (DEPRECATED: was used for plotting). Default: 610000000')
    
    # Parse command-line arguments.
    args = parser.parse_args()

    # TODO(AG): validate parameters here if needed.
    
    # Construct command with arguments.
    command = [
        "./htsim_uec_entry_modern",
        "-o", args.output,
        "-algorithm", args.algorithm,
        "-k", str(args.oversub_ratio),
        "-q", str(args.queue_size),
        "-nodes", str(args.nodes),
        "-strat", args.strat,
        "-number_entropies", str(args.number_entropies),
        "-kmin", str(args.kmin),
        "-kmax", str(args.kmax),
        "-use_fast_increase", str(args.use_fast_increase),
        "-fast_drop", str(args.fast_drop),
        "-linkspeed", str(args.linkspeed),
        "-mtu", str(args.mtu),
        "-seed", str(args.seed),
        "-queue_type", args.queue_type,
        "-hop_latency", str(args.hop_latency),
        "-switch_latency", str(args.switch_latency),
        "-reuse_entropy", str(args.reuse_entropy),
        "-goal", args.goal,
        "-x_gain", str(args.x_gain),
        "-y_gain", str(args.y_gain),
        "-w_gain", str(args.w_gain),
        "-z_gain", str(args.z_gain),
        "-bonus_drop", str(args.bonus_drop),
        "-collect_data", str(args.collect_data),
        "-explicit_starting_cwnd", str(args.explicit_starting_cwnd),
        "-explicit_starting_buffer", str(args.explicit_starting_buffer),
        "-use_pacing", str(args.use_pacing),
        "-use_phantom", str(args.use_phantom),
        "-phantom_slowdown", str(args.phantom_slowdown),
        "-phantom_size", str(args.phantom_size),
    ]
    
    # Print command to be executed for debugging.
    commandStr = ' '.join(command)
    print(f"Executing command: {commandStr}")
    
    # Open output file for redirection.
    with open(args.output_file, "w") as output_file:
        # Execute the command with output redirection to the file, and check for errors.
        subprocess.run(command, stdout=output_file, stderr=subprocess.STDOUT, check=True)
    
    # Process the output file to plot the relevant data.
    ProcessResults()

if __name__ == "__main__":
    main()
