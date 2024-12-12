import numpy as np
import matplotlib.pyplot as plt

throughputList = []
maxNumberOfNodes = 150


def sim():
    gTime = 0.4
    broadcast = 7
    headerSize = 2
    packetSize = 254
    timeSlots = 100
    packetAirtime = 11.6
    lampda = 1/10**3

    periodeTime = broadcast + (gTime + packetAirtime) * timeSlots
    maxThroughPerTimeSlot = (headerSize + packetSize) / packetAirtime
    maxThroughForPeriode = (
        (maxThroughPerTimeSlot * timeSlots) / periodeTime) * 8

    print(f"{maxThroughPerTimeSlot=}")
    print(f"{maxThroughForPeriode=}")

    for nodecount in range(1, maxNumberOfNodes):
        print(f"\n{nodecount=}:")
        perNodeairtime = (timeSlots/nodecount) * \
            (packetAirtime + gTime)  # Seconds on air

        print(f"{perNodeairtime=}")

        dutyTimepercentage = perNodeairtime / periodeTime

        print(f"{dutyTimepercentage=}")

        expectedPacketPerNoder = lampda * periodeTime

        print(f"{expectedPacketPerNoder=}")

        packetSent = expectedPacketPerNoder / (timeSlots/nodecount)

        print(f"{packetSent=}")

        if packetSent > 1:
            packetSent = 1

        maxthroughtimeslots = packetSize / (packetAirtime + gTime)

        print(f"{maxthroughtimeslots=}")

        throughPuttimeslot = maxthroughtimeslots * packetSent

        print(f"{throughPuttimeslot=}")

        throughput = ((throughPuttimeslot * timeSlots) / periodeTime) * 8
        throughputList.append(throughput)
        print(f"{throughput=}")

    x = list(range(1, maxNumberOfNodes))
    y = throughputList
    print(f"{x=}, {y=}")
    plt.plot(x, y)
    plt.axhline(maxThroughForPeriode, linestyle="dashed",
                label="Max Throughput", color="red")
    plt.show()


if __name__ == "__main__":
    sim()
