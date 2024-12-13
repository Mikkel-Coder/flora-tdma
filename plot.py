import json
import pickle
from pathlib import Path
import re
import subprocess
from time import sleep

import matplotlib.pyplot as plt

path = Path('samples/flora-tdma/simulations/results/')
omnetpp_root = Path('.')
# path = Path('.')
json_vec_path: Path = path / 'data_vec.json'
pickle_vec_path: Path = path / 'data_vec.pkl'
pickle_vec_path_clean: Path = path / 'data_clean_vec.pkl'
json_vec_path_clean: Path = path / 'data_vec_clean.json'
json_sca_path: Path = path / 'data_sca.json'
pickle_sca_path: Path = path / 'data_sca.pkl'
pickle_sca_path_clean: Path = path / 'data_clean_sca.pkl'
json_sca_path_clean: Path = path / 'data_sca_clean.json'

ln_re_pattern = r'LoRaNetworkTest\.loRaNodes\[(\d+)\]'

time = 3 * 60 * 60 # hours to sec

def generate_pickles():
    print("Reading vec json file...")
    with json_vec_path.open('r') as fp:
        json_vec_data = json.load(fp)

    print("Writing json vec data to a pickle file...")
    with pickle_vec_path.open('wb') as fp:
        pickle.dump(json_vec_data, fp)
    
    del json_vec_data # For optimize
    
    print("Reading sca json file...")
    with json_sca_path.open('r') as fp:
        json_sca_data = json.load(fp)

    print("Writing json sca data to a pickle file...")
    with pickle_sca_path.open('wb') as fp:
        pickle.dump(json_sca_data, fp)
    
    del json_sca_data

def clean_pickles_data():
    print("Loading dirty vec pickle data..")
    with pickle_vec_path.open('rb') as fp:
        dirty_vec_data: dict = pickle.load(fp)

    clean_vec_data = _clean_vec_data(dirty_vec_data)    

    print("Writing vec clean pickle data...")
    with pickle_vec_path_clean.open('wb') as fp:
        pickle.dump(clean_vec_data, fp)

    print("Writing clean vec data to json file...")
    with json_vec_path_clean.open('w') as fp:
        json.dump(clean_vec_data, fp)
    
    del clean_vec_data

    print("Loading dirty sca pickle data..")
    with pickle_sca_path.open('rb') as fp:
        dirty_sca_data: dict = pickle.load(fp)

    clean_sca_data = _clean_sca_data(dirty_sca_data)

    print("Writing sca clean pickle data...")
    with pickle_sca_path_clean.open('wb') as fp:
        pickle.dump(clean_sca_data, fp)

    print("Writing clean sca data to json file...")
    with json_sca_path_clean.open('w') as fp:
        json.dump(clean_sca_data, fp)
    
    del clean_sca_data


def _clean_vec_data(dirty_data: dict):
    # Clean
    print("Cleaning dirty data...")
    clean_vec_data = dict()
    for key in dirty_data.keys():
        numNodes = dirty_data[key]["itervars"]["numNodes"]
        print(f"\nCleaning data for {numNodes} node sim...")
        clean_vec_data[numNodes] = {}
        current_sec = clean_vec_data[numNodes]

        for i in range(int(numNodes)):
            current_sec[i] = {}

        vectors = dirty_data[key]["vectors"]

        queue_module_pattern = ln_re_pattern + \
            r'\.LoRaNic\.queue'

        total_packet_length_sum = 0

        print('Running through vectors...')
        for vector in vectors:
            queue_match = re.match(queue_module_pattern, vector['module'])
            if (queue_match and
                    vector['name'] == 'outgoingPacketLengths:vector'):
                nodeNumber = int(queue_match.group(1))
                #print(f'Found packet lengths for node {nodeNumber}')
                this_node = current_sec[nodeNumber]
                this_node['packet_lengths'] = []

                for value in vector['value']:
                    if value != 0:
                        this_node['packet_lengths'].append(value)

                packet_length_sum = sum(this_node['packet_lengths'])
                this_node['packet_length_sum'] = packet_length_sum
                total_packet_length_sum += packet_length_sum

        current_sec['total_packet_length_sum'] = total_packet_length_sum

        print(f'Total Packet length Sum: {total_packet_length_sum}')
    
    return clean_vec_data

