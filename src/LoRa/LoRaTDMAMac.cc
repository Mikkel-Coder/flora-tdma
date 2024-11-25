//
// Copyright (C) 2016 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/common/ModuleAccess.h"
#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "inet/linklayer/common/UserPriority.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "LoRaTDMAMac.h"
#include "LoRaTagInfo_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"


namespace flora_tdma {

Define_Module(LoRaTDMAMac);

LoRaTDMAMac::~LoRaTDMAMac()
{
    /* self cMessages */
    cancelAndDelete(startRXSlot);
    cancelAndDelete(endRXSlot);
    cancelAndDelete(startTXSlot);
    cancelAndDelete(endTXSlot);
    cancelAndDelete(endTransmission);
    cancelAndDelete(endReception);

    /* What about the Queue? Perhaps clearQueue() */
}

/*
 * Initialization functions.
 */
void LoRaTDMAMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        EV << "Initializing stage 0\n";

        /* Parameters are set by there values from the ini file */
        headerLength = par("headerLength"); /* Should be removed in the future */

        const char *addressString = par("address");
        if (!strcmp(addressString, "auto")) {
            // assign automatic address
            address = MacAddress::generateAutoAddress();
            // change module parameter from "auto" to concrete address
            par("address").setStringValue(address.str().c_str());
        }
        else {
            address.setAddress(addressString);
        }

        // subscribe for the information of the carrier sense
        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        radioModule->subscribe(IRadio::receptionStateChangedSignal, this);
        radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radioModule->subscribe(LoRaRadio::droppedPacket, this);
        radio = check_and_cast<IRadio *>(radioModule);

        // initialize self messages
        startRXSlot = new cMessage("startRXSlot");
        endRXSlot = new cMessage("endRXSlot");
        startTXSlot = new cMessage("startTXSlot");
        endTXSlot = new cMessage("endTXSlot");
        endTransmission = new cMessage("endTransmission");
        endReception = new cMessage("endReception");

        // set up internal queue
        txQueue = getQueue(gate(upperLayerInGateId));

        // state variables
        macState = INIT;

        // sequence number for messages
        sequenceNumber = 0;

        // statistics
        numSent = 0;
        numReceived = 0;

        // initialize watches
        WATCH(numSent);
        WATCH(numReceived);
    }
    // TODO: Use the function isInitializeStage()
    else if (stage == INITSTAGE_LINK_LAYER)
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
}

void LoRaTDMAMac::finish()
{
    recordScalar("numSent", numSent);
    recordScalar("numReceived", numReceived);
}

/*
 * Configures the Inet interface to represent LoRa's capabilities. 
 */
void LoRaTDMAMac::configureNetworkInterface()
{
    // Set the bitrate limit. By default no limit
    networkInterface->setDatarate(bitrate);

    // Adds an IEEE 802 MAC address
    networkInterface->setMacAddress(address);

    // Capabilities of the physical interface with LoRa 
    networkInterface->setMtu(std::numeric_limits<int>::quiet_NaN()); // There is no MTU limit in LoRa
    networkInterface->setMulticast(true); // Because we are using wireless
    networkInterface->setBroadcast(true);
    networkInterface->setPointToPoint(false); // Because we are always broadcasting
}

/*
 * Message handling functions.
 */
void LoRaTDMAMac::handleSelfMessage(cMessage *msg)
{
    EV << "received self message: " << msg << endl;
    handleState(msg); 
}

void LoRaTDMAMac::handleUpperPacket(Packet *packet)
{
    // MAGIC
}

void LoRaTDMAMac::handleLowerPacket(Packet *msg)
{
    // MAGIC
}

void LoRaTDMAMac::processUpperPacket()
{
    Packet *packet = dequeuePacket();
    handleUpperMessage(packet);
}

/*
 * Who uses this? Delete if possible
 * Required by IPassivePacketSink
 */
queueing::IPassivePacketSource *LoRaTDMAMac::getProvider(cGate *gate)
{
    return (gate->getId() == upperLayerInGateId) ? txQueue.get() : nullptr;
}

/*
 * Check if this can be deleted/removed
 * Required to be implemented by IActivePacketSink
 */
void LoRaTDMAMac::handleCanPullPacketChanged(cGate *gate)
{
    Enter_Method("handleCanPullPacketChanged");
}

/*
 * Check if this can be deleted/removed
 * Required to be implemented by IActivePacketSink
 */
void LoRaTDMAMac::handlePullPacketProcessed(Packet *packet, cGate *gate, bool successful)
{
    Enter_Method("handlePullPacketProcessed");
    throw cRuntimeError("Not supported callback");
}

