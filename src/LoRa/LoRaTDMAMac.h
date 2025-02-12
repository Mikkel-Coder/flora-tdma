#ifndef __LORATDMAMAC_H
#define __LORATDMAMAC_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/linklayer/contract/IMacProtocol.h"
#include "inet/linklayer/base/MacProtocolBase.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "LoRaTDMAMacFrame_m.h"
#include "LoRaTDMAGWFrame_m.h"
#include "inet/common/Protocol.h"
#include "inet/queueing/contract/IActivePacketSink.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "inet/linklayer/contract/IMacProtocol.h"
#include "inet/clock/model/SettableClock.h"
#include <queue>

#include "LoRaRadio.h"

namespace flora_tdma {

using namespace physicallayer;

/**
 * Based on CSMA class. 
 * There is no CMSA class in INET4.4 or OMNet++ 6.1?!
 */

// NOTE: the error then I used clockusermixin was not from that fact,
// may refactor to clockusermixin if we have time in the future
class LoRaTDMAMac : public MacProtocolBase, public IMacProtocol, public queueing::IActivePacketSink
{
  protected:
    /**
     * @name Configuration parameters
     */
    //@{
    MacAddress address;
    clocktime_t txslotDuration;
    clocktime_t rxslotDuration;
    clocktime_t broadcastGuard;
    clocktime_t startTransmitOffset;
    clocktime_t firstRxSlot;
    double bitrate = NaN;
    int headerLength = -1;
    // int sequenceNumber = 0;
    //@}

    /** End of the Short Inter-Frame Time period */
    cMessage *endSifs = nullptr;

    std::queue<int> nextTimeSlots;
    clocktime_t lastRXendTime;

    /** @name MAC States */
    enum States {
      INIT,
      SLEEP,
      TRANSMIT,
      LISTEN,
      RECEIVE,
    };

    IRadio *radio = nullptr;
    IRadio::TransmissionState transmissionState = IRadio::TRANSMISSION_STATE_UNDEFINED;
    IRadio::ReceptionState receptionState = IRadio::RECEPTION_STATE_UNDEFINED;

    SettableClock *clock = nullptr;

    /** @name the mac state */
    States macState;

    /** @name Timer messages */
    ClockEvent *startRXSlot = nullptr;
    ClockEvent *endRXSlot = nullptr;
    ClockEvent *startTXSlot = nullptr;
    ClockEvent *endTXSlot = nullptr;
    ClockEvent *startTransmit = nullptr;

    /** @name State transition messages */
    cMessage *endTransmission = nullptr;
    cMessage *endReception = nullptr;

    cMessage *mediumStateChange = nullptr;
    cMessage *endRXEarly = nullptr;

    /** @name Statistics */
    //@{
    long numSent;
    long numReceived;
    //@}

  public:
    /**
     * @name Construction functions
     */
    //@{
    virtual ~LoRaTDMAMac();
    //@}
    virtual MacAddress getAddress();
    virtual queueing::IPassivePacketSource *getProvider(cGate *gate) override;
    virtual void handleCanPullPacketChanged(cGate *gate) override;
    virtual void handlePullPacketProcessed(Packet *packet, cGate *gate, bool successful) override;

  protected:
    /**
     * @name Initialization functions
     */
    //@{
    /** @brief Initialization of the module and its variables */
    virtual void initialize(int stage) override;
    virtual void finish() override;
    virtual void configureNetworkInterface() override;
    //@}

    /**
     * @name Message handing functions
     * @brief Functions called from other classes to notify about state changes and to handle messages.
     */
    //@{
    virtual void handleSelfMessage(cMessage *msg) override;
    virtual void handleUpperPacket(Packet *packet) override;
    virtual void handleLowerPacket(Packet *packet) override;
    // virtual void handleWithFsm(cMessage *msg);
    virtual void handleState(cMessage *msg);
    virtual void handleNextTXSlot();

    virtual void receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details) override;

    virtual Packet *encapsulate(Packet *msg);
    virtual Packet *decapsulate(Packet *frame); // Remake this as a future feature
    //@}

    /**
     * @name Frame transmission functions
     */
    //@{
    virtual void sendDataFrame(Packet *frameToSend);
    //@}

    /**
     * @name Utility functions
     */
    //@{
    // virtual void finishCurrentTransmission();
    virtual Packet *getCurrentTransmission();

    // virtual bool isReceiving();
    // virtual bool isForUs(const Ptr<const LoRaTDMAMacFrame> &msg);

    virtual void processUpperPacket();
    //@}
};

} // namespace inet

#endif // ifndef __LORATDMAMAC_H
