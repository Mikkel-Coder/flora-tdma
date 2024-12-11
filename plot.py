import json
import pickle
from pathlib import Path
import logging
import re
import sys

logger = logging.getLogger(__name__)
logger.addHandler(logging.StreamHandler(sys.stdout))
logger.setLevel(logging.INFO)

path = Path('samples/flora-tdma/simulations/results/')
# path = Path('.')
json_path: Path = path / 'data_vec.json'
pickle_path: Path = path / 'data_vec.pkl'
pickle_path_clean: Path = path / 'data_clean_vec.pkl'

ln_re_pattern = r'LoRaNetworkTest\.loRaNodes\[(\d+)\]'


def generate_pickle():
    logger.info("Reading json file...")
    with json_path.open('r') as fp:
        json_data = json.load(fp)

    logger.info("Writing json data to a pickle file...")
    with pickle_path.open('wb') as fp:
        pickle.dump(json_data, fp)


def clean_pickle_data():
    with pickle_path.open('rb') as fp:
        dirty_data: dict = pickle.load(fp)

    # Clean
    clean_data = dict()
    for key in dirty_data.keys():
        numNodes = dirty_data[key]["itervars"]["numNodes"]
        logger.info(f"\nCleaning data for {numNodes} node sim...")
        clean_data[numNodes] = {}
        current_sec = clean_data[numNodes]

        for i in range(int(numNodes)):
            current_sec[i] = {}

        vectors = dirty_data[key]["vectors"]

        energy_module_pattern = ln_re_pattern + \
            r'\.LoRaNic\.radio\.energyConsumer'

        total_power_sum = 0

        queue_module_pattern = ln_re_pattern + \
            r'\.LoRaNic\.queue'

        total_packet_length_sum = 0

        logger.info('Running through vectors...')
        for vector in vectors:
            energy_match = re.match(energy_module_pattern, vector['module'])
            if (energy_match and
                    vector['name'] == 'powerConsumption:vector'):
                nodeNumber = int(energy_match.group(1))
                logger.debug(f'Found power consumption for node {nodeNumber}')
                this_node = current_sec[nodeNumber]
                this_node['power'] = []

                for value in vector['value']:
                    if value != 0:
                        this_node['power'].append(value)

                power_sum = sum(this_node['power'])
                this_node['power_sum'] = power_sum
                total_power_sum += power_sum

            queue_match = re.match(queue_module_pattern, vector['module'])
            if (queue_match and
                    vector['name'] == 'outgoingPacketLengths:vector'):
                nodeNumber = int(queue_match.group(1))
                logger.debug(f'Found packet lengths for node {nodeNumber}')
                this_node = current_sec[nodeNumber]
                this_node['packet_lengths'] = []

                for value in vector['value']:
                    if value != 0:
                        this_node['packet_lengths'].append(value)

                packet_length_sum = sum(this_node['packet_lengths'])
                this_node['packet_length_sum'] = packet_length_sum
                total_packet_length_sum += packet_length_sum

        current_sec['total_power_sum'] = total_power_sum
        current_sec['total_packet_length_sum'] = total_packet_length_sum

        logger.info(f'Total Power Sum: {total_power_sum}')
        logger.info(f'Total Packet length Sum: {total_packet_length_sum}')

    with pickle_path_clean.open('wb') as fp:
        pickle.dump(clean_data, fp)


def plot_power_consumption(data: dict):
    ...


def main() -> None:
    if not pickle_path.exists():
        logger.info("Not pickle data found. Generating one...")
        generate_pickle()

    if not pickle_path_clean.exists():
        logger.info("Not clean pickle data found. Cleaning ...")
        clean_pickle_data()

    logger.info("Loading clean pickle data...")
    with pickle_path_clean.open('rb') as fp:
        data: dict = pickle.load(fp)

    # plot power consumption per node
    plot_power_consumption(data)
    # plot Network Power Consumption
    # Throughput


if __name__ == '__main__':
    main()
