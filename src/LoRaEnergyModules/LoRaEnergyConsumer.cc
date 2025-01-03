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

#include "LoRaEnergyConsumer.h"

#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "LoRaPhy/LoRaTransmitter.h"
namespace flora_tdma {

using namespace inet::power;

Define_Module(LoRaEnergyConsumer);

void LoRaEnergyConsumer::initialize(int stage)
{
    cSimpleModule::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        if (!readConfigurationFile())
            throw cRuntimeError("LoRaEnergyConsumer: error in reading the input configuration file");
        standbySupplyCurrent = 0;
        sleepSupplyCurrent = 0;
        sleepPowerConsumption = mW(supplyVoltage*sleepSupplyCurrent);

        receiverIdlePowerConsumption = mW(supplyVoltage*idleSupplyCurrent);
        transmitterIdlePowerConsumption = mW(supplyVoltage*idleSupplyCurrent);
        receiverReceivingPowerConsumption = mW(supplyVoltage*receiverReceivingSupplyCurrent);
        receiverBusyPowerConsumption = mW(supplyVoltage*receiverBusySupplyCurrent);

        offPowerConsumption = W(par("offPowerConsumption"));
        switchingPowerConsumption = W(par("switchingPowerConsumption"));

        //Setting to zero, values are passed to energystorage based on tx power of current packet in getPowerConsumption()
        transmitterTransmittingPowerConsumption = W(0);
        transmitterTransmittingPreamblePowerConsumption = W(0);
        transmitterTransmittingHeaderPowerConsumption = W(0);
        transmitterTransmittingDataPowerConsumption = W(0);

        cModule *radioModule = getParentModule();
        radioModule->subscribe(IRadio::radioModeChangedSignal, this);
        radioModule->subscribe(IRadio::receptionStateChangedSignal, this);
        radioModule->subscribe(IRadio::transmissionStateChangedSignal, this);
        radioModule->subscribe(IRadio::receivedSignalPartChangedSignal, this);
        radioModule->subscribe(IRadio::transmittedSignalPartChangedSignal, this);
        radio = check_and_cast<IRadio *>(radioModule);

        energySource.reference(this, "energySourceModule", true);

        totalEnergyConsumed = 0;
        energyBalance = J(0);
    }
    else if (stage == INITSTAGE_POWER)
        energySource->addEnergyConsumer(this);
}

void LoRaEnergyConsumer::finish()
{
    recordScalar("totalEnergyConsumed", double(totalEnergyConsumed));
}

bool LoRaEnergyConsumer::readConfigurationFile()
{
    cXMLElement *xmlConfig = par("configFile").xmlValue();
    if (xmlConfig == nullptr)
        return false;
    cXMLElementList tagList;
    cXMLElement *tempTag;
    const char *str;
    std::string sstr;    // for easier comparison

    tagList = xmlConfig->getElementsByTagName("receiverReceivingSupplyCurrent");
    if(tagList.empty()) {
        throw cRuntimeError("receiverReceivingSupplyCurrent not defined in the configuration file!");
    }
    tempTag = tagList.front();
    str = tempTag->getAttribute("value");
    receiverReceivingSupplyCurrent = strtod(str, nullptr);

    tagList = xmlConfig->getElementsByTagName("receiverBusySupplyCurrent");
    if(tagList.empty()) {
        throw cRuntimeError("receiverBusySupplyCurrent not defined in the configuration file!");
    }
    tempTag = tagList.front();
    str = tempTag->getAttribute("value");
    receiverBusySupplyCurrent = strtod(str, nullptr);

    tagList = xmlConfig->getElementsByTagName("idleSupplyCurrent");
    if(tagList.empty()) {
        throw cRuntimeError("idleSupplyCurrent not defined in the configuration file!");
    }
    tempTag = tagList.front();
    str = tempTag->getAttribute("value");
    idleSupplyCurrent = strtod(str, nullptr);

    tagList = xmlConfig->getElementsByTagName("supplyVoltage");
    if(tagList.empty()) {
        throw cRuntimeError("supplyVoltage not defined in the configuration file!");
    }
    tempTag = tagList.front();
    str = tempTag->getAttribute("value");
    supplyVoltage = strtod(str, nullptr);

    tagList = xmlConfig->getElementsByTagName("txSupplyCurrents");
    if(tagList.empty()) {
        throw cRuntimeError("txSupplyCurrents not defined in the configuration file!");
    }
    tempTag = tagList.front();
    cXMLElementList txSupplyCurrentList = tempTag->getElementsByTagName("txSupplyCurrent");
    if (txSupplyCurrentList.empty())
        throw cRuntimeError("No txSupplyCurrent have been defined in txSupplyCurrents!");

    for (cXMLElementList::const_iterator aComb = txSupplyCurrentList.begin(); aComb != txSupplyCurrentList.end(); aComb++) {
        const char *txPower, *supplyCurrent;
        if ((*aComb)->getAttribute("txPower") != nullptr)
            txPower = (*aComb)->getAttribute("txPower");
        else
            txPower = "";
        if ((*aComb)->getAttribute("supplyCurrent") != nullptr)
            supplyCurrent = (*aComb)->getAttribute("supplyCurrent");
        else
            supplyCurrent = "";
        transmitterTransmittingSupplyCurrent[strtod(txPower, nullptr)] = strtod(supplyCurrent, nullptr);
    }
    return true;
}

void LoRaEnergyConsumer::receiveSignal(cComponent *source, simsignal_t signal, intval_t value, cObject *details)
{
    if (signal == IRadio::radioModeChangedSignal ||
        signal == IRadio::receptionStateChangedSignal ||
        signal == IRadio::transmissionStateChangedSignal ||
        signal == IRadio::receivedSignalPartChangedSignal ||
        signal == IRadio::transmittedSignalPartChangedSignal)
    {
        powerConsumption = getPowerConsumption();
        emit(powerConsumptionChangedSignal, powerConsumption.get());

        simtime_t currentSimulationTime = simTime();
        energyBalance += s((currentSimulationTime - lastEnergyBalanceUpdate).dbl()) * (lastPowerConsumption);
        totalEnergyConsumed = (energyBalance.get());
        lastEnergyBalanceUpdate = currentSimulationTime;
        lastPowerConsumption = powerConsumption;
    }
    else
        throw cRuntimeError("Unknown signal");
}

W LoRaEnergyConsumer::getPowerConsumption() const
{
    IRadio::RadioMode radioMode = radio->getRadioMode();

    if (radioMode == IRadio::RADIO_MODE_OFF)
        return offPowerConsumption;
    if (radioMode == IRadio::RADIO_MODE_SLEEP || radioMode == IRadio::RADIO_MODE_SWITCHING)
        return W(0);

    W powerConsumption = W(0);

    if (radioMode == IRadio::RADIO_MODE_RECEIVER) {
        powerConsumption += mW(supplyVoltage*receiverBusySupplyCurrent);
    } else if (radioMode == IRadio::RADIO_MODE_TRANSMITTER) {
        LoRaRadio *radio = check_and_cast<LoRaRadio *>(getParentModule());
        auto current = transmitterTransmittingSupplyCurrent.find(14);
        powerConsumption += mW(supplyVoltage*current->second);
    } else {
        powerConsumption += mW(supplyVoltage*idleSupplyCurrent);
    }

    return powerConsumption;
}
}
