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

#include "LoRaRadio.h"
#include "LoRaPhy/LoRaMedium.h"
#include "inet/common/LayeredProtocolBase.h"
#include "inet/common/lifecycle/NodeStatus.h"
#include "inet/common/ModuleAccess.h"
#include "inet/physicallayer/wireless/common/radio/packetlevel/Radio.h"
#include "inet/physicallayer/wireless/common/medium/RadioMedium.h"
#include "LoRaPhy/LoRaTransmitter.h"
#include "LoRaPhy/LoRaReceiver.h"
#include "LoRaMacFrame_m.h"
#include "LoRaTagInfo_m.h"
#include "../LoRaPhy/LoRaPhyPreamble_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

/* Almost the same as LoRaGWRadio.cc */

namespace flora {

Define_Module(LoRaRadio);

simsignal_t LoRaRadio::minSNIRSignal = cComponent::registerSignal("minSNIR");
simsignal_t LoRaRadio::packetErrorRateSignal = cComponent::registerSignal("packetErrorRate");
simsignal_t LoRaRadio::bitErrorRateSignal = cComponent::registerSignal("bitErrorRate");
simsignal_t LoRaRadio::symbolErrorRateSignal = cComponent::registerSignal("symbolErrorRate");
simsignal_t LoRaRadio::droppedPacket = cComponent::registerSignal("droppedPacket");


void LoRaRadio::initialize(int stage)
{
    NarrowbandRadioBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        iAmGateway = par("iAmGateway").boolValue();
    }
}

LoRaRadio::~LoRaRadio() {
}

std::ostream& LoRaRadio::printToStream(std::ostream& stream, int level, int evFlags) const
{
    stream << static_cast<const cSimpleModule *>(this);
    if (level <= PRINT_LEVEL_TRACE)
        stream << ", antenna = " << printFieldToString(antenna, level + 1, evFlags)
               << ", transmitter = " << printFieldToString(transmitter, level + 1, evFlags)
               << ", receiver = " << printFieldToString(receiver, level + 1, evFlags);
    return stream;
}


const ITransmission *LoRaRadio::getTransmissionInProgress() const
{
    if (!transmissionTimer->isScheduled()) {
        return nullptr;
    }
    else {
        return static_cast<WirelessSignal *>(transmissionTimer->getContextPointer())->getTransmission();
    }
}

const ITransmission *LoRaRadio::getReceptionInProgress() const
{
    if (receptionTimer == nullptr) {
        return nullptr;
    }
    else {
        return static_cast<WirelessSignal *>(receptionTimer->getControlInfo())->getTransmission();
    }
}

IRadioSignal::SignalPart LoRaRadio::getTransmittedSignalPart() const
{
    return transmittedSignalPart;
}

IRadioSignal::SignalPart LoRaRadio::getReceivedSignalPart() const
{
    return receivedSignalPart;
}

void LoRaRadio::handleMessageWhenDown(cMessage *message)
{
    if (message->getArrivalGate() == radioIn || isReceptionTimer(message)) {
        delete message;
    }
    else {
        OperationalBase::handleMessageWhenDown(message);
    }
}

void LoRaRadio::handleMessageWhenUp(cMessage *message)
{
    if (message->isSelfMessage())
        handleSelfMessage(message);
    else if (message->getArrivalGate() == upperLayerIn) {
        if (!message->isPacket()) {
            handleUpperCommand(message);
            delete message;
        }
        else
            handleUpperPacket(check_and_cast<Packet *>(message));
    }
    else if (message->getArrivalGate() == radioIn) {
        if (!message->isPacket()) {
            handleLowerCommand(message);
            delete message;
        }
        else
            handleSignal(check_and_cast<WirelessSignal *>(message));
    }
    else
        throw cRuntimeError("Unknown arrival gate '%s'.", message->getArrivalGate()->getFullName());
}

void LoRaRadio::handleSelfMessage(cMessage *message)
{
    /* This should be removed */
    NarrowbandRadioBase::handleSelfMessage(message);
}

void LoRaRadio::handleTransmissionTimer(cMessage *message)
{
    /* I guess that this is the same for inets NarrowBandradio::handleTransmisionTimer.
     * If so, then remove this
     */
    if (message->getKind() == IRadioSignal::SIGNAL_PART_WHOLE)
        endTransmission();
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_PREAMBLE)
        continueTransmission();
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_HEADER)
        continueTransmission();
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_DATA)
        endTransmission();
    else
        throw cRuntimeError("Unknown self message");
}

