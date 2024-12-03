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

#ifndef LORA_LORATDMAGWMAC_H_
#define LORA_LORATDMAGWMAC_H_

#include "inet/common/INETDefs.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/linklayer/contract/IMacProtocol.h"
#include "inet/linklayer/base/MacProtocolBase.h"
#include "inet/linklayer/common/InterfaceTag_m.h"
#include "inet/linklayer/common/MacAddressTag_m.h"
#include "inet/common/ModuleAccess.h"
#include <vector>

#include "LoRaTDMAMac.h"
#include "LoRaTDMAMacFrame_m.h"
#include "LoRaTDMAGWFrame_m.h"

#if INET_VERSION < 0x0403 || ( INET_VERSION == 0x0403 && INET_PATCH_LEVEL == 0x00 )
#  error At least INET 4.3.1 is required. Please update your INET dependency and fully rebuild the project.
#endif
namespace flora_tdma {

using namespace inet;
using namespace inet::physicallayer;

class LoRaTDMAGWMac: public MacProtocolBase {
public:
    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void configureNetworkInterface() override;
    long GW_forwardedDown;
    long GW_droppedDC;
    int numOfTimeslots;
    simtime_t txslotDuration;
    simtime_t rxslotDuration;
    simtime_t broadcastGuard;
    simtime_t firstTxSlot;

    cMessage *startBroadcast;
    cMessage *endBroadcast;

    MacAddress clients[1000];
    std::vector<MacAddress> *timeslots;

    /** @name MAC States */
    enum States {
      INIT,
      TRANSMIT,
      RECEIVE,
    };

    /** @name the mac state */
    States macState;

    virtual void handleLowerMessage(cMessage *msg) override;
    virtual void handleSelfMessage(cMessage *message) override;
    
    virtual MacAddress getAddress();

protected:
    MacAddress address;

    IRadio *radio = nullptr;
    IRadio::TransmissionState transmissionState = IRadio::TRANSMISSION_STATE_UNDEFINED;

    virtual void createTimeslots();
    virtual void handleState(cMessage *msg);

    virtual void receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details) override;
};

}

#endif /* LORA_LORATDMAGWMAC_H_ */
