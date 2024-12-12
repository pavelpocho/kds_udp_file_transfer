#ifndef __FILE_TRANSMITTER__
#define __FILE_TRANSMITTER__

#include "sha256.h"
#include "transmitter.h"

typedef struct {
    std::vector<std::byte> data;
    uint32_t msg_id;
} PacketShelfItem;

class FileTransmitter : public Transmitter
{
  public:
    FileTransmitter(std::string &dest_ip, size_t out_msg_count,
                    size_t in_msg_count, Queue<MainEvent> &main_queue,
                    Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                    uint32_t min_msg_id, uint32_t f_pckt_n);

    FileTransmitter(size_t in_msg_count, Queue<MainEvent> &main_queue,
                    Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                    uint32_t min_msg_id, uint32_t f_pckt_n);

    ~FileTransmitter();

    void start_stream_file(const std::string &filename, std::size_t chunk_size);
    void continue_stream_file(std::size_t chunk_size, std::string &sha);

    void prep_receive_file(const std::string &f_name);
    void receive_stream_file(std::vector<MainEvent> evs);
    std::string receive_checksum_msg();

    bool did_receive_checksum_confirmation();
    bool receive_checksum_confirmation_msg();

    void close_write_file();

  private:
    std::ifstream file;
    std::ofstream file_o;
    bool sent_checksum{false};
    uint32_t next_packet_id_to_write;
    uint32_t f_pckt_n;
    std::vector<PacketShelfItem> packet_shelf;
    SHA256 sha;
    std::unordered_map<uint32_t, uint32_t> recvd_fs_msgs;
};

#endif /* __CHECKSUM_TRANSMITTER__ */