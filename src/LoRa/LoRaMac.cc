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
#include "LoRaMac.h"
#include "LoRaTagInfo_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/linklayer/common/InterfaceTag_m.h"


namespace flora_tdma {

Define_Module(LoRaMac);

LoRaMac::~LoRaMac()
{
    /* self cMessages */
    cancelAndDelete(endTransmission);
    cancelAndDelete(endReception);
    cancelAndDelete(droppedPacket);
    cancelAndDelete(endDelay_1);
    cancelAndDelete(endListening_1);
    cancelAndDelete(endDelay_2);
    cancelAndDelete(endListening_2);
    cancelAndDelete(mediumStateChange);

    /* What about the Queue? Perhaps clearQueue() */
}

/*
 * Initialization functions.
 */
void LoRaMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        EV << "Initializing stage 0\n";

        /* Parameters are set by there values from the ini file */
        headerLength = par("headerLength"); /* Should be removed in the future */

        waitDelay1Time = 1;
        listening1Time = 1;
        waitDelay2Time = 1;
        listening2Time = 1;

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
        endTransmission = new cMessage("Transmission");
        endReception = new cMessage("Reception");
        droppedPacket = new cMessage("Dropped Packet");
        endDelay_1 = new cMessage("Delay_1");
        endListening_1 = new cMessage("Listening_1");
        endDelay_2 = new cMessage("Delay_2");
        endListening_2 = new cMessage("Listening_2");
        mediumStateChange = new cMessage("MediumStateChange");

        // set up internal queue
        txQueue = getQueue(gate(upperLayerInGateId));

        // state variables
        fsm.setName("LoRaMac State Machine");

        // sequence number for messages
        sequenceNumber = 0;

        // statistics
        numSent = 0;
        numReceived = 0;

        // initialize watches
        WATCH(fsm);
        WATCH(numSent);
        WATCH(numReceived);
    }
    // TODO: Use the function isInitializeStage()
    else if (stage == INITSTAGE_LINK_LAYER)
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
}

void LoRaMac::finish()
{
    recordScalar("numSent", numSent);
    recordScalar("numReceived", numReceived);
}

/*
 * Configures the Inet interface to represent LoRa's capabilities. 
 */
void LoRaMac::configureNetworkInterface()
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
void LoRaMac::handleSelfMessage(cMessage *msg)
{
    EV << "received self message: " << msg << endl;
    handleWithFsm(msg);
}

void LoRaMac::handleUpperPacket(Packet *packet)
{
    if(fsm.getState() != IDLE) {
         error("Wrong, it should not happen erroneous state: %s", fsm.getStateName());
    }
    packet->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);

    EV << "frame " << packet << " received from higher layer " << endl;
    auto pktEncap = encapsulate(packet);
    const auto &frame = pktEncap->peekAtFront<LoRaMacFrame>();
    if (frame == nullptr)
        throw cRuntimeError("Header LoRaMacFrame not found");

    if (currentTxFrame != nullptr)
        throw cRuntimeError("Model error: incomplete transmission exists");
    currentTxFrame = pktEncap;
    handleWithFsm(currentTxFrame);
}

void LoRaMac::handleLowerPacket(Packet *msg)
{
    if( (fsm.getState() == RECEIVING_1) || (fsm.getState() == RECEIVING_2)) handleWithFsm(msg);
    else delete msg;
}

void LoRaMac::processUpperPacket()
{
    Packet *packet = dequeuePacket();
    handleUpperMessage(packet);
}

/*
 * Who uses this? Delete if possible
 * Required by IPassivePacketSink
 */
queueing::IPassivePacketSource *LoRaMac::getProvider(cGate *gate)
{
    return (gate->getId() == upperLayerInGateId) ? txQueue.get() : nullptr;
}

/*
 * Check if this can be deleted/removed
 * Required to be implemented by IActivePacketSink
 */
void LoRaMac::handleCanPullPacketChanged(cGate *gate)
{
    Enter_Method("handleCanPullPacketChanged");
    if (fsm.getState() == IDLE && !txQueue->isEmpty()) {
        processUpperPacket();
    }
}

/*
 * Check if this can be deleted/removed
 * Required to be implemented by IActivePacketSink
 */
void LoRaMac::handlePullPacketProcessed(Packet *packet, cGate *gate, bool successful)
{
    Enter_Method("handlePullPacketProcessed");
    throw cRuntimeError("Not supported callback");
}

/*
 * This is used to handle a LoRaWAN class A end device with its 
 */
