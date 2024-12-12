import matplotlib.pyplot as plt

power_consumtion = []
max_number_of_nodes = 150
print("hi")


def sim():
    guard_time = 0.4
    broadcast = 7
    packet_airtime = 11.6
    packet_size = 20
    max_packet_size = 256
    packet_bits = packet_size*8
    timeslots = 100
    lampda = 1/10**3

    periode_time = broadcast + (guard_time + packet_airtime) * timeslots

    # broadcastConsumtion = 0.03201  # Wattage to receive broadcast
    # transConsumtion = 0.1452  # wattage to send with 20 bytes of data + header
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
        power_consumtion.append(power_consumed)
        print(f"{power_consumed}")

    x = list(range(1, max_number_of_nodes))
    y = power_consumtion
    print(f"{x=},{y=}")
    plt.plot(x, y)

    plt.legend()
    plt.show()


if __name__ == "__main__":
    sim()