void LoRaTDMAMac::handleState(cMessage *msg)
{
    switch (macState)
    {
    case INIT:
        EV_DETAIL << "MAC Initialized, transition: INIT -> SLEEP" << endl;
        macState = SLEEP;
        break;

    case SLEEP:
        if (msg == startTXSlot) { // Transmission slot (aka my slot) has begun
            radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);

            // TODO: Make it send

            EV_DETAIL << "transition: SLEEP -> TRANSMIT" << endl;
            macState = TRANSMIT;
        } else if (msg == startRXSlot) { // The gateways broadcast slot (receive slot) has begun
            radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
            EV_DETAIL << "transition: SLEEP -> LISTEN" << endl;
            macState = LISTEN;
        }
        break;

    case TRANSMIT:
        if (msg == endTXSlot) { // End of the transmission slot
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
            EV_DETAIL << "transition: TRANSMIT -> SLEEP" << endl;
            macState = SLEEP;
        }
        break;

    case LISTEN:
        if (msg == endRXSlot) { // End of the receive slot
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
            EV_DETAIL << "transition: LISTEN -> SLEEP" << endl;
            macState = SLEEP;
        }
        break;

    case RECEIVE:
        if (msg == endRXSlot) { // End of the receive slot

            // TODO: cancel reception
            
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
            EV_DETAIL << "transition: RECEIVE -> SLEEP" << endl;
            macState = SLEEP;
        } // TODO: Ignore or smth
        break;
    
    default:
        throw cRuntimeError("Unknown MAC State: %d", macState);
        break;
    }
}

/*
 *  This is used to receive signalIDs from the LoRaRadio.
 *  We update our own FSM and Radio to reflect the right states
 */
void LoRaTDMAMac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method_Silent(); // this is an event but do not animate in GUI
    
    /* Check if we are receiving */
    if (signalID == IRadio::receptionStateChangedSignal) {
        IRadio::ReceptionState newRadioReceptionState = (IRadio::ReceptionState)value;

        /* Remember to also change the FSM in the Radio to sleep */
        /* BUG: We should check if the Radio is in TRANSMISSON_STATE_IDLE */
        if (receptionState == IRadio::RECEPTION_STATE_RECEIVING) {
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        }

        /* Now update our own FSM to the parsed state from Radio */
        receptionState = newRadioReceptionState;
        // handleWithFsm(mediumStateChange); // Our own cMessage
    }

    /* Check if we would drop the packet */
    else if (signalID == LoRaRadio::droppedPacket) {
        /* If so then sleep the Radio and drop using our own FSM */
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
    }

    /* Check if we are transmitting */
    else if (signalID == IRadio::transmissionStateChangedSignal) {
        IRadio::TransmissionState newRadioTransmissionState = (IRadio::TransmissionState)value;

        /* If our transmissionState is transmitting & our Radio state is idle */
        if (transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING && newRadioTransmissionState == IRadio::TRANSMISSION_STATE_IDLE) {
            /* Then end our current transmission state */
            // handleWithFsm(endTransmission);
            /* And update the Radio to sleep mode */
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        }
        /* And remember to update our */
        transmissionState = newRadioTransmissionState;
    }
}

Packet *LoRaTDMAMac::encapsulate(Packet *msg)
{
    auto frame = makeShared<LoRaTDMAMacFrame>();
    frame->setChunkLength(B(headerLength));
    msg->setArrival(msg->getArrivalModuleId(), msg->getArrivalGateId());

    /* For TDMA it would perhaps be better to have this in the LoRaPhy preamble,
     * but i do not think it is possible
     */
    frame->setTransmitterAddress(address);
    ++sequenceNumber;
    msg->insertAtFront(frame);
    return msg;
}

Packet *LoRaTDMAMac::decapsulate(Packet *frame)
{
    auto loraHeader = frame->popAtFront<LoRaTDMAMacFrame>();
    frame->addTagIfAbsent<MacAddressInd>()->setSrcAddress(loraHeader->getTransmitterAddress());
    frame->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
    return frame;
}

/*
 * Frame sender functions.
 */
void LoRaTDMAMac::sendDataFrame(Packet *frameToSend)
{
    EV << "sending Data frame\n";

    auto frameCopy = frameToSend->dup();

    auto macHeader = frameCopy->peekAtFront<LoRaTDMAMacFrame>();

    auto macAddressInd = frameCopy->addTagIfAbsent<MacAddressInd>();
    macAddressInd->setSrcAddress(macHeader->getTransmitterAddress());
    macAddressInd->setDestAddress(MacAddress::BROADCAST_ADDRESS);

    sendDown(frameCopy);
}


Packet *LoRaTDMAMac::getCurrentTransmission()
{
    ASSERT(currentTxFrame != nullptr);
    return currentTxFrame;
}

bool LoRaTDMAMac::isReceiving()
{
    return radio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING;
}


// bool LoRaTDMAMac::isForUs(const Ptr<const LoRaTDMAMacFrame> &frame)
// {
//     return frame->getReceiverAddress() == address;
// }

/*
 * Do not delete. It is used in LoRaReceiver.cc
 */
MacAddress LoRaTDMAMac::getAddress()
{
    return address;
}

} // namespace flora