void LoRaMac::handleWithFsm(cMessage *msg)
{
    Ptr<LoRaMacFrame>frame = nullptr;

    auto pkt = dynamic_cast<Packet *>(msg);
    if (pkt) {
        const auto &chunk = pkt->peekAtFront<Chunk>();
        frame = dynamicPtrCast<LoRaMacFrame>(constPtrCast<Chunk>(chunk));
    }
    FSMA_Switch(fsm)
    {
        FSMA_State(IDLE)
        {
            FSMA_Enter(turnOffReceiver());
            FSMA_Event_Transition(Idle-Transmit,
                                  isUpperMessage(msg),
                                  TRANSMIT,
            );
        }
        FSMA_State(TRANSMIT)
        {
            FSMA_Enter(sendDataFrame(getCurrentTransmission()));
            FSMA_Event_Transition(Transmit-Wait_Delay_1,
                                  msg == endTransmission,
                                  WAIT_DELAY_1,
                finishCurrentTransmission();
                numSent++;
            );
        }
        FSMA_State(WAIT_DELAY_1)
        {
            FSMA_Enter(turnOffReceiver());
            FSMA_Event_Transition(Wait_Delay_1-Listening_1,
                                  msg == endDelay_1 || endDelay_1->isScheduled() == false,
                                  LISTENING_1,
            );
        }
        /* LoRaWAN window 1*/
        FSMA_State(LISTENING_1)
        {
            FSMA_Enter(turnOnReceiver());
            FSMA_Event_Transition(Listening_1-Wait_Delay_2,
                                  msg == endListening_1 || endListening_1->isScheduled() == false,
                                  WAIT_DELAY_2,
            );
            FSMA_Event_Transition(Listening_1-Receiving1,
                                  msg == mediumStateChange && isReceiving(),
                                  RECEIVING_1,
            );
        }
        FSMA_State(RECEIVING_1)
        {
            /* Check if the frame not for us */
            FSMA_Event_Transition(Receive-Unicast-Not-For, /* This name does not make any sense */
                                  isLowerMessage(msg) && !isForUs(frame),
                                  LISTENING_1,
            );
            FSMA_Event_Transition(Receive-Unicast,
                                  isLowerMessage(msg) && isForUs(frame),
                                  IDLE,
                sendUp(decapsulate(pkt));
                numReceived++;
                cancelEvent(endListening_1); // LoRaWAN window 2 has been canceled and the rest
                cancelEvent(endDelay_2);
                cancelEvent(endListening_2);
            );
            FSMA_Event_Transition(Receive-BelowSensitivity,
                                  msg == droppedPacket,
                                  LISTENING_1,
            );

        }
        FSMA_State(WAIT_DELAY_2)
        {
            FSMA_Enter(turnOffReceiver());
            FSMA_Event_Transition(Wait_Delay_2-Listening_2,
                                  msg == endDelay_2 || endDelay_2->isScheduled() == false,
                                  LISTENING_2,
            );
        }
        /* LoRaWAN window 2*/
        FSMA_State(LISTENING_2)
        {
            FSMA_Enter(turnOnReceiver());
            FSMA_Event_Transition(Listening_2-idle,
                                  msg == endListening_2 || endListening_2->isScheduled() == false,
                                  IDLE,
            );
            FSMA_Event_Transition(Listening_2-Receiving2,
                                  msg == mediumStateChange && isReceiving(),
                                  RECEIVING_2,
            );
        }
        FSMA_State(RECEIVING_2)
        {
            FSMA_Event_Transition(Receive2-Unicast-Not-For,
                                  isLowerMessage(msg) && !isForUs(frame),
                                  LISTENING_2,
            );
            FSMA_Event_Transition(Receive2-Unicast,
                                  isLowerMessage(msg) && isForUs(frame),
                                  IDLE,
                sendUp(pkt);
                numReceived++;
                cancelEvent(endListening_2);
            );
            FSMA_Event_Transition(Receive2-BelowSensitivity,
                                  msg == droppedPacket,
                                  LISTENING_2,
            );
        }
    }

    /* If we are idle, check if we can continue by: */
    if (fsm.getState() == IDLE) {

        /* if we are receiving, then change state in the FSM */
        if (isReceiving())
            handleWithFsm(mediumStateChange);

        /* If we have something to process, then process it in the FSM */
        else if (currentTxFrame != nullptr)
            handleWithFsm(currentTxFrame);
        
        /* If someone form the upper layer has given us something*/
        else if (!txQueue->isEmpty()) {
            /* Then process it (try it as a packet) */
            processUpperPacket();
        }
    }

    /* God only knows what happens here */
    if (endSifs) {
        if (isLowerMessage(msg) && pkt->getOwner() == this && (endSifs->getContextPointer() != pkt))
            delete pkt;
    }
    else {
        if (isLowerMessage(msg) && pkt->getOwner() == this)
            delete pkt;
    }
    getDisplayString().setTagArg("t", 0, fsm.getStateName());
}

/*
 *  This is used to receive signalIDs from the LoRaRadio.
 *  We update our own FSM and Radio to reflect the right states
 */
