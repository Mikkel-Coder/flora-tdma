import matplotlib.pyplot as plt
from pathlib import Path
import json
from collections import defaultdict

from plot_utils import (
    load_clean_dataset,
    calculate_throughput,
    calculate_power_consumption_per_node,
    calculate_network_energy_consumption,
)

from PySim.ThroughputSim import sim as throughput_theorectical
from PySim.PowerConSim import sim as power_consumption_theorectical

SAVE_PLOT: bool = True

plt.rcParams["font.family"] = "DejaVu Serif"

def save_plot_to_folder(fig, filename: str) -> None:
    save_folder = Path('samples/flora-tdma/figures/')
    fig.savefig(save_folder / filename)
    plt.close(fig)
    

def plot_throughput(data: dict, size: int):
    fig, ax = plt.subplots()
    
    ax.set_title(f'Throughput using packet size: {size}B')
    ax.plot(data[f'throughput-{size}']['x'],
            data[f'throughput-{size}']['y'],
            color="blue",
            label="Simulation" 
    )
    ax.plot(data[f'throughput-theoretical-{size}']['x'],
            data[f'throughput-theoretical-{size}']['y'],
            color="red",
            label="Theoretical" 
    )
    ax.set_xlabel("Number of Nodes")
    ax.set_ylabel("Throughput [bps]")
    ax.legend()
    ax.grid(True)
    fig.tight_layout()
    save_plot_to_folder(fig, f'throughput-{size}.pdf') if SAVE_PLOT else plt.show()


def plot_power_per_node(data: dict):
    fig, ax = plt.subplots()
    
    ax.set_title(f'Power Consumption per Node')
    ax.plot(data[f'power_per_node-{20}']['x'],
            data[f'power_per_node-{20}']['y'],
            color="blue",
            label="Simulation: 20B" 
    )
    ax.plot(data[f'power_per_node-254']['x'],
            data[f'power_per_node-254']['y'],
            color="red",
            label="Simulation: 254B" 
    )
    ax.set_xlabel("Number of Nodes")
    ax.set_ylabel("Energy Consumption [J]")
    ax.legend()
    ax.grid(True)
    save_plot_to_folder(fig, 'power-per-node.pdf') if SAVE_PLOT else plt.show()


def plot_nec_sim(data: dict):
    fig, ax = plt.subplots()

    ax.plot(data[f'nec-{20}']['x'],
            data[f'nec-{20}']['y'],
            color="red",
            label="Simulation: 20B" 
    )
    ax.plot(data[f'nec-{254}']['x'],
            data[f'nec-{254}']['y'],
            color="blue",
            label="Simulation: 254B" 
    )

    ax.set_title('Network Energy Consumption Simulation')
    ax.set_xlabel("Number of Nodes")
    ax.set_ylabel("NEC [J]")
    ax.legend()
    ax.grid(True)
    save_plot_to_folder(fig, 'nec_sim.pdf') if SAVE_PLOT else plt.show()


def plot_nec_theoretical(data: dict):
    fig, ax = plt.subplots()

    ax.plot(data[f'nec-theoretical-{20}']['x'],
            data[f'nec-theoretical-{20}']['y'],
            color="red",
            label="Theoretical: 20B" 
    )
    ax.plot(data[f'nec-theoretical-{254}']['x'],
            data[f'nec-theoretical-{254}']['y'],
            color="blue",
            label="Theoretical: 254B" 
    )

    ax.set_title('Network Energy Consumption Theoretical')
    ax.set_xlabel("Number of Nodes")
    ax.set_ylabel("NEC [mJ]")
    ax.legend()
    ax.grid(True)
    save_plot_to_folder(fig, 'nec_theoretical.pdf') if SAVE_PLOT else plt.show()


def compute_and_save(functions_to_call, run_parameters, pre_computed_results):
    for bruh in functions_to_call:
        x, y = bruh[0](*bruh[1])
        json_data: dict = {"x": x, "y": y}
        file_name: str = f'{bruh[2]}-{run_parameters['lora_app_data']}.json'

        with (pre_computed_results / file_name).open('w') as fp:
            json.dump(json_data, fp)


def main() -> None:
    pre_computed_results = Path('samples/flora-tdma/simulations/results/pre_computed_results/')
    data_vec, data_sca = load_clean_dataset()
    run_parameters = { # Remember to change this according to the ini file
        'lora_app_data': 254,
        'time': 3 * 60 * 60, # hours to sec
    }
    functions_to_call = [
        (calculate_throughput, [data_vec, run_parameters], "throughput"),
        (calculate_power_consumption_per_node, [data_sca, run_parameters], "power_per_node"),
        (calculate_network_energy_consumption, [data_sca], "nec")
    ]

    #compute_and_save(functions_to_call, run_parameters, pre_computed_results)

    # load in data from disk
    pre_computed_results_data = defaultdict(dict)
    for file in pre_computed_results.iterdir():
        with file.open('r') as fp:
            pre_computed_results_data[file.stem] = json.load(fp)
    
    # Compute the theoretical throughput
    x, y = throughput_theorectical(20)
    pre_computed_results_data['throughput-theoretical-20']['x'] = x
    pre_computed_results_data['throughput-theoretical-20']['y'] = y

    x, y = throughput_theorectical(254)
    pre_computed_results_data['throughput-theoretical-254']['x'] = x
    pre_computed_results_data['throughput-theoretical-254']['y'] = y

    # Compute the theoretical power
    # Wrong calculations
    #x, y = power_consumption_theorectical(20, total=False)
    #pre_computed_results_data['power-per-node-theoretical-20']['x'] = x
    #pre_computed_results_data['power-per-node-theoretical-20']['y'] = y

    # Wrong calculations
    #x, y = power_consumption_theorectical(254, total=False)
    #pre_computed_results_data['power-per-node-theoretical-254']['x'] = x
    #pre_computed_results_data['power-per-node-theoretical-254']['y'] = y

    x, y = power_consumption_theorectical(20, total=True)
    y = [mj / 1000 for mj in y]
    pre_computed_results_data['nec-theoretical-20']['x'] = x
    pre_computed_results_data['nec-theoretical-20']['y'] = y

    x, y = power_consumption_theorectical(254, total=True)
    y = [mj / 1000 for mj in y]
    pre_computed_results_data['nec-theoretical-254']['x'] = x
    pre_computed_results_data['nec-theoretical-254']['y'] = y

    plot_throughput(pre_computed_results_data, size=20)
    plot_throughput(pre_computed_results_data, size=254)

    plot_power_per_node(pre_computed_results_data)

    plot_nec_sim(pre_computed_results_data)
    plot_nec_theoretical(pre_computed_results_data)


if __name__ == '__main__':
    main()
