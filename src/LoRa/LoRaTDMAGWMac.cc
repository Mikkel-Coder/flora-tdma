//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "LoRaTDMAGWMac.h"
#include "inet/common/ModuleAccess.h"
#include "../LoRaPhy/LoRaPhyPreamble_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"


namespace flora_tdma {

Define_Module(LoRaTDMAGWMac);

void LoRaTDMAGWMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        // subscribe for the information of the carrier sense
        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        // radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radio = check_and_cast<IRadio *>(radioModule);
        const char *addressString = par("address");
        GW_forwardedDown = 0;
        GW_droppedDC = 0;
        txslotDuration = par("txslotDuration");
        rxslotDuration = par("rxslotDuration");
        broadcastGuard = par("broadcastGuard");
        startTransmitOffset = par("startTransmitOffset");
        firstTXSlot = par("firstTXSlot");

        startTXSlot = new cMessage("startTXSlot");
        endTXSlot = new cMessage("endTXSlot");
        startTransmit = new cMessage("startTransmit");

        timeslots = new std::vector<MacAddress>(900);

        if (!strcmp(addressString, "auto")) {
            // assign automatic address
            address = MacAddress::generateAutoAddress();
            // change module parameter from "auto" to concrete address
            par("address").setStringValue(address.str().c_str());
        } else {
            address.setAddress(addressString);
        }

        // state variables
        macState = INIT;
        
        scheduleAt(firstTXSlot, startTXSlot);
        scheduleAt(firstTXSlot + txslotDuration, endTXSlot);
        scheduleAt(firstTXSlot + startTransmitOffset, startTransmit);
        handleState(nullptr);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        // This should populate the client array with macadresses
        size_t i = 0;
        cModule *network = cSimulation::getActiveSimulation()->getSystemModule();
        for (SubmoduleIterator it(network); !it.end(); ++it) {
            cModule *mod = *it;
            EV_DETAIL << "Searching in " << mod << endl;

            cModule *nicMod = mod->getSubmodule("LoRaNic");
            if (nicMod != nullptr) {
                // It has a LoRaNic
                EV_DETAIL << "Found nic: " << nicMod << endl;
                cModule *macMod = nicMod->getSubmodule("mac");
                if (macMod != nullptr) {
                    // Found a mac module
                    EV_DETAIL << "Found mac: " << macMod << endl;
                    LoRaTDMAMac *nodeMac = dynamic_cast<LoRaTDMAMac *>(macMod);
                    MacAddress nodeAddress = nodeMac->getAddress();
                    EV_DETAIL << "Node address: " << nodeAddress << endl;
                    clients[i++] = nodeAddress;
                }
            }
            if (i > MAX_MAC_ADDR_GW_FRAME) {
                throw cRuntimeError("Too many clients");
            }
        }
        numberOfNodes = i;
        EV << "Number of nodes in this simulation is: " << numberOfNodes << endl;
        radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
        nextNodeInTimeSlotQueue = 0;
    }
}

void LoRaTDMAGWMac::finish()
{
}


void LoRaTDMAGWMac::configureNetworkInterface()
{
    // Get the mac address from parameter
    MacAddress address = parseMacAddressParameter(par("address"));

    // generate a link-layer address to be used as interface token for IPv6
    networkInterface->setMacAddress(address);

    // set capabilities
    networkInterface->setMtu(par("mtu"));
    networkInterface->setMulticast(true);
    networkInterface->setBroadcast(true);
    networkInterface->setPointToPoint(false);
}

void LoRaTDMAGWMac::handleSelfMessage(cMessage *msg)
{
    EV << "Received self message: " << msg << endl;
    handleState(msg);
}

void LoRaTDMAGWMac::handleLowerMessage(cMessage *msg)
{
    if (macState == RECEIVE)
    {
        // Make the message a packet and get the Preamble and macframe from it
        auto pkt = check_and_cast<Packet *>(msg);
        auto header = pkt->popAtFront<LoRaPhyPreamble>();
        const auto &frame = pkt->peekAtFront<LoRaTDMAMacFrame>();
        EV << "Received packet: " << pkt << endl;
        EV << "HEADER: " << header << endl;
        EV << "MAC FRAME: " << frame << endl;   
    } else {
        EV << "Got message from lower layer: " << msg << ". But not in RECEIVE, discarding" << endl;
        EV_DEBUG << "macState: " << macState << endl;
        delete msg;
    }
    
}

