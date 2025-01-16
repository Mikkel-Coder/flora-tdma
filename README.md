# lora-tdma
flora-tdma is a proof-of-concept (PoC) implementation of Time Division Multiple Access (TDMA) based on [FLoRa](https://github.com/florasim/flora), [OMNeT++](https://omnetpp.org/), and [INET](https://inet.omnetpp.org/).

![LoRa-TDMA simulation](lora-tdma.gif)

## Description
The LoRaTDMAGW (gateway) starts by broadcasting a static timeslot allocation table of 100 slots, using a round-robbin inspired algorithm divided among the predefined number of participating nodes. Every node then receives the broadcast checking what (if any) timeslot(s) it has been designated to.

A timeslot is 12 seconds, based on worst-case scenarios and real life limitations using LoRa modulation with spreading factor $SF = 12$, bandwidth $BW = 125 kHz$, and coding rate $CR = 4$, including a simulated clock deviation of $30ppm$. The LoRaTMDGW broadcast takes 6.42 seconds. This means that a TDMA-cycle takes $12 \cdot 100+6.7 ≈ 1206s ≈ 20 min$, repeating indefinitely.

The probability of a LoRaTDMA nodes application sending a packet is simulated using a Poisson process with a lambda of $1 \cdot 10^{-6}ms$. LoRaTMDA nodes always have a packet to send in the beginning of a simulation.