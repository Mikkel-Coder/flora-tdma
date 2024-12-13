import matplotlib.pyplot as plt
from pathlib import Path
import json
import re

from plot_utils import (
    load_clean_dataset,
    calculate_throughput,
    calculate_power_consumption_per_node,
    calculate_network_energy_consumption,
)

def plot(data: dict, conf: dict):
    # Copy for PySim. 
    # fig, ax = plt.subplots()
    # ax.plot(x, y, color="blue", label="Power Consumption")
    # ax.set_title("Theoretical Power Consumption per node " +
    #              f"[Packet size: {packet_size}]")
    # ax.set_xlabel("Number of nodes")
    # ax.set_ylabel("Node Power Consumption [mJ]")

    # ax.axvline(power_break, linestyle="dashed",
    #            label="Power Consumption begins to decrease", color="green")
    
    # ax.text(power_break - 4, power_consumption[power_break] - 0.15,
    #         str(power_break), color='green',
    #         fontsize=12, ha='center', va='center')
    # ax.text(5, power_consumption[0] - 0.05,
    #         f"{power_consumption[0]:.2f}", color="blue",
    #         fontsize=12, ha='center', va='center')

    # ax.legend()
    # ax.grid(True)
    # fig.tight_layout()
    # fig.savefig("Pplot.png")
    # plt.close(fig)
    ...

def compute_and_save(functions_to_call, run_parameters, pre_computed_results):
    for bruh in functions_to_call:
        x, y = bruh[0](*bruh[1])
        json_data: dict = {"x": x, "y": y}
        file_name: str = f'{bruh[2]}-{run_parameters['lora_app_data']}.json'

        with (pre_computed_results / file_name).open('w') as fp:
            json.dump(json_data, fp)



def main() -> None:
    show_plots: bool = False
    pre_computed_results = Path('samples/flora-tdma/simulations/results/pre_computed_results/')
    data_vec, data_sca = load_clean_dataset()
    run_parameters = { # Remember to change this according to the ini file
        'lora_app_data': 20,
        'time': 3 * 60 * 60, # hours to sec
    }
    functions_to_call = [
        (calculate_throughput, [data_vec, run_parameters], "throughput"),
        (calculate_power_consumption_per_node, [data_sca], "power_per_node"),
        (calculate_network_energy_consumption, [data_sca], "nec")
    ]

    # Before plotting we check if we have the right json data beforehand
    filename_pattern = r"(254|20)\.json$"
    matches = [re.match(filename_pattern, filename) for filename in pre_computed_results.iterdir()]

    # Check if we found our json results
    if not any(matches):
        compute_and_save(functions_to_call, run_parameters, pre_computed_results)

    found_lora_app_data = { ma.group(1) for ma in matches }
    if {'20', '254'} != found_lora_app_data:
        print("We are missing data for lora app datasize. Please generate it")
        exit(1)
    
    # We now have the data we need for plotting
    print("We are ready to plot!")


    





    # TODO: MzKay function call here 


if __name__ == '__main__':
    main()