void LoRaTDMAGWMac::createTimeslots() {
    // Make sure that the timeslots are empty
    timeslots->clear();

    // TODO: make this not a loop and something more intelligent
    // Clients 300+ do not have timeslots. They should have, now define by MAX_MAC_ADDR_GW_FRAME
    
    // Continue in a repeating order to fill the timeslots up for max utilization 
    size_t nodeIndex;
    for (size_t i = 0; i < 900; i++) {
        nodeIndex = (i + nextNodeInTimeSlotQueue) % numberOfNodes;
        timeslots->push_back(clients[nodeIndex]);
    }

    // Remember what lora node we got to and continue from there next time
    nextNodeInTimeSlotQueue = (nodeIndex+1);
    EV << "Next node MAC to send is: " << clients[(nextNodeInTimeSlotQueue % numberOfNodes)] << endl;

    EV_DETAIL << "Generated timeslots" << endl;
    std::vector<MacAddress>& vecRef = *timeslots;
    for (size_t i = 0; i < timeslots->size(); i++)
    {
        EV_DEBUG << "timeslot[" << i << "] = " << vecRef[i] << endl;
    }
    
    ASSERT(timeslots->size() == 900);
}

void LoRaTDMAGWMac::handleState(cMessage *msg)
{
    switch (macState)
    {
    case INIT:
        EV_DETAIL << "Mac Initialized, transition: INIT -> RECEIVE" << endl;
        macState = RECEIVE;
        break;

    case TRANSMIT:
        if (msg == startTransmit) {
            Packet *pkt = new Packet("GatewayBroadcast");
            IntrusivePtr<LoRaTDMAGWFrame> frame = makeShared<LoRaTDMAGWFrame>();
            frame->setTransmitterAddress(address);
            frame->setSyncTime(SIMTIME_AS_CLOCKTIME(simTime()) + ClockTime(49.946624)); // FIXME: Calculated the extra time
            frame->setUsedTimeSlots(900);
            createTimeslots();
            std::vector<MacAddress>& vecRef = *timeslots;
            for (size_t i = 0; i < timeslots->size(); i++) {
                frame->setTimeslots(i, vecRef[i]);
            }
            frame->setChunkLength(b(10+16+10*900)); // Calculated for now
            pkt->insertAtFront(frame);
            pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);

            sendDown(pkt);
        } else if (msg == endTXSlot) {
            radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
            EV_DETAIL << "transition: TRANSMIT -> RECEIVE" << endl;
            macState = RECEIVE;
            // Schedule next broadcast
            simtime_t txStartTime = simTime() + rxslotDuration*900 + broadcastGuard; // Check if broadcast does not exceed 20sec in total because it is now dynamic
            simtime_t txEndTime = txStartTime + txslotDuration;
            EV << "TX slot START time set in simtime: " << txStartTime << endl;
            EV << "TX slot END time set in simtime: " << txEndTime << endl;
            EV << "Start of us transmitting set in simtime: " << txStartTime + startTransmitOffset << endl;
            scheduleAt(txStartTime, startTXSlot);
            scheduleAt(txEndTime, endTXSlot);
            scheduleAt(txStartTime + startTransmitOffset, startTransmit);
        }
        break;
    
    case RECEIVE:
        if (msg == startTXSlot)
        {
            radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
            EV_DETAIL << "transition: RECEIVE -> TRANSMIT" << endl;
            macState = TRANSMIT;
        }
        break;
    
    default:
        throw cRuntimeError("Unknown MAC State: %d", macState);
        break;
    }
}

void LoRaTDMAGWMac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method_Silent(); // TODO: look at this function

    // NOTE: Not sure, but it seems that we switch to RECEIVER or something in this function
    // cant be fully sure to the extend of this function
    if (signalID == IRadio::transmissionStateChangedSignal) {
        IRadio::TransmissionState newRadioTransmissionState = (IRadio::TransmissionState)value;
        if (transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING && newRadioTransmissionState == IRadio::TRANSMISSION_STATE_IDLE) {
            //transmissin is finished
            radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
        }
        transmissionState = newRadioTransmissionState;
    }
}

MacAddress LoRaTDMAGWMac::getAddress()
{
    // No explanation needed
    return address;
}

}
