#ifndef __LORAMAC_H
#define __LORAMAC_H

#include "inet/physicallayer/wireless/common/contract/packetlevel/IRadio.h"
#include "inet/linklayer/contract/IMacProtocol.h"
#include "inet/linklayer/base/MacProtocolBase.h"
#include "inet/common/FSMA.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "LoRaMacFrame_m.h"
#include "inet/common/Protocol.h"
#include "inet/queueing/contract/IActivePacketSink.h"
#include "inet/queueing/contract/IPacketQueue.h"
#include "inet/linklayer/contract/IMacProtocol.h"

#include "LoRaRadio.h"

namespace flora_tdma {

using namespace physicallayer;

/**
 * Based on CSMA class. 
 * There is no CMSA class in INET4.4 or OMNet++ 6.1?!
 */

class LoRaMac : public MacProtocolBase, public IMacProtocol, public queueing::IActivePacketSink
{
  protected:
    /**
     * @name Configuration parameters
     */
    //@{
    MacAddress address;
    double bitrate = NaN;
    int headerLength = -1;
    simtime_t waitDelay1Time = -1; // used for LoRaWAN
    simtime_t listening1Time = -1; // used for LoRaWAN
    simtime_t waitDelay2Time = -1; // used for LoRaWAN
    simtime_t listening2Time = -1; // used for LoRaWAN
    int sequenceNumber = 0;
    //@}

    /** End of the Short Inter-Frame Time period */
    cMessage *endSifs = nullptr;

    /**
     * @name CsmaCaMac state variables
     * Various state information checked and modified according to the state machine.
     */
    //@{
    enum State {
        IDLE,
        TRANSMIT,
        WAIT_DELAY_1, /* From this: This is used to alike LoRaWAN wait for receive window 1 */
        LISTENING_1,
        RECEIVING_1,
        WAIT_DELAY_2, 
        LISTENING_2,
        RECEIVING_2, /* To this line is form LoRaWAN like */
    };

    IRadio *radio = nullptr;
    IRadio::TransmissionState transmissionState = IRadio::TRANSMISSION_STATE_UNDEFINED;
    IRadio::ReceptionState receptionState = IRadio::RECEPTION_STATE_UNDEFINED;

    cFSM fsm;

    /** @name Timer messages */
    //@{
    /** Timeout after the transmission of a Data frame */
    cMessage *endTransmission = nullptr;

    /** Timeout after the reception of a Data frame */
    cMessage *endReception = nullptr;

    /** Timeout after the reception of a Data frame */
    cMessage *droppedPacket = nullptr;

    /** End of the Delay_1 */
    cMessage *endDelay_1 = nullptr;

    /** End of the Listening_1 */
    cMessage *endListening_1 = nullptr;

    /** End of the Delay_2 */
    cMessage *endDelay_2 = nullptr;

    /** End of the Listening_2 */
    cMessage *endListening_2 = nullptr;

    /** Radio state change self message. Currently this is optimized away and sent directly */
    cMessage *mediumStateChange = nullptr;
    //@}

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
    virtual ~LoRaMac();
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
    virtual void handleWithFsm(cMessage *msg);

    virtual void receiveSignal(cComponent *source, simsignal_t signalID, intval_t value, cObject *details) override;

    virtual Packet *encapsulate(Packet *msg);
    virtual Packet *decapsulate(Packet *frame);
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
    virtual void finishCurrentTransmission();
    virtual Packet *getCurrentTransmission();

    virtual bool isReceiving();
    virtual bool isForUs(const Ptr<const LoRaMacFrame> &msg);

    void turnOnReceiver(void);
    void turnOffReceiver(void);
    virtual void processUpperPacket();
    //@}
};

} // namespace inet

#endif // ifndef __LORAMAC_H
