#include "header_transmitter.h"

HeaderTransmitter::HeaderTransmitter(std::string &dest_ip, size_t out_msg_count,
                                     size_t in_msg_count,
                                     Queue<MainEvent> &main_queue,
                                     Queue<OutEvent> &out_queue,
                                     uint32_t min_ack_id, uint32_t min_msg_id)
    : Transmitter{dest_ip,   out_msg_count, in_msg_count, main_queue,
                  out_queue, min_ack_id,    min_msg_id}
{
}

HeaderTransmitter::HeaderTransmitter(size_t in_msg_count,
                                     Queue<MainEvent> &main_queue,
                                     Queue<OutEvent> &out_queue,
                                     uint32_t min_ack_id, uint32_t min_msg_id)
    : Transmitter{in_msg_count, main_queue, out_queue, min_ack_id, min_msg_id}
{
}

void HeaderTransmitter::send_header_msg(const std::string &f_name,
                                        const size_t &f_size)
{
    /* File name length limited to 256 characters. */
    std::size_t max_ch = std::min(f_name.length(), (size_t)256);
    std::string str = "%*%HEADER%*%" + f_name.substr(0, max_ch) + "%*%";

    std::vector<std::byte> data;
    data.reserve(str.size() + sizeof(f_size));

    data.insert(data.end(), reinterpret_cast<const std::byte *>(str.data()),
                reinterpret_cast<const std::byte *>(str.data()) + str.size());
    data.insert(data.end(), reinterpret_cast<const std::byte *>(&f_size),
                reinterpret_cast<const std::byte *>(&f_size) + sizeof(f_size));

    send_msg(data);
}

void HeaderTransmitter::receive_header_msg(std::string &f_name, size_t &f_size)
{
    const auto &content = recvd_msgs[0].content;
    if (content.size() < 16 + sizeof(size_t)) {
        throw std::runtime_error("Invalid header: insufficient data.");
    }

    // Extract and validate the header text
    std::string h_text(reinterpret_cast<const char *>(content.data()) + 3, 6);
    if (h_text != "HEADER") {
        throw std::runtime_error("Expected header, but got something else.");
    }

    size_t nm_start = 12;
    // -3 for the last %*%
    size_t nm_end = content.size() - sizeof(size_t) - 3;

    const char *ptr = reinterpret_cast<const char *>(content.data() + nm_start);
    f_name = std::string(ptr, nm_end - nm_start);
    std::memcpy(&f_size, content.data() + nm_end + 3, sizeof(size_t));
}