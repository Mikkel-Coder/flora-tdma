def sim(packet_size: int) -> list[int, float]:
    max_number_of_nodes = 150
    guard_time = 0.4
    broadcast = 7
    header_size = 2
    time_slots = 100
    packet_airtime = 11.6
    lambda_value = 1/10**3

    time_of_period = broadcast + (guard_time + packet_airtime) * time_slots

    shortest_airtime = (0.401) + (packet_size * 8) / 183.1 # preamble + data rate

    max_throughput_for_packet_size = (packet_size * 8 / shortest_airtime)

    #print(f"{max_throughput_for_packet_size=}")

    throughput_list = []

    packetgen_break = 0

    for nodecount in range(1, max_number_of_nodes):
        # print(f"\n{nodecount=}:")
        # perNodeairtime = (timeSlots/nodecount) * \
        #     (packetAirtime + gTime)  # Seconds on air

        # print(f"{perNodeairtime=}")

        # dutyTimepercentage = perNodeairtime / periodeTime

        # print(f"{dutyTimepercentage=}")

        expectedPacketPerNoder = lambda_value * time_of_period

        # print(f"{expectedPacketPerNoder=}")

        packetSent = expectedPacketPerNoder / (time_slots/nodecount)

        # print(f"{packetSent=}")

        if packetSent > 1:
            if packetgen_break == 0:
                packetgen_break = nodecount
            packetSent = 1

        maxthroughtimeslots = packet_size / (packet_airtime + guard_time)

        # print(f"{maxthroughtimeslots=}")

        throughPuttimeslot = maxthroughtimeslots * packetSent

        # print(f"{throughPuttimeslot=}")

        throughput = throughPuttimeslot * 8
        throughput_list.append(throughput)
        #print(f"{throughput=}")

    x = list(range(1, max_number_of_nodes))
    y = throughput_list

    # fig, ax = plt.subplots()

    # ax.plot(x, y, color="blue", label="Throughput")
    # ax.set_title("Theoretically Throughtput for node count " +
    #              f"[Packet Size: {packet_size}]")
    # ax.set_xlabel("Number of nodes")
    # ax.set_ylabel("Throughput [bps]")

    # # ax.axhline(max_throughput_possible, linestyle="dashed",
    # #             label="Max Throughput Possible", color="red")
    # ax.axhline(max_throughput_for_packet_size, linestyle="dashed",
    #            label="Max Throughput For Size", color="orange")
    # ax.axvline(packetgen_break, linestyle="dashed",
    #            label="More packets then we can send", color="green")

    # ax.text(max_number_of_nodes - 1, max_throughput_for_packet_size - 4,
    #         f"{max_throughput_for_packet_size:.2f}", color='orange',
    #         fontsize=12, ha='center', va='center')
    # ax.text(packetgen_break - 4, -0.15,
    #         str(packetgen_break), color='green',
    #         fontsize=12, ha='center', va='center')
    # ax.text(max_number_of_nodes - 8, throughput_list[-1] - 4,
    #         f"{throughput_list[-1]:.2f}", color="blue",
    #         fontsize=12, ha='center', va='center')

    # ax.legend()
    # ax.grid(True)
    # fig.tight_layout()
    # fig.savefig("plot.png")
    # plt.close(fig)

    # with open('throughputdata.pkl', 'wb') as fp:
    #     pickle.dump((x, y), fp)

    # del fig
    # del ax
    #del y
    #del x

    return x, y

if __name__ == "__main__":
    sim(254)
