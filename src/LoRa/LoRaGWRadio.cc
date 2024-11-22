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

#include "LoRaGWRadio.h"
#include "LoRaPhy/LoRaMedium.h"
#include "LoRaPhy/LoRaPhyPreamble_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"


namespace flora_tdma {

Define_Module(LoRaGWRadio);

void LoRaGWRadio::initialize(int stage)
{
    FlatRadioBase::initialize(stage);
    iAmGateway = par("iAmGateway").boolValue();
    if (stage == INITSTAGE_LAST) {
        setRadioMode(RADIO_MODE_TRANSCEIVER);
        LoRaGWRadioReceptionStarted = registerSignal("LoRaGWRadioReceptionStarted");
        LoRaGWRadioReceptionFinishedCorrect = registerSignal("LoRaGWRadioReceptionFinishedCorrect");
        LoRaGWRadioReceptionStarted_counter = 0;
        LoRaGWRadioReceptionFinishedCorrect_counter = 0;
        iAmTransmiting = false;
    }
}

void LoRaGWRadio::finish()
{
    /* Used to clean up */
    FlatRadioBase::finish();
    recordScalar("DER - Data Extraction Rate", double(LoRaGWRadioReceptionFinishedCorrect_counter)/LoRaGWRadioReceptionStarted_counter);
}

void LoRaGWRadio::handleSelfMessage(cMessage *message)
{
    /* Used to handle our own cMessages (events) */
    if (message == switchTimer) {
        /* Used to end finalize Radio mode switch */
        handleSwitchTimer(message);
    }
    else if (isTransmissionTimer(message)) {
        /* Handle tx timer */
        handleTransmissionTimer(message);
    }
    else if (isReceptionTimer(message)) {
        /* Handle rx timer */
        handleReceptionTimer(message);
    }
    else
        throw cRuntimeError("Unknown self message");
}


bool LoRaGWRadio::isTransmissionTimer(const cMessage *message) const
{
    return !strcmp(message->getName(), "transmissionTimer");
}

void LoRaGWRadio::handleTransmissionTimer(cMessage *message)
{
    /* Almost the same as Radio, but we use message in transmission */
    if (message->getKind() == IRadioSignal::SIGNAL_PART_WHOLE)
        endTransmission(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_PREAMBLE)
        continueTransmission(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_HEADER)
        continueTransmission(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_DATA)
        endTransmission(message);
    else
        throw cRuntimeError("Unknown self message");
}

void LoRaGWRadio::handleUpperPacket(Packet *packet)
{
    /* Used to handle upper layer packets. AKA we add a preamble and send */

    emit(packetReceivedFromUpperSignal, packet);

    EV << packet->getDetailStringRepresentation(evFlags) << endl;

    /* Convert the upper layer packet to a frame and set the right parameters */
    const auto &frame = packet->peekAtFront<LoRaTDMAMacFrame>();
    auto preamble = makeShared<LoRaPhyPreamble>();

    preamble->setBandwidth(frame->getLoRaBW());
    preamble->setCenterFrequency(frame->getLoRaCF());
    preamble->setCodeRendundance(frame->getLoRaCR());
    preamble->setPower(mW(frame->getLoRaTP()));
    preamble->setSpreadFactor(frame->getLoRaSF());
    preamble->setUseHeader(frame->getLoRaUseHeader());
    preamble->setReceiverAddress(frame->getReceiverAddress());

    /* Keep track of the power required to transmit the packet */
    auto signalPowerReq = packet->addTagIfAbsent<SignalPowerReq>();
    signalPowerReq->setPower(mW(frame->getLoRaTP()));

    preamble->setChunkLength(b(16)); /* This is not important because the preamble is 8+4.25 symbols */
    packet->insertAtFront(preamble); /* Be sure to place the preamble at front */
    EV << "Sending with power: " << preamble->getPower() << " and SF: " << preamble->getSpreadFactor() << endl;

    /* inet determines if the signal can be sent as one or multiple parts */
    if (separateTransmissionParts)
        startTransmission(packet, IRadioSignal::SIGNAL_PART_PREAMBLE);
    else
        startTransmission(packet, IRadioSignal::SIGNAL_PART_WHOLE);
}

void LoRaGWRadio::startTransmission(Packet *packet, IRadioSignal::SignalPart part)
{
    /* Used to send a packet determined by its signalpart */

    /* If we are already transmitting, delete the senders packet! */
    if(iAmTransmiting) {
        delete packet;
        return;
    }

    /* Else then send the packet!! */
    iAmTransmiting = true;
    auto radioFrame = createSignal(packet);
    auto transmission = radioFrame->getTransmission();

    /* Notify ourself when this transmission ends*/
    cMessage *txTimer = new cMessage("transmissionTimer");
    txTimer->setKind(part);
    txTimer->setContextPointer(radioFrame);
    scheduleAt(transmission->getEndTime(part), txTimer);
    /* And when then transmission starts */
    emit(transmissionStartedSignal, check_and_cast<const cObject *>(transmission));

    EV_INFO << "Transmission started: " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << transmission << endl;

    /* Fancy trick to get the LoRamedium to emit the omnet signal instead of our self */
    check_and_cast<LoRaMedium *>(medium.get())->emit(IRadioMedium::signalDepartureStartedSignal, check_and_cast<const cObject *>(transmission));
}

void LoRaGWRadio::continueTransmission(cMessage *timer)
{
    /* If the signal we are trying to transmit was not whole, then we need to continue to send it */
    auto previousPart = (IRadioSignal::SignalPart)timer->getKind();
    auto nextPart = (IRadioSignal::SignalPart)(previousPart + 1);
    auto radioFrame = static_cast<IWirelessSignal *>(timer->getContextPointer());
    auto transmission = radioFrame->getTransmission();

    /* Previously tranmission has now been sent. */
    EV_INFO << "Transmission ended: " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(previousPart) << " as " << radioFrame->getTransmission() << endl;

    /* Now schedule to send the next part */
    timer->setKind(nextPart);
    scheduleAt(transmission->getEndTime(nextPart), timer);
    EV_INFO << "Transmission started: " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(nextPart) << " as " << transmission << endl;
}

void LoRaGWRadio::endTransmission(cMessage *timer)
{
    /* Used to end the transmission */
    /* Most code is used for logging */
    iAmTransmiting = false;
    auto part = (IRadioSignal::SignalPart)timer->getKind();
    auto signal = static_cast<WirelessSignal *>(timer->getContextPointer());
    auto transmission = signal->getTransmission();
    timer->setContextPointer(nullptr); /* Important */
    EV_INFO << "Transmission ended: " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << transmission << endl;

    /* Notify ourself that the transmission is over */
    emit(transmissionEndedSignal, check_and_cast<const cObject *>(transmission));

    /* Trick to get the LoRaMedium to end the signal */
    check_and_cast<LoRaMedium *>(medium.get())->emit(IRadioMedium::signalDepartureEndedSignal, check_and_cast<const cObject *>(transmission));
    delete(timer);
}

void LoRaGWRadio::handleSignal(WirelessSignal *radioFrame)
{
    /* Used to handle "lower layer". AKA we receive a signal */

    /* Creates a cMessage, and check if it is only preamble or whole radio frame */
    auto receptionTimer = createReceptionTimer(radioFrame);
    if (separateReceptionParts)
        startReception(receptionTimer, IRadioSignal::SIGNAL_PART_PREAMBLE);
    else
        startReception(receptionTimer, IRadioSignal::SIGNAL_PART_WHOLE);
}

bool LoRaGWRadio::isReceptionTimer(const cMessage *message) const
{
    return !strcmp(message->getName(), "receptionTimer");
}

void LoRaGWRadio::startReception(cMessage *timer, IRadioSignal::SignalPart part)
{
    /* We get back the reception timer and get its values */
    auto radioFrame = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = radioFrame->getArrival();
    auto reception = radioFrame->getReception();

    /* Notify ourself that we are receiving */
    emit(LoRaGWRadioReceptionStarted, true);
    
    /* Used to calculate DER */
    if (simTime() >= getSimulation()->getWarmupPeriod()) {
        LoRaGWRadioReceptionStarted_counter++;
    }

    /* If we are in receivermode, and ready to receive it in time, and we are not currently transmitting, 
       Then we are ready to receive!
    */
    if (isReceiverMode(radioMode) && arrival->getStartTime(part) == simTime() && iAmTransmiting == false) {
        auto transmission = radioFrame->getTransmission();
        auto isReceptionAttempted = medium->isReceptionAttempted(this, transmission, part);
        EV_INFO << "LoRaGWRadio Reception started: " << (isReceptionAttempted ? "attempting" : "not attempting") << " " << (WirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
        if (isReceptionAttempted) {
            if(iAmGateway) {
                /* TODO: idk. We will look back later */
                concurrentReceptions.push_back(timer);
            }
            receptionTimer = timer;
        }
    } 
    else {
        /* Else ignore the reception */
        EV_INFO << "LoRaGWRadio Reception started: ignoring " << (WirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    }

    /* Now we can receive it */
    timer->setKind(part);
    scheduleAt(arrival->getEndTime(part), timer);
    radioMode = RADIO_MODE_TRANSCEIVER;
    check_and_cast<LoRaMedium *>(medium.get())->emit(IRadioMedium::signalArrivalStartedSignal, check_and_cast<const cObject *>(reception));
    if(iAmGateway) {
        EV_INFO << "Start reception, size : " << concurrentReceptions.size() << endl;
    }
}

void LoRaGWRadio::continueReception(cMessage *timer)
{
    /* If we need to receive reception */
    auto previousPart = (IRadioSignal::SignalPart)timer->getKind();
    auto nextPart = (IRadioSignal::SignalPart)(previousPart + 1);
    auto radioFrame = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = radioFrame->getArrival();
    auto reception = radioFrame->getReception();

    /* Note that a gateway can receive more than one reception at a time */
    /* We find the timer for the reception we were already receiving form */
    if(iAmGateway) {
        std::list<cMessage *>::iterator it;
        for (it=concurrentReceptions.begin(); it!=concurrentReceptions.end(); it++) {
            if(*it == timer) {
                receptionTimer = timer;
            }    
        }
    }

    /* Check if the reception timer we found was a match to the expected reception */
    if (timer == receptionTimer && isReceiverMode(radioMode) && arrival->getEndTime(previousPart) == simTime() && iAmTransmiting == false) {
        auto transmission = radioFrame->getTransmission();
        bool isReceptionSuccessful = medium->isReceptionSuccessful(this, transmission, previousPart); /* Magic! Thx inet*/
        EV_INFO << "LoRaGWRadio Reception ended: " << (isReceptionSuccessful ? "successfully" : "unsuccessfully") << " for " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(previousPart) << " as " << reception << endl;
        
        /* Failed to receive the reception */
        if (!isReceptionSuccessful) {
            receptionTimer = nullptr;
            if(iAmGateway) 
                /* cleanup (remove) the timer, as it is invalid now */
                concurrentReceptions.remove(timer);
        }

        auto isReceptionAttempted = medium->isReceptionAttempted(this, transmission, nextPart);
        EV_INFO << "LoRaGWRadio Reception started: " << (isReceptionAttempted ? "attempting" : "not attempting") << " " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(nextPart) << " as " << reception << endl;
        
        /* NOTE: same as before for failed */
        if (!isReceptionAttempted) {
            receptionTimer = nullptr;
            if(iAmGateway) 
                concurrentReceptions.remove(timer);
        }
    }
    else {
        /* Ignore any receptions */
        EV_INFO << "LoRaGWRadio Reception ended: ignoring " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(previousPart) << " as " << reception << endl;
        EV_INFO << "LoRaGWRadio Reception started: ignoring " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(nextPart) << " as " << reception << endl;
    }

    /* Update ourself to start receiving the next */
    timer->setKind(nextPart);
    scheduleAt(arrival->getEndTime(nextPart), timer);
    radioMode = RADIO_MODE_TRANSCEIVER;
}

void LoRaGWRadio::endReception(cMessage *timer)
{
    /* This is used to end an reception */
    /* NOTE: This should be refactored as it is alike to continueReception */

    auto part = (IRadioSignal::SignalPart)timer->getKind();
    auto radioFrame = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = radioFrame->getArrival();
    auto reception = radioFrame->getReception();
    std::list<cMessage *>::iterator it;
    if(iAmGateway) {
        for (it=concurrentReceptions.begin(); it!=concurrentReceptions.end(); it++) {
            if(*it == timer) receptionTimer = timer;
        }
    }
    /* If we found the timer for the same reception, and we are sane */
    if (timer == receptionTimer && isReceiverMode(radioMode) && arrival->getEndTime() == simTime() && iAmTransmiting == false) {
        auto transmission = radioFrame->getTransmission();
// OLD TODO: this would draw twice from the random number generator in isReceptionSuccessful: auto isReceptionSuccessful = medium->isReceptionSuccessful(this, transmission, part); /* idk */
        auto isReceptionSuccessful = medium->getReceptionDecision(this, radioFrame->getListening(), transmission, part)->isReceptionSuccessful();
        EV_INFO << "LoRaGWRadio Reception ended: " << (isReceptionSuccessful ? "successfully" : "unsuccessfully") << " for " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;

        if(isReceptionSuccessful) {
            /* Send the received radioFrame to the upper layers as macFrame */
            auto macFrame = medium->receivePacket(this, radioFrame);
            take(macFrame);
            emit(packetSentToUpperSignal, macFrame);
            emit(LoRaGWRadioReceptionFinishedCorrect, true);
            if (simTime() >= getSimulation()->getWarmupPeriod())
                /* Used for statistics */
                LoRaGWRadioReceptionFinishedCorrect_counter++;
            EV << macFrame->getCompleteStringRepresentation(evFlags) << endl;

            /* Send up to the upper layer */
            sendUp(macFrame);
        }

        /* Cleanup */
        receptionTimer = nullptr;
        if(iAmGateway) concurrentReceptions.remove(timer);
    }
    else {
        /* Log that the reception was ignored */
        EV_INFO << "LoRaGWRadio Reception ended: ignoring " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    }

    /* Remember to change the radio mode, that we are now ready to receive again */
    radioMode = RADIO_MODE_TRANSCEIVER;

    /* Trick to get the LoRaMedium to emit omnet signal vent for end signal */
    check_and_cast<LoRaMedium *>(medium.get())->emit(IRadioMedium::signalArrivalEndedSignal, check_and_cast<const cObject *>(reception));
    delete timer;
}

void LoRaGWRadio::abortReception(cMessage *timer)
{
    /* Who calls this? */
    auto radioFrame = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto part = (IRadioSignal::SignalPart)timer->getKind();
    auto reception = radioFrame->getReception();
    EV_INFO << "LoRaGWRadio Reception aborted: for " << (IWirelessSignal *)radioFrame << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    if (timer == receptionTimer) {
        if(iAmGateway) concurrentReceptions.remove(timer);
        receptionTimer = nullptr;
    }

    /* radioMode = ? instead*/
    updateTransceiverState();
    updateTransceiverPart();
}

}
