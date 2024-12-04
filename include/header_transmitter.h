#ifndef __HEADER_TRANSMITTER__
#define __HEADER_TRANSMITTER__

#include "transmitter.h"

class HeaderTransmitter : public Transmitter
{
  public:
    HeaderTransmitter(std::string &dest_ip, size_t out_msg_count,
                      size_t in_msg_count, Queue<MainEvent> &main_queue,
                      Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                      uint32_t min_msg_id);

    HeaderTransmitter(size_t in_msg_count, Queue<MainEvent> &main_queue,
                      Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                      uint32_t min_msg_id);

    void send_header_msg(const std::string &f_name, const size_t &f_size);
    void receive_header_msg(std::string &f_name, size_t &f_size);
};

#endif /* __HEADER_TRANSMITTER__ */