import matplotlib.pyplot as plt
import pickle

power_consumption = []
power_consumption_total = []
max_number_of_nodes = 150


def sim():
    guard_time = 0.4
    broadcast = 7
    packet_airtime = 11.6
    packet_size = 20
    max_packet_size = 254
    packet_bits = packet_size*8
    timeslots = 100
    lampda = 1/10**3
    cycle_time = broadcast + (guard_time + packet_airtime) * timeslots

    power_break = 0

    # broadcastConsumtion = 0.03201  # Wattage to receive broadcast
    # transConsumtion = 0.1452  #wattage to send with 20 bytes of data + header
    voltage = 3.3
    transmit_mA = 44
    receive_mA = 9.7
    idle_mA = 0.0001

    transmit_mW = voltage * transmit_mA
    receive_mW = voltage * receive_mA
    idle_mW = voltage * idle_mA

    datarate = 183.1054688
    timeslot_transmit_time = packet_bits/datarate

    time_used_on_receive = 6.4 / cycle_time

    receive_cost_mJ = time_used_on_receive * receive_mW

    for node_count in range(1, max_number_of_nodes):
        expected_packet_per_node = lampda * cycle_time

        num_of_timeslots_per_node = timeslots/node_count

        packet_sent = expected_packet_per_node / num_of_timeslots_per_node

        if packet_sent > 1:
            packet_sent = 1

        timeslots_filled = num_of_timeslots_per_node * packet_sent

        time_used_on_transmit = \
            (timeslot_transmit_time * timeslots_filled) / cycle_time
        time_used_on_idle = \
            (cycle_time - (time_used_on_receive + time_used_on_transmit)) \
            / cycle_time

        transmit_cost_mJ = time_used_on_transmit * transmit_mW
        idle_cost_mJ = time_used_on_idle * idle_mW

        power_consumed = transmit_cost_mJ + receive_cost_mJ + idle_cost_mJ
        power_consumption.append(power_consumed)
        power_consumption_total.append(power_consumed * node_count)

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

    ax.plot(x, y, color="blue", label="Power Consumption")
    ax.set_title("Theoretical Power Consumption per node per second \n" +
                 f"[Packet size: {packet_size}]")
    ax.set_xlabel("Number of nodes")
    ax.set_ylabel("Node Power Consumption [mJ]")

    ax.axvline(power_break, linestyle="dashed",
               label="Power Consumption begins to decrease", color="green")

    ax.text(power_break - 4, power_consumption[power_break] - 0.01,
            str(power_break), color='green',
            fontsize=12, ha='center', va='center')
    ax.text(5, power_consumption[0] - 0.002,
            f"{power_consumption[0]:.2f}", color="blue",
            fontsize=12, ha='center', va='center')

    ax.legend()
    ax.grid(True)
    fig.tight_layout()
    fig.savefig("Pplot.png")
    plt.close(fig)

    with open('powercondata.pkl', 'wb') as fp:
        pickle.dump((x, y), fp)

    del fig
    del ax
    del y

    y = power_consumption_total

    fig, ax = plt.subplots()

    ax.plot(x, y, color="blue", label="Network Power Consumption")
    ax.set_title("Theoretical Network Power Consumption \n" +
                 f"[Packet size: {packet_size}]")
    ax.set_xlabel("Number of nodes")
    ax.set_ylabel("Network Power Consumption [mJ]")

    ax.legend()
    ax.grid(True)
    fig.tight_layout()
    fig.savefig("NPplot.png")
    plt.close(fig)

    with open('networkpowercondata.pkl', 'wb') as fp:
        pickle.dump((x, y), fp)
    
    del fig
    del ax
    del y
    del x



if __name__ == "__main__":
    sim()