void LoRaRadio::handleReceptionTimer(cMessage *message)
{
     /* I guess that this is the same for inets NarrowBandradio::handleReceptionTimer.
     * If so, then remove this
     */
    if (message->getKind() == IRadioSignal::SIGNAL_PART_WHOLE)
        endReception(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_PREAMBLE)
        continueReception(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_HEADER)
        continueReception(message);
    else if (message->getKind() == IRadioSignal::SIGNAL_PART_DATA)
        endReception(message);
    else
        throw cRuntimeError("Unknown self message");
}

void LoRaRadio::handleUpperCommand(cMessage *message)
{
    if (message->getKind() == RADIO_C_CONFIGURE) {
        ConfigureRadioCommand *configureCommand = check_and_cast<ConfigureRadioCommand *>(message->getControlInfo());
        if (configureCommand->getRadioMode() != -1)
            setRadioMode((RadioMode)configureCommand->getRadioMode());
    }
    else
        throw cRuntimeError("Unsupported command");
}

void LoRaRadio::handleLowerCommand(cMessage *message)
{
    throw cRuntimeError("Unsupported command");
}

void LoRaRadio::handleUpperPacket(Packet *packet)
{
    emit(packetReceivedFromUpperSignal, packet);
    if (isTransmitterMode(radioMode)) {
        auto tag = packet->removeTag<LoRaTag>();
        auto preamble = makeShared<LoRaPhyPreamble>();

        preamble->setBandwidth(tag->getBandwidth());
        preamble->setCenterFrequency(tag->getCenterFrequency());
        preamble->setCodeRendundance(tag->getCodeRendundance());
        preamble->setPower(tag->getPower());
        preamble->setSpreadFactor(tag->getSpreadFactor());
        preamble->setUseHeader(tag->getUseHeader());
        const auto & loraHeader =  packet->peekAtFront<LoRaMacFrame>();
        preamble->setReceiverAddress(loraHeader->getReceiverAddress());

        auto signalPowerReq = packet->addTagIfAbsent<SignalPowerReq>();
        signalPowerReq->setPower(tag->getPower());

        preamble->setChunkLength(b(16)); // Why 2B? This should be in symbols (Is that not possible in omnet?)
        packet->insertAtFront(preamble);

        if (transmissionTimer->isScheduled())
            throw cRuntimeError("Received frame from upper layer while already transmitting.");
        if (separateTransmissionParts)
            startTransmission(packet, IRadioSignal::SIGNAL_PART_PREAMBLE);
        else
            startTransmission(packet, IRadioSignal::SIGNAL_PART_WHOLE);
    }
    else {
        EV_ERROR << "Radio is not in transmitter or transceiver mode, dropping frame." << endl;
        delete packet;
    }
}

void LoRaRadio::handleSignal(WirelessSignal *radioFrame)
{
    auto receptionTimer = createReceptionTimer(radioFrame);
    if (separateReceptionParts)
        startReception(receptionTimer, IRadioSignal::SIGNAL_PART_PREAMBLE);
    else
        startReception(receptionTimer, IRadioSignal::SIGNAL_PART_WHOLE);
}

void LoRaRadio::startTransmission(Packet *macFrame, IRadioSignal::SignalPart part)
{
    /* This should use NarrowBandRadio method instead */
    NarrowbandRadioBase::startTransmission(macFrame, part);
}

void LoRaRadio::continueTransmission()
{
    /* This should use NarrowBandRadio method instead */
    NarrowbandRadioBase::continueTransmission();
}

void LoRaRadio::endTransmission()
{
    /* This should use NarrowBandRadio method instead */
    NarrowbandRadioBase::endTransmission();
}

void LoRaRadio::abortTransmission()
{
    /* This should use NarrowBandRadio method instead */
    NarrowbandRadioBase::abortTransmission();
}

WirelessSignal *LoRaRadio::createSignal(Packet *packet) const
{
    /* This should use NarrowBandRadio method instead */
    return NarrowbandRadioBase::createSignal(packet);
}