def _clean_sca_data(dirty_data: dict):
    clean_sca_data = dict()
    for key in dirty_data.keys():
        numNodes: str = dirty_data[key]["itervars"]["numNodes"]
        print(f"\nCleaning data for {numNodes} node sim...")
        clean_sca_data[numNodes] = {}
        current_sec = clean_sca_data[numNodes]

        for i in range(int(numNodes)):
            current_sec[i] = {}
        
        scalars = dirty_data[key]["scalars"]

        energy_module_pattern = ln_re_pattern + \
            r'\.LoRaNic\.radio\.energyConsumer'
        
        total_energy_consumed: float = 0.

        for scalar in scalars:
            energy_match = re.match(energy_module_pattern, scalar['module'])
            if (energy_match and
                    scalar['name'] == 'totalEnergyConsumed'):
                nodeNumber = int(energy_match.group(1))
                
                this_node = current_sec[nodeNumber]
                this_node['energy_consumed'] = scalar['value']
                total_energy_consumed += this_node['energy_consumed']
    
        current_sec['total_energy_consumed'] = total_energy_consumed
        current_sec['mean_node_energy_consumption'] = \
            total_energy_consumed / int(numNodes)

    return clean_sca_data

def plot_power_consumption_per_node(data: dict):
    x = sorted(int(key) for key in data.keys())
    y = [data[str(key)]["mean_node_energy_consumption"] for key in x]
    
    plt.figure(figsize=(8, 5))
    plt.plot(x, y, marker="o", linestyle="-", color="blue")
    plt.xlabel("Number of Nodes")
    plt.ylabel("Power Consumption Per Node [Joule]")
    plt.grid(True)
    plt.show()

def plot_throughput(data: dict):
    x = sorted(int(key) for key in data.keys())
    y = [data[str(key)]["total_packet_length_sum"] / time for key in x]

    plt.figure(figsize=(8, 5))
    plt.plot(x, y, marker="o", linestyle="-", color="blue")
    plt.xlabel("Number of Nodes")
    plt.ylabel("Network Throughput [bps]")
    plt.grid(True)
    plt.show()

def generate_omnetpp_json():
    # opp_scavetool index flora-tdma-1000.vec
    # opp_scavetool x -o data_vec.json *.vec
    # opp_scavetool x -o data_sca.json *.sca
    vec_command: str = "bash -c \"source setenv " + \
        "&& cd samples/flora-tdma/simulations/results " + \
        "&& opp_scavetool x -o data_vec.json *.vec\""
    
    sca_command: str = "bash -c \"source setenv " + \
        "&& cd samples/flora-tdma/simulations/results " + \
        "&& opp_scavetool x -o data_sca.json *.sca\""
    
    print("Creating omnetpp export vec data job.")
    process_vec = subprocess.Popen(
        vec_command,
        shell=True,
        cwd=omnetpp_root,
        stdout=subprocess.DEVNULL
    )

    print("Waiting for vec data to be generated:", end='')
    while process_vec.poll() is None:
        print(".", end='', flush=True)
        sleep(2)
    
    exit_code = process_vec.wait() 
    if exit_code != 0:
        print(f"Unable to generate data_vec.json, exit code: {exit_code}")
        exit(1)
    
    print("Creating omnetpp export sca data job.")
    process_sca = subprocess.Popen(
        sca_command,
        shell=True,
        cwd=omnetpp_root,
        stdout=subprocess.DEVNULL
    )

    print("Waiting for sca data to be generated:", end='')
    while process_sca.poll() is None:
        print(".", end='', flush=True)
        sleep(3)
    
    exit_code = process_sca.wait() 
    if exit_code != 0:
        print(f"Unable to generate data_sca.json, exit code: {exit_code}")
        exit(1)


def main() -> None:
    pattern = re.compile(r"flora-tdma-\d+\.(vec|sca)")
    sca_and_vec_files = path.iterdir()
    matching_files = [f.name for f in sca_and_vec_files if f.is_file() and pattern.match(f.name)]

    if not matching_files:
        print("No OMNet++ output files found. Please run the simulation first using OMNet++.")
        exit(1)
    
    if not json_vec_path.exists() or not json_sca_path.exists():
        print("No json data found. Generating it.")
        generate_omnetpp_json()

    if not pickle_vec_path.exists() or not pickle_sca_path.exists():
        print("No pickle data found. Generating...")
        generate_pickles()

    if not pickle_vec_path_clean.exists() or not pickle_sca_path_clean.exists():
        print("No clean pickle data found. Cleaning...")
        clean_pickles_data()

    print("Loading clean vec pickle data...")
    with pickle_vec_path_clean.open('rb') as fp:
        data_vec: dict = pickle.load(fp)

    print("Loading clean sca pickle data...")
    with pickle_sca_path_clean.open('rb') as fp:
        data_sca: dict = pickle.load(fp)

    print(data_vec)
    print(data_sca)

    # plot power consumption per node
    plot_power_consumption_per_node(data_sca)
    #plot_throughput_per_node(data)
    #plot Network Power Consumption
    # Throughput


if __name__ == '__main__':
    main()
