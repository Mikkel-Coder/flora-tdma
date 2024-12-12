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

#define CHECKCLEV(clev, value) clev && clev == value

namespace flora_tdma {

Define_Module(LoRaTDMAMac);

LoRaTDMAMac::~LoRaTDMAMac()
{
    /* self cMessages */
    cancelAndDelete(startRXSlot);
    cancelAndDelete(endRXSlot);
    cancelAndDelete(startTXSlot);
    cancelAndDelete(endTXSlot);
    cancelAndDelete(startTransmit);
    // clock->cancelClockEvent(startRXSlot);
    // clock->cancelClockEvent(endRXSlot);
    // clock->cancelClockEvent(startTXSlot);
    // clock->cancelClockEvent(endTXSlot);
    // clock->cancelClockEvent(startTransmit);
    cancelAndDelete(endTransmission);
    cancelAndDelete(endReception);
    cancelAndDelete(mediumStateChange);
    cancelAndDelete(endRXEarly);

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

        txslotDuration = par("txslotDuration");
        rxslotDuration = par("rxslotDuration");
        broadcastGuard = par("broadcastGuard");
        startTransmitOffset = par("startTransmitOffset");
        firstRxSlot = par("firstRxSlot");

        // subscribe for the information of the carrier sense
        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        radioModule->subscribe(IRadio::receptionStateChangedSignal, this);
        radioModule->subscribe(inet::transmissionEndedSignal, this);
        // radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        // radioModule->subscribe(LoRaRadio::droppedPacket, this);
        radio = check_and_cast<IRadio *>(radioModule);

        cModule *clockModule = getModuleFromPar<cModule>(par("clockModule"), this);
        clock = check_and_cast<SettableClock *>(clockModule);

        // initialize self messages
        startRXSlot = new ClockEvent("startRXSlot");
        endRXSlot = new ClockEvent("endRXSlot");
        startTXSlot = new ClockEvent("startTXSlot");
        endTXSlot = new ClockEvent("endTXSlot");
        startTransmit = new ClockEvent("startTransmit");
        endTransmission = new cMessage("endTransmission");
        endReception = new cMessage("endReception");
        mediumStateChange = new cMessage("mediumStateChange");
        endRXEarly = new cMessage("endRXEarly");

        // set up internal queue
        txQueue = getQueue(gate(upperLayerInGateId));

        // state variables
        macState = INIT;

        // sequence number for messages
        // sequenceNumber = 0;

        // statistics
        numSent = 0;
        numReceived = 0;

        // initialize watches
        WATCH(numSent);
        WATCH(numReceived);

        clock->scheduleClockEventAt(firstRxSlot, startRXSlot); // The very first receive to kickstart it all
        clock->scheduleClockEventAt(firstRxSlot + rxslotDuration, endRXSlot); // and then end it at some point
        handleState(nullptr);
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
    EV << "Received self message: " << msg << endl;
    handleState(msg); 
}

void LoRaTDMAMac::handleUpperPacket(Packet *packet)
{
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);
    EV << "frame " << packet << " received from higher layer" << endl;
    Packet *pktEncap = encapsulate(packet);
    if (currentTxFrame != nullptr) {
        throw cRuntimeError("Already have a current txFrame");
    }
    currentTxFrame = pktEncap;
}

