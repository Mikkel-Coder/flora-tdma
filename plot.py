import matplotlib.pyplot as plt

from plot_utils import load_clean_dataset

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
    time = 3 * 60 * 60 # hours to sec
    x = sorted(int(key) for key in data.keys())
    y = [data[str(key)]["total_packet_length_sum"] / time for key in x]

    plt.figure(figsize=(8, 5))
    plt.plot(x, y, marker="o", linestyle="-", color="blue")
    plt.xlabel("Number of Nodes")
    plt.ylabel("Network Throughput [bps]")
    plt.grid(True)
    plt.show()


def main() -> None:
    data_vec, data_sca = load_clean_dataset()

    print(data_vec)
    print(data_sca)

    # plot power consumption per node
    plot_power_consumption_per_node(data_sca)
    #plot_throughput_per_node(data)
    #plot Network Power Consumption
    # Throughput


if __name__ == '__main__':
    main()
