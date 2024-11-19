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

#include "LoRaGWMac.h"
#include "inet/common/ModuleAccess.h"
#include "../LoRaPhy/LoRaPhyPreamble_m.h"
#include "inet/common/ProtocolTag_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"


namespace flora {

Define_Module(LoRaGWMac);

void LoRaGWMac::initialize(int stage)
{
    MacProtocolBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        // subscribe for the information of the carrier sense
        cModule *radioModule = getModuleFromPar<cModule>(par("radioModule"), this);
        radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radio = check_and_cast<IRadio *>(radioModule);
        waitingForDC = false;
        dutyCycleTimer = new cMessage("Duty Cycle Timer");
        const char *addressString = par("address");
        GW_forwardedDown = 0;
        GW_droppedDC = 0;
        if (!strcmp(addressString, "auto")) {
            // assign automatic address
            address = MacAddress::generateAutoAddress();
            // change module parameter from "auto" to concrete address
            par("address").setStringValue(address.str().c_str());
        }
        else
            address.setAddress(addressString);
    }
    else if (stage == INITSTAGE_LINK_LAYER) {
        radio->setRadioMode(IRadio::RADIO_MODE_TRANSCEIVER);
    }
}

void LoRaGWMac::finish()
{
    recordScalar("GW_forwardedDown", GW_forwardedDown);
    recordScalar("GW_droppedDC", GW_droppedDC);
    cancelAndDelete(dutyCycleTimer);
}


void LoRaGWMac::configureNetworkInterface()
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

void LoRaGWMac::handleSelfMessage(cMessage *msg)
{
    // We only have a dutycycletimer that when rings we stop waiting for the DC
    if(msg == dutyCycleTimer) waitingForDC = false;
}

void LoRaGWMac::handleUpperMessage(cMessage *msg)
{
    // If we arent waiting for the DC
    if(waitingForDC == false)
    {
        // Make the message a packet and get the LoRaMacFrame at the start of it
        auto pkt = check_and_cast<Packet *>(msg);
        const auto &frame = pkt->peekAtFront<LoRaMacFrame>();

        // Remove the control info if it exists
        if (pkt->getControlInfo())
            delete pkt->removeControlInfo();

        // Making sure to tag the packet (for stats or something like that)
        auto tag = pkt->addTagIfAbsent<MacAddressReq>();
        tag->setDestAddress(frame->getReceiverAddress());

        // We are now waiting for the DC, so set the variable
        waitingForDC = true;

        // Set delta based on spreading factor
        // TODO: refactor to a switch statement
        double delta;
        if(frame->getLoRaSF() == 7) delta = 0.61696;
        if(frame->getLoRaSF() == 8) delta = 1.23392;
        if(frame->getLoRaSF() == 9) delta = 2.14016;
        if(frame->getLoRaSF() == 10) delta = 4.28032;
        if(frame->getLoRaSF() == 11) delta = 7.24992;
        if(frame->getLoRaSF() == 12) delta = 14.49984;

        // Schedule the dutycycletimer to ring in delta seconds
        scheduleAt(simTime() + delta, dutyCycleTimer);

        // Counter, tagging and sending the packet to the lower layer
        GW_forwardedDown++;
        pkt->addTagIfAbsent<PacketProtocolTag>()->setProtocol(&Protocol::apskPhy);
        sendDown(pkt);
    }
    else // If we where waiting for the DC drop the packet
    {
        GW_droppedDC++;
        delete msg;
    }
}

void LoRaGWMac::handleLowerMessage(cMessage *msg)
{
    // Make the message a packet and get the Preamble and macframe from it
    auto pkt = check_and_cast<Packet *>(msg);
    auto header = pkt->popAtFront<LoRaPhyPreamble>();
    const auto &frame = pkt->peekAtFront<LoRaMacFrame>();

    // Check if it is broadcasted and send it up else drop the packet
    if(frame->getReceiverAddress() == MacAddress::BROADCAST_ADDRESS)
        sendUp(pkt);
    else
        delete pkt;
}

void LoRaGWMac::sendPacketBack(Packet *receivedFrame)
{
    // Make a packet to send back to the sender
    const auto &frame = receivedFrame->peekAtFront<LoRaMacFrame>();
    EV << "sending Data frame back" << endl;
    auto pktBack = new Packet("LoraPacket");
    auto frameToSend = makeShared<LoRaMacFrame>();
    frameToSend->setChunkLength(B(par("headerLength").intValue()));

    frameToSend->setReceiverAddress(frame->getTransmitterAddress());
    pktBack->insertAtFront(frameToSend);
    sendDown(pktBack);
}

void LoRaGWMac::receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details)
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

MacAddress LoRaGWMac::getAddress()
{
    // No explanation needed
    return address;
}

}
