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
        numOfTimeslots = par("numOfTimeslots");
        txslotDuration = par("txslotDuration");
        rxslotDuration = par("rxslotDuration");
        broadcastGuard = par("broadcastGuard");
        firstTxSlot = par("firstTxSlot");

        startBroadcast = new cMessage("startBroadcast");
        endBroadcast = new cMessage("endBroadcast");

        timeslots = new std::vector<MacAddress>(50);

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
        
        scheduleAt(firstTxSlot, startBroadcast);
        scheduleAt(firstTxSlot + txslotDuration, endBroadcast);
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
            if (i > 1000) {
                throw cRuntimeError("Too many clients");
            }
        }
        radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
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
    // Make the message a packet and get the Preamble and macframe from it
    auto pkt = check_and_cast<Packet *>(msg);
    auto header = pkt->popAtFront<LoRaPhyPreamble>();
    const auto &frame = pkt->peekAtFront<LoRaTDMAMacFrame>();
    EV << "Received packet: " << pkt << endl;
    EV << "HEADER: " << header << endl;
    EV << "MAC FRAME: " << frame << endl;
}

void LoRaTDMAGWMac::createTimeslots() {
    // Make sure that the timeslots are empty
    timeslots->clear();

    // TODO: make this not a loop and something more intelligent
    // Clients 300+ do not have timeslots
    for (size_t i = 0; i < 300; i++) {
        timeslots->push_back(clients[i]);
    }

    EV_DETAIL << "Generated timeslots: " << timeslots << endl;
    ASSERT(timeslots->size() == 300);
}

void LoRaTDMAGWMac::handleState(cMessage *msg)
{
    switch (macState)
    {
    case INIT:
        EV_DETAIL << "Mac Initialized, transition: INIT -> SLEEP" << endl;
        macState = RECEIVE;
        break;

    case TRANSMIT:
        if (msg == endBroadcast) {
            radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
            EV_DETAIL << "transition: TRANSMIT -> RECEIVE" << endl;
            macState = RECEIVE;
            // Schedule next broadcast
            simtime_t txStartTime = simTime() + rxslotDuration*numOfTimeslots + broadcastGuard;
            simtime_t txEndTime = txStartTime + txslotDuration;
            EV << "Full time offset for start transmit is: " << txStartTime << endl;
            EV << "Full time offset for end transmit is: " << txEndTime << endl;
            scheduleAt(txStartTime, startBroadcast);
            scheduleAt(txEndTime, endBroadcast);
        }
        break;
    
    case RECEIVE:
        if (msg == startBroadcast)
        {
            radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);
            EV_DETAIL << "transition: RECEIVE -> TRANSMIT" << endl;
            macState = TRANSMIT;

            Packet *pkt = new Packet("Gateway Broadcast");
            IntrusivePtr<LoRaTDMAGWFrame> frame = makeShared<LoRaTDMAGWFrame>();
            frame->setTransmitterAddress(address);
            frame->setSyncTime(SIMTIME_AS_CLOCKTIME(simTime()) + ClockTime(17.440768)); // FIXME: Calculated the extra time
            createTimeslots();
            std::vector<MacAddress>& vecRef = *timeslots;
            for (size_t i = 0; i < timeslots->size(); i++) {
                frame->setTimeslots(i, vecRef[i]);
            }
            frame->setChunkLength(B(379)); // Calculated for now
            pkt->insertAtFront(frame);
            pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);

            sendDown(pkt);
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
