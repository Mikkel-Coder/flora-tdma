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

#include "SimpleLoRaApp.h"
#include "inet/mobility/static/StationaryMobility.h"
#include "../LoRa/LoRaTagInfo_m.h"
#include "inet/common/packet/Packet.h"


namespace flora_tdma {

Define_Module(SimpleLoRaApp);

void SimpleLoRaApp::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        std::pair<double,double> coordsValues = std::make_pair(-1, -1);
        cModule *host = getContainingNode(this);

        lambdaApp = par("lambdaApp");

        // Generate random location for nodes if circle deployment type

        // This is not possible to do at runtime
        if (strcmp(host->par("deploymentType").stringValue(), "circle")==0) {
           coordsValues = generateUniformCircleCoordinates(host->par("maxGatewayDistance").doubleValue(), host->par("gatewayX").doubleValue(), host->par("gatewayY").doubleValue());
           StationaryMobility *mobility = check_and_cast<StationaryMobility *>(host->getSubmodule("mobility"));
           mobility->par("initialX").setDoubleValue(coordsValues.first);
           mobility->par("initialY").setDoubleValue(coordsValues.second);
        }
    }
    else if (stage == INITSTAGE_APPLICATION_LAYER) {
        bool isOperational;
        NodeStatus *nodeStatus = dynamic_cast<NodeStatus *>(findContainingNode(this)->getSubmodule("status"));
        isOperational = (!nodeStatus) || nodeStatus->getState() == NodeStatus::UP;
        if (!isOperational)
            throw cRuntimeError("This module doesn't support starting in node DOWN state");
        /* This do loop does not make any sense. It does not seem likely that the timeToFirstPacket will change */
        timeToFirstPacket = SimTime(1);

        //timeToFirstPacket = par("timeToFirstPacket");
        sendMeasurements = new cMessage("sendMeasurements");
        scheduleAt(simTime()+timeToFirstPacket, sendMeasurements);

        sentPackets = 0;
        receivedADRCommands = 0;
        numberOfPacketsToSend = par("numberOfPacketsToSend");

        LoRa_AppPacketSent = registerSignal("LoRa_AppPacketSent");

        //LoRa physical layer parameters
        loRaRadio = check_and_cast<LoRaRadio *>(getParentModule()->getSubmodule("LoRaNic")->getSubmodule("radio"));
        loRaRadio->loRaTP = par("initialLoRaTP").doubleValue();
        loRaRadio->loRaCF = units::values::Hz(par("initialLoRaCF").doubleValue());
        loRaRadio->loRaSF = par("initialLoRaSF");
        loRaRadio->loRaBW = inet::units::values::Hz(par("initialLoRaBW").doubleValue());
        loRaRadio->loRaCR = par("initialLoRaCR");
        loRaRadio->loRaUseHeader = par("initialUseHeader");
        evaluateADRinNode = par("evaluateADRinNode");
        sfVector.setName("SF Vector");
        tpVector.setName("TP Vector");
    }
}

std::pair<double,double> SimpleLoRaApp::generateUniformCircleCoordinates(double radius, double gatewayX, double gatewayY)
{
    double randomValueRadius = uniform(0,(radius*radius));
    double randomTheta = uniform(0,2*M_PI);

    // generate coordinates for circle with origin at 0,0
    double x = sqrt(randomValueRadius) * cos(randomTheta);
    double y = sqrt(randomValueRadius) * sin(randomTheta);
    // Change coordinates based on coordinate system used in OMNeT, with origin at top left
    x = x + gatewayX;
    y = gatewayY - y;
    std::pair<double,double> coordValues = std::make_pair(x,y);
    return coordValues;
}

void SimpleLoRaApp::finish()
{
    cModule *host = getContainingNode(this);
    StationaryMobility *mobility = check_and_cast<StationaryMobility *>(host->getSubmodule("mobility"));
    Coord coord = mobility->getCurrentPosition();
    recordScalar("positionX", coord.x);
    recordScalar("positionY", coord.y);
    recordScalar("finalTP", getTP());
    recordScalar("finalSF", getSF());
    recordScalar("sentPackets", sentPackets);
    recordScalar("receivedADRCommands", receivedADRCommands);
}

void SimpleLoRaApp::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (msg == sendMeasurements)
        {
            // sendJoinRequest();
            // if (simTime() >= getSimulation()->getWarmupPeriod())
                // sentPackets++;
            delete msg;

            // Make this into a poisson (randomness)
            Packet *pkt = new Packet("DataFrame");
            IntrusivePtr<LoRaAppRandomData> payload = makeShared<LoRaAppRandomData>();
            generateRandomAppData(payload, par("dataSize").intValue());
            payload->setChunkLength(B(par("dataSize").intValue()));
            pkt->insertAtFront(payload);
            send(pkt, "socketOut");

            timeToNextPacket = exponential(1/lambdaApp);
            EV << "Next App packet comes in: " << timeToNextPacket << "s" << endl;
            sendMeasurements = new cMessage("sendMeasurements");
            scheduleAt(simTime() + timeToNextPacket, sendMeasurements);
        }
    }
    else
    {
        handleMessageFromLowerLayer(msg);
        delete msg;
    }
}