void LoRaTDMAMac::handleLowerPacket(Packet *msg)
{
    // TODO: skip reception from other nodes
    
    if (macState == RECEIVE) {

        // Get and decode the LoRaTDMAGWFrame
        const auto &chunk = msg->peekAtFront<Chunk>();
        Ptr<LoRaTDMAGWFrame> frame = dynamicPtrCast<LoRaTDMAGWFrame>(constPtrCast<Chunk>(chunk));


        // Update our clock
        clocktime_t synctime = frame->getSyncTime();
        clock->setClockTime(synctime);

        // Check if we have a time slot
        // TODO: Check and save what receive windows we have been given and use them
        auto timeslotarraysize = frame->getUsedTimeSlots();
        nextTimeSlots = {};
        EV << "The broadcasted timeslot size is: " << timeslotarraysize << endl;
        for (int i = 0; i < timeslotarraysize; i++)
        {
            MacAddress timeslotAddr = frame->getTimeslots(i);
            if (timeslotAddr == address)
            {
                // We found ourself. Note the index. That is the time_index we can transmit
                nextTimeSlots.push(i);
                EV << "We got to TX in slot number: " << i << endl;
            }
        }

        lastRXendTime = endRXSlot->getArrivalClockTime();
        
        if (nextTimeSlots.empty()) {
            EV << "No timeslot for me" << endl;
        }
        else {
            handleNextTXSlot();
        }

        /* Calculate the next receive slot, this is done in similar fashions as the transmit slot
         * But we take the total size of all transmissions in this "round"
         */

        // This does not work, as we wait waaaaayy too long (because there is often not 1000 nodes)
        clocktime_t rxSlotStartTime = txslotDuration*timeslotarraysize + broadcastGuard + lastRXendTime;
        EV << "RX slot START time set on the clock: " << rxSlotStartTime << endl;
        EV << "RX slot END time set on the clock: " << rxSlotStartTime + rxslotDuration << endl;
        clock->cancelClockEvent(endRXSlot); // Cancel the event before rescheduling
        clock->scheduleClockEventAt(rxSlotStartTime, startRXSlot); // Schedule the next receive slot to listen to the gateway
        clock->scheduleClockEventAt(rxSlotStartTime + rxslotDuration, endRXSlot); // And the end
        delete msg;
        handleState(endRXEarly);
    } else {
        EV << "Got message from lower layer: " << msg << ". But not in RECEIVE, discarding" << endl;
        EV_DEBUG << "macState: " << macState << endl;
        delete msg;
    }
    
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
    auto msgclev = dynamic_cast<ClockEvent *>(msg);
    if(msgclev) {
        EV << "Arrival clock time for message: " << msgclev->getArrivalClockTime() << ", simtime for message: " << msgclev->getArrivalTime() << endl;
    }

    switch (macState)
    {
    case INIT:
        EV_DETAIL << "MAC Initialized, transition: INIT -> SLEEP" << endl;
        macState = SLEEP;
        break;

    case SLEEP:
        if (CHECKCLEV(msgclev, startTXSlot)) { // Transmission slot (aka my slot) has begun
            
            if(txQueue->isEmpty()) {
                /* If there is nothing in the queue,
                 * there is no reason to turn on the transmitter and send
                 */
                EV << "Nothing to send, doing nothing" << endl;
                clock->cancelClockEvent(endTXSlot);
                clock->cancelClockEvent(startTransmit);
                if (!nextTimeSlots.empty())
                    handleNextTXSlot();
                return;
            }

            radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);

            EV_DETAIL << "transition: SLEEP -> TRANSMIT" << endl;
            macState = TRANSMIT;

        } else if (CHECKCLEV(msgclev, startRXSlot)) { // The gateways broadcast slot (receive slot) has begun
            radio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
            EV_DETAIL << "transition: SLEEP -> LISTEN" << endl;
            macState = LISTEN;
        }
        break;

    case TRANSMIT:
        if (CHECKCLEV(msgclev, startTransmit)) { // Actually send now
            EV << "Starting to transmit" << endl;
            processUpperPacket();
            ASSERT(currentTxFrame);
            sendDown(currentTxFrame);
        } else if (CHECKCLEV(msgclev, endTXSlot)) { // End of the transmission slot
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
            EV_DETAIL << "transition: TRANSMIT -> SLEEP" << endl;
            macState = SLEEP;
            currentTxFrame = nullptr;
            if (!nextTimeSlots.empty())
                handleNextTXSlot();
        }
        break;

    case LISTEN:
        if (CHECKCLEV(msgclev, endRXSlot)) { // End of the receive slot
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
            EV_DETAIL << "transition: LISTEN -> SLEEP" << endl;
            macState = SLEEP;
        
        // If the radio is in a reception state, we must also receive it
        } else if (msg == mediumStateChange && radio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING) {
            EV_DETAIL << "transition: LISTEN -> RECEIVE" << endl;
            macState = RECEIVE;
        }
        break;

    case RECEIVE:
        if (CHECKCLEV(msgclev, endRXSlot) || msg == endRXEarly) { // End of the receive slot

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

void LoRaTDMAMac::handleNextTXSlot() 
{
    int timeslotIdx = nextTimeSlots.front();
    EV << "Trying to use timeslot: " << timeslotIdx << endl;

    /* Calculate the clock time when we can send, this is based on 3 things:
    * 1. The Duration of a txSlot times our timeslotIdx (so the times of all transmissions before ours)
    * 2. The broadcast guard interval
    * 3. The end of the receive slot (as given by the arrival clock of the endRXSlot)
    */
    clocktime_t txSlotStartTime = txslotDuration*timeslotIdx + broadcastGuard + lastRXendTime;
    EV << "TX slot START time set on the clock: " << txSlotStartTime << endl;
    EV << "TX slot END time set on the clock: " << txSlotStartTime + txslotDuration << endl;
    EV << "Start of us transmitting set on the clock: " << txSlotStartTime + startTransmitOffset << endl;
    clock->scheduleClockEventAt(txSlotStartTime, startTXSlot); // Schedule our transmission slot
    clock->scheduleClockEventAt(txSlotStartTime + txslotDuration, endTXSlot); // Schedule the end of our transmission slot
    clock->scheduleClockEventAt(txSlotStartTime + startTransmitOffset, startTransmit); // The actual point that we start to transmit

    nextTimeSlots.pop();
}

/*
 *  This is used to receive signalIDs from the LoRaRadio.
 *  We update our own FSM and Radio to reflect the right states
 */
void LoRaTDMAMac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
{
    Enter_Method_Silent(); // this is an event but do not animate in GUI

    if(signalID == IRadio::receptionStateChangedSignal) {
        IRadio::ReceptionState newRadioReceptionState = (IRadio::ReceptionState)value;
        if (receptionState == IRadio::RECEPTION_STATE_RECEIVING && newRadioReceptionState == IRadio::RECEPTION_STATE_IDLE) {
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        }

        receptionState = newRadioReceptionState;
        handleState(mediumStateChange);
    } else if (signalID == inet::transmissionEndedSignal) {
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
    }    
}

Packet *LoRaTDMAMac::encapsulate(Packet *msg)
{
    IntrusivePtr<LoRaTDMAMacFrame> frame = makeShared<LoRaTDMAMacFrame>();
    frame->setChunkLength(b(10));

    auto tag = msg->addTagIfAbsent<LoRaTag>();
    tag->setPower(mW(math::dBmW2mW(14)));
    tag->setCenterFrequency(MHz(868));
    tag->setBandwidth(kHz(125));
    tag->setCodeRendundance(4);
    tag->setSpreadFactor(12);
    tag->setUseHeader(true);

    frame->setTransmitterAddress(address);
    msg->insertAtFront(frame);
    return msg;
}

Packet *LoRaTDMAMac::decapsulate(Packet *frame)
// TODO: change or remove
{
    throw cRuntimeError("Not implemented, that is happing?");
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