void LoRaRadio::startReception(cMessage *timer, IRadioSignal::SignalPart part)
{
    /* This should use NarrowBandRadio method instead */
    NarrowbandRadioBase::startReception(timer, part);
}

void LoRaRadio::continueReception(cMessage *timer)
{
    /* This should use NarrowBandRadio method instead */
    NarrowbandRadioBase::continueReception(timer);
}

void LoRaRadio::decapsulate(Packet *packet) const
{
    auto tag = packet->addTag<LoRaTag>();
    auto preamble = packet->popAtFront<LoRaPhyPreamble>();

    tag->setBandwidth(preamble->getBandwidth());
    tag->setCenterFrequency(preamble->getCenterFrequency());
    tag->setCodeRendundance(preamble->getCodeRendundance());
    tag->setPower(preamble->getPower());
    tag->setSpreadFactor(preamble->getSpreadFactor());
    tag->setUseHeader(preamble->getUseHeader());
}

void LoRaRadio::endReception(cMessage *timer)
{
    auto part = (IRadioSignal::SignalPart)timer->getKind();
    auto signal = static_cast<WirelessSignal *>(timer->getControlInfo());
    auto arrival = signal->getArrival();
    auto reception = signal->getReception();

    /* If timers are sane and the radio is ready to receive */
    if (timer == receptionTimer && isReceiverMode(radioMode) && arrival->getEndTime() == simTime()) {
        auto transmission = signal->getTransmission();
        // TODO: this would draw twice from the random number generator in isReceptionSuccessful: auto isReceptionSuccessful = medium->isReceptionSuccessful(this, transmission, part);
        auto isReceptionSuccessful = medium->getReceptionDecision(this, signal->getListening(), transmission, part)->isReceptionSuccessful();
        EV_INFO << "Reception ended: " << (isReceptionSuccessful ? "\x1b[1msuccessfully\x1b[0m" : "\x1b[1munsuccessfully\x1b[0m") << " for " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
        auto macFrame = medium->receivePacket(this, signal);
        take(macFrame);
        decapsulate(macFrame);
        if (isReceptionSuccessful)
            sendUp(macFrame);
        else {
            emit(LoRaRadio::droppedPacket, 0);
            delete macFrame;
        }
        receptionTimer = nullptr;
        emit(receptionEndedSignal, check_and_cast<const cObject *>(reception));
    }
    else {
        EV_INFO << "Reception ended: \x1b[1mignoring\x1b[0m " << (IWirelessSignal *)signal << " " << IRadioSignal::getSignalPartName(part) << " as " << reception << endl;
    }
    updateTransceiverState();
    updateTransceiverPart();
    delete timer;
    // TODO: move to radio medium. It is already done?
    check_and_cast<RadioMedium *>(medium.get())->emit(IRadioMedium::signalArrivalEndedSignal, check_and_cast<const cObject *>(reception));
}

void LoRaRadio::abortReception(cMessage *timer)
{
    /* This should use NarrowBandRadio method instead */
    NarrowbandRadioBase::abortReception(timer);
}

void LoRaRadio::captureReception(cMessage *timer)
{
    // TODO: this would be called when the receiver switches to a stronger signal while receiving a weaker one
    throw cRuntimeError("Not yet implemented");
}

void LoRaRadio::sendUp(Packet *macFrame)
{
    auto signalPowerInd = macFrame->findTag<SignalPowerInd>();
    if (signalPowerInd == nullptr)
        throw cRuntimeError("signal Power indication not present");
    auto snirInd =  macFrame->findTag<SnirInd>();
    if (snirInd == nullptr)
        throw cRuntimeError("snir indication not present");

    auto errorTag = macFrame->findTag<ErrorRateInd>();

    emit(minSNIRSignal, snirInd->getMinimumSnir());
    if (errorTag && !std::isnan(errorTag->getPacketErrorRate()))
        emit(packetErrorRateSignal, errorTag->getPacketErrorRate());
    if (errorTag && !std::isnan(errorTag->getBitErrorRate()))
        emit(bitErrorRateSignal, errorTag->getBitErrorRate());
    if (errorTag && !std::isnan(errorTag->getSymbolErrorRate()))
        emit(symbolErrorRateSignal, errorTag->getSymbolErrorRate());
    EV_INFO << "Sending up " << macFrame << endl;
    NarrowbandRadioBase::sendUp(macFrame);
}

} // namespace inet
