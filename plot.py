import json
import pickle
from pathlib import Path
import logging

path = Path('samples/flora-tdma/simulations/results/')
json_path: Path = path / 'data_vec.json'
pickle_path: Path = path / 'data_vec.pkl'
pickle_path_clean: Path = path / 'data_clean_vec.pkl'

def generate_pickle():
    logging.info("Reading json file...")
    with json_path.open('r') as file:
        json_data = json.load(file)
    
    logging.info("Writing json data to a pickle file...")
    with pickle_path.open('wb') as file:
        pickle.dump(json_data, file)

def clean_pikle_data():
    with pickle_path.open('rb') as file:
        dirty_data: dict = pickle.load(file)

    # Clean
    clean_data = dict()
    for key in dirty_data.keys():
        new_key = dirty_data[key]["itervars"]["numNodes"]
        clean_data[new_key] = {}

    

    with pickle_path_clean.open('wb') as file:
        pickle.dump(clean_data, file)

def plot_power_consumption():
    ...

def main() -> None:
    if not pickle_path.exists():
        logging.info("Not pickle data found. Generating one...")
        generate_pickle()

    if not pickle_path_clean.exists():
        logging.info("Not clean pickle data found. Cleaning ...")
        clean_pikle_data()

    logging.info("Loading clean pickle data...")
    with pickle_path_clean.open('rb') as file:
        data: dict = pickle.load(file)

    # plot power consumption per node
    plot_power_consumption()
    # plot Network Power Consumption
    # Throughput 

if __name__ == '__main__':
    main()