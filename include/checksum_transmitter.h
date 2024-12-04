#ifndef __CHECKSUM_TRANSMITTER__
#define __CHECKSUM_TRANSMITTER__

#include "transmitter.h"

/**
 * @brief This is called ChecksumTransmitter, but it really only transmits
 * checksum confirmations from the receiver to the sender!
 *
 */
class ChecksumTransmitter : public Transmitter
{
  public:
    ChecksumTransmitter(std::string &dest_ip, size_t out_msg_count,
                        size_t in_msg_count, Queue<MainEvent> &main_queue,
                        Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                        uint32_t min_msg_id);

    ChecksumTransmitter(size_t in_msg_count, Queue<MainEvent> &main_queue,
                        Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                        uint32_t min_msg_id);

    void send_checksum_confirmation_msg(bool match);
    void receive_checksum_msg();
};

#endif /* __CHECKSUM_TRANSMITTER__ */