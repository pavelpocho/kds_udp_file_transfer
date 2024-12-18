#include "file_transmitter.h"
#include <algorithm>

#define CHKSUM_MSG_INV "Invalid checksum confirmation: insufficient data."
#define CHKSUM_DATA_INV "Expected checksum confirmation, got something else."

FileTransmitter::FileTransmitter(std::string &dest_ip, size_t out_msg_count,
                                 size_t in_msg_count,
                                 Queue<MainEvent> &main_queue,
                                 Queue<OutEvent> &out_queue,
                                 uint32_t min_ack_id, uint32_t min_msg_id,
                                 uint32_t f_pckt_n)
    : Transmitter{dest_ip,   out_msg_count, in_msg_count, main_queue,
                  out_queue, min_ack_id,    min_msg_id}
{
    next_packet_id_to_write = min_msg_id;
    this->f_pckt_n = f_pckt_n;
}

FileTransmitter::FileTransmitter(size_t in_msg_count,
                                 Queue<MainEvent> &main_queue,
                                 Queue<OutEvent> &out_queue,
                                 uint32_t min_ack_id, uint32_t min_msg_id,
                                 uint32_t f_pckt_n)
    : Transmitter{in_msg_count, main_queue, out_queue, min_ack_id, min_msg_id}
{
    next_packet_id_to_write = min_msg_id;
    this->f_pckt_n = f_pckt_n;
}

FileTransmitter::~FileTransmitter()
{
    if (file_o.is_open()) {
        file_o.close();
    }
}

void FileTransmitter::start_stream_file(const std::string &filename,
                                        std::size_t chunk_size)
{
    file = std::ifstream{filename, std::ios::binary};
    if (!file.is_open())
        throw std::runtime_error("Couldn't open file :( " + filename);

    std::vector<std::byte> buffer{chunk_size};
    file.read(reinterpret_cast<char *>(buffer.data()), chunk_size);
    std::size_t bytes_read = file.gcount();

    if (bytes_read == chunk_size)
        send_msg(buffer);
    else if (bytes_read > 0 && bytes_read < chunk_size) {
        std::vector<std::byte> data{bytes_read};
        memcpy(&data[0], &buffer[0], bytes_read);
        send_msg(data);
    }
}

void FileTransmitter::continue_stream_file(std::size_t chunk_size,
                                           std::string &sha)
{
    /* If we already got checksum confirmation, it means all content
     * messages arrived even if we didn't get acks back. */
    if (did_receive_checksum_confirmation()) {
        this->done = true;
    }

    int ackd = 0;
    for (auto &msg : sent_msgs) {
        ackd += static_cast<int>(msg.second.ackd);
    }
    int in_the_air = static_cast<int>(sent_msgs.size()) - ackd;

    if (file.tellg() == -1) {
        if (this->sent_checksum) {
            return;
        }
        std::vector<std::byte> data;
        data.reserve(sha.size());
        for (char c : sha) {
            data.push_back(std::byte{static_cast<unsigned char>(c)});
        }
        send_msg(data);
        this->sent_checksum = true;
        return;
    }
    std::vector<std::byte> buffer(chunk_size);
    for (int i = 0; i < WINDOW_SIZE - in_the_air; ++i) {
        file.read(reinterpret_cast<char *>(buffer.data()), chunk_size);
        std::size_t bytes_read = file.gcount();

        if (bytes_read == chunk_size)
            send_msg(buffer);
        else if (bytes_read > 0 && bytes_read < chunk_size) {
            std::vector<std::byte> data{bytes_read};
            memcpy(&data[0], &buffer[0], bytes_read);
            send_msg(data);
        }
    }

    if (file.peek() == EOF)
        file.close();
}

void FileTransmitter::prep_receive_file(const std::string &f_name)
{
    file_o.open(f_name, std::ios::binary | std::ios::out);
    if (!file_o.is_open()) {
        throw std::runtime_error("Couldn't open file for writing :( ");
    }
}

void FileTransmitter::receive_stream_file(std::vector<MainEvent> evs)
{
    if (!file_o.is_open()) {
        throw std::runtime_error("File for writing not open...");
    }

    for (MainEvent ev : evs) {
        /* If not a message, below minimum ID or above max. packet ID (equal to
         * number of packets because the file packets start at 1), don't add to
         * file. */
        if (ev.type != MainEventType::M_MSG || ev.msg_id < this->min_msg_id ||
            ev.msg_id >= this->f_pckt_n ||
            recvd_fs_msgs.find(ev.msg_id) != recvd_fs_msgs.end())
            continue;

        recvd_fs_msgs[ev.msg_id] = ev.msg_id;

        /* Remove data from message so we don't store it pointlessly... */
        recvd_msgs[ev.msg_id].content = std::vector<std::byte>{0};

        /* If correct packet received, add it to file. */
        /* If wrong packet received, stash it and sort the stash. */
        /* Once correct packet received, add it and as many stashed packets as
         * possible. */

        if (recvd_fs_msgs.size() % 10 == 0)
            std::cout << "Progress: "
                      << recvd_fs_msgs.size() / (float)f_pckt_n * 100.0f
                      << "%\r" << std::flush;

        if (ev.msg_id == next_packet_id_to_write) {
            auto &c = ev.content;
            file_o.write((char *)&c[0], c.size());
            ev.content = std::vector<std::byte>{};
            ++next_packet_id_to_write;

            if (packet_shelf.size() > 0)
                while (packet_shelf[0].msg_id == next_packet_id_to_write) {
                    auto &c = packet_shelf[0].data;
                    file_o.write((char *)&c[0], c.size());
                    packet_shelf.erase(packet_shelf.begin());
                    ++next_packet_id_to_write;
                }
        } else {
            PacketShelfItem item{.data = ev.content, .msg_id = ev.msg_id};
            packet_shelf.push_back(item);
            ev.content = std::vector<std::byte>{};
            std::sort(packet_shelf.begin(), packet_shelf.end(),
                      [](const PacketShelfItem &a, const PacketShelfItem &b) {
                          return a.msg_id < b.msg_id;
                      });
        }
    }
}

bool FileTransmitter::receive_checksum_confirmation_msg()
{
    const auto &content = recvd_msgs[0].content;
    if (content.size() < 16)
        throw std::runtime_error(CHKSUM_MSG_INV);

    /* Extract and validate the checksum confirmation text */
    std::string h_text(reinterpret_cast<const char *>(content.data()) + 3, 6);
    if (h_text != "CHKSUM")
        throw std::runtime_error(CHKSUM_DATA_INV);

    return (char)(content[12]) == '1';
}

bool FileTransmitter::did_receive_checksum_confirmation()
{
    return recvd_msgs.size() == 1;
}

std::string FileTransmitter::receive_checksum_msg()
{
    /* recvd_msgs is indexed by message ID, where message with
    ID=0 is not in the map because that's the header and that is received
    by a different transmitter. */
    const auto &content = recvd_msgs[recvd_msgs.size()].content;
    std::string md5;
    md5.reserve(content.size());
    for (const auto &byte : content)
        md5.push_back(static_cast<char>(byte));

    return md5;
}

void FileTransmitter::close_write_file() { this->file_o.close(); }