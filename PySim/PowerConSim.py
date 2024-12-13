import matplotlib.pyplot as plt

power_consumption = []
max_number_of_nodes = 150
print("hi")


def sim():
    guard_time = 0.4
    broadcast = 7
    packet_airtime = 11.6
    packet_size = 20
    max_packet_size = 254
    packet_bits = packet_size*8
    timeslots = 100
    lampda = 1/10**3

    power_break = 0
    periode_time = broadcast + (guard_time + packet_airtime) * timeslots

    # broadcastConsumtion = 0.03201  # Wattage to receive broadcast
    # transConsumtion = 0.1452  #wattage to send with 20 bytes of data + header
    voltage = 3.3
    mA = 44
    mW_power = voltage * mA

    datarate = 183.1054688
    time_transmitting = packet_bits/datarate

    # This is our powerConsumtion per transmission (dependend on packetsize)
    cost_mJ = mW_power * time_transmitting

    for node_count in range(1, max_number_of_nodes):
        expected_packet_per_node = lampda * periode_time

        packet_sent = expected_packet_per_node / (timeslots/node_count)

        if packet_sent > 1:
            packet_sent = 1

        power_consumed = (cost_mJ * packet_sent) / node_count
        power_consumption.append(power_consumed)

        if (
            power_break == 0 and
            len(power_consumption) > 1 and
            round(power_consumption[-1], 2) < round(power_consumption[-2], 2)
        ):
            power_break = node_count - 1

        print(f"{power_consumed}")

    x = list(range(1, max_number_of_nodes))
    y = power_consumption
    fig, ax = plt.subplots()
    print(f"{x=},{y=}")
    ax.plot(x, y, color="green", label="Power Consumption")
    ax.set_title("Theoretical Power Consumption per node " +
                 f"[Packet size: {packet_size}]")
    ax.set_xlabel("Number of nodes")
    ax.set_ylabel("Node Power Consumption [mJ]")

    ax.axvline(power_break, linestyle="dashed",
               label="Power Consumption begins to decrease", color="green")
    ax.grid(True)
    plt.legend()
    fig.tight_layout()
    fig.savefig("Pplot.png")
    plt.close(fig)


if __name__ == "__main__":
    sim()