void LoRaMac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
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
        handleWithFsm(mediumStateChange); // Our own cMessage
    }

    /* Check if we would drop the packet */
    else if (signalID == LoRaRadio::droppedPacket) {
        /* If so then sleep the Radio and drop using our own FSM */
        radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        handleWithFsm(droppedPacket);
    }

    /* Check if we are transmitting */
    else if (signalID == IRadio::transmissionStateChangedSignal) {
        IRadio::TransmissionState newRadioTransmissionState = (IRadio::TransmissionState)value;

        /* If our transmissionState is transmitting & our Radio state is idle */
        if (transmissionState == IRadio::TRANSMISSION_STATE_TRANSMITTING && newRadioTransmissionState == IRadio::TRANSMISSION_STATE_IDLE) {
            /* Then end our current transmission state */
            handleWithFsm(endTransmission);
            /* And update the Radio to sleep mode */
            radio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
        }
        /* And remember to update our */
        transmissionState = newRadioTransmissionState;
    }
}

Packet *LoRaMac::encapsulate(Packet *msg)
{
    auto frame = makeShared<LoRaMacFrame>();
    frame->setChunkLength(B(headerLength));
    msg->setArrival(msg->getArrivalModuleId(), msg->getArrivalGateId());
    auto tag = msg->getTag<LoRaTag>();

    /* For TDMA it would perhaps be better to have this in the LoRaPhy preamble,
     * but i do not think it is possible
     */
    frame->setTransmitterAddress(address);
    frame->setLoRaTP(tag->getPower().get());
    frame->setLoRaCF(tag->getCenterFrequency());
    frame->setLoRaSF(tag->getSpreadFactor());
    frame->setLoRaBW(tag->getBandwidth());
    frame->setLoRaCR(tag->getCodeRendundance());
    frame->setSequenceNumber(sequenceNumber);
    frame->setReceiverAddress(MacAddress::BROADCAST_ADDRESS);

    ++sequenceNumber;
    frame->setLoRaUseHeader(tag->getUseHeader());

    msg->insertAtFront(frame);

    return msg;
}

Packet *LoRaMac::decapsulate(Packet *frame)
{
    auto loraHeader = frame->popAtFront<LoRaMacFrame>();
    frame->addTagIfAbsent<MacAddressInd>()->setSrcAddress(loraHeader->getTransmitterAddress());
    frame->addTagIfAbsent<MacAddressInd>()->setDestAddress(loraHeader->getReceiverAddress());
    frame->addTagIfAbsent<InterfaceInd>()->setInterfaceId(networkInterface->getInterfaceId());
    return frame;
}

/*
 * Frame sender functions.
 */
void LoRaMac::sendDataFrame(Packet *frameToSend)
{
    EV << "sending Data frame\n";
    radio->setRadioMode(IRadio::RADIO_MODE_TRANSMITTER);

    auto frameCopy = frameToSend->dup();

    auto macHeader = frameCopy->peekAtFront<LoRaMacFrame>();

    auto macAddressInd = frameCopy->addTagIfAbsent<MacAddressInd>();
    macAddressInd->setSrcAddress(macHeader->getTransmitterAddress());
    macAddressInd->setDestAddress(macHeader->getReceiverAddress());

    sendDown(frameCopy);
}

/*
 * This is used to schedule LoRaWAN like receiving window 1 and 2 
 */
void LoRaMac::finishCurrentTransmission()
{
    scheduleAt(simTime() + waitDelay1Time, endDelay_1);
    scheduleAt(simTime() + waitDelay1Time + listening1Time, endListening_1);
    scheduleAt(simTime() + waitDelay1Time + listening1Time + waitDelay2Time, endDelay_2);
    scheduleAt(simTime() + waitDelay1Time + listening1Time + waitDelay2Time + listening2Time, endListening_2);
    /* Remember to delete the old frame, as the copy has been sent successfully */
    deleteCurrentTxFrame();
}

Packet *LoRaMac::getCurrentTransmission()
{
    ASSERT(currentTxFrame != nullptr);
    return currentTxFrame;
}

bool LoRaMac::isReceiving()
{
    return radio->getReceptionState() == IRadio::RECEPTION_STATE_RECEIVING;
}


bool LoRaMac::isForUs(const Ptr<const LoRaMacFrame> &frame)
{
    return frame->getReceiverAddress() == address;
}

void LoRaMac::turnOnReceiver()
{
    LoRaRadio *loraRadio;
    loraRadio = check_and_cast<LoRaRadio *>(radio);
    loraRadio->setRadioMode(IRadio::RADIO_MODE_RECEIVER);
}

void LoRaMac::turnOffReceiver()
{
    LoRaRadio *loraRadio;
    loraRadio = check_and_cast<LoRaRadio *>(radio); /* This should be one line */
    loraRadio->setRadioMode(IRadio::RADIO_MODE_SLEEP);
}

/*
 * Do not delete. It is used in LoRaReceiver.cc
 */
MacAddress LoRaMac::getAddress()
{
    return address;
}

} // namespace flora