void SimpleLoRaApp::generateRandomAppData(IntrusivePtr<LoRaAppRandomData> payload, int length)
{
    EV << "Got length: " << length << endl;
    if (length < 0 || length > 254)
    {
        throw cRuntimeError("Unable to generate random AppData. Invalid dataSize");
    }

    for (size_t i = 0; i < length-1; i++)
    {
        char ascii_char = (char)intuniform(65,90); // ascii values [A-Z]
        EV << "RANDOM CHAR: " << ascii_char << endl;
        payload->setText(i, ascii_char);
    }
    payload->setText(length-1, '\0');
    EV << "Generated random data with length: " << length << endl;

}

void SimpleLoRaApp::handleMessageFromLowerLayer(cMessage *msg)
{
//    LoRaAppPacket *packet = check_and_cast<LoRaAppPacket *>(msg);
    auto pkt = check_and_cast<Packet *>(msg);
    const auto & packet = pkt->peekAtFront<LoRaAppPacket>();
    if (simTime() >= getSimulation()->getWarmupPeriod())
        receivedADRCommands++;
    if(evaluateADRinNode)
    {
        ADR_ACK_CNT = 0;
        if(packet->getMsgType() == TXCONFIG)
        {
            if(packet->getOptions().getLoRaTP() != -1)
            {
                setTP(packet->getOptions().getLoRaTP());
            }
            if(packet->getOptions().getLoRaSF() != -1)
            {
                setSF(packet->getOptions().getLoRaSF());
            }
            EV << "New TP " << getTP() << endl;
            EV << "New SF " << getSF() << endl;
        }
    }
}

bool SimpleLoRaApp::handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback)
{
    Enter_Method_Silent();

    throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());
    return true;
}

void SimpleLoRaApp::sendJoinRequest()
{
    auto pktRequest = new Packet("DataFrame");
    pktRequest->setKind(DATA);

    auto payload = makeShared<LoRaAppPacket>();
    payload->setChunkLength(B(par("dataSize").intValue()));

    lastSentMeasurement = rand();
    payload->setSampleMeasurement(lastSentMeasurement);

    if(evaluateADRinNode && sendNextPacketWithADRACKReq)
    {
        auto opt = payload->getOptions();
        opt.setADRACKReq(true);
        payload->setOptions(opt);
        //request->getOptions().setADRACKReq(true);
        sendNextPacketWithADRACKReq = false;
    }


    auto loraTag = pktRequest->addTagIfAbsent<LoRaTag>();
    loraTag->setBandwidth(getBW());
    loraTag->setCenterFrequency(getCF());
    loraTag->setSpreadFactor(getSF());
    loraTag->setCodeRendundance(getCR());
    loraTag->setPower(mW(math::dBmW2mW(getTP())));

    //add LoRa control info
    sfVector.record(getSF());
    tpVector.record(getTP());
    EV << "Wysylam pakiet z TP: " << getTP() << endl;
    EV << "Wysylam pakiet z SF: " << getSF() << endl;
    pktRequest->insertAtBack(payload);
    send(pktRequest, "socketOut");
    if(evaluateADRinNode)
    {
        ADR_ACK_CNT++;
        if(ADR_ACK_CNT == ADR_ACK_LIMIT) sendNextPacketWithADRACKReq = true;
        if(ADR_ACK_CNT >= ADR_ACK_LIMIT + ADR_ACK_DELAY)
        {
            ADR_ACK_CNT = 0;
            increaseSFIfPossible();
        }
    }
    emit(LoRa_AppPacketSent, getSF());
}

void SimpleLoRaApp::increaseSFIfPossible()
{
//    if(loRaSF < 12) loRaSF++;
    if(getSF() < 12) setSF(getSF() + 1);
}

void SimpleLoRaApp::setSF(int SF) {
    loRaRadio->loRaSF = SF;
}

int SimpleLoRaApp::getSF() {
    return loRaRadio->loRaSF;
}

void SimpleLoRaApp::setTP(int TP) {
    loRaRadio->loRaTP = TP;
}

double SimpleLoRaApp::getTP() {
    return loRaRadio->loRaTP;
}

void SimpleLoRaApp::setCF(units::values::Hz CF) {
    loRaRadio->loRaCF = CF;
}

units::values::Hz SimpleLoRaApp::getCF() {
    return loRaRadio->loRaCF;
}

void SimpleLoRaApp::setBW(units::values::Hz BW) {
    loRaRadio->loRaBW = BW;
}

units::values::Hz SimpleLoRaApp::getBW() {
    return loRaRadio->loRaBW;
}

void SimpleLoRaApp::setCR(int CR) {
    loRaRadio->loRaCR = CR;
}

int SimpleLoRaApp::getCR() {
    return loRaRadio->loRaCR;
}

} //end namespace inet
