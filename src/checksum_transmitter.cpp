#include "checksum_transmitter.h"

ChecksumTransmitter::ChecksumTransmitter(
    std::string &dest_ip, size_t out_msg_count, size_t in_msg_count,
    Queue<MainEvent> &main_queue, Queue<OutEvent> &out_queue,
    uint32_t min_ack_id, uint32_t min_msg_id)
    : Transmitter{dest_ip,   out_msg_count, in_msg_count, main_queue,
                  out_queue, min_ack_id,    min_msg_id}
{
}

ChecksumTransmitter::ChecksumTransmitter(size_t in_msg_count,
                                         Queue<MainEvent> &main_queue,
                                         Queue<OutEvent> &out_queue,
                                         uint32_t min_ack_id,
                                         uint32_t min_msg_id)
    : Transmitter{in_msg_count, main_queue, out_queue, min_ack_id, min_msg_id}
{
}

void ChecksumTransmitter::send_checksum_confirmation_msg(bool match)
{
    std::string val = (match ? "1" : "0");
    std::string str = "%*%CHKSUM%*%" + val + "%*%";
    std::vector<std::byte> data;
    data.reserve(str.size());
    for (char c : str) {
        data.push_back(std::byte{static_cast<unsigned char>(c)});
    }
    send_msg(data);
}