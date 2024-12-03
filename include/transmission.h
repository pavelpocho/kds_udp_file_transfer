#ifndef __TRANSMISSION__
#define __TRANSMISSION__

#include "utils.h"

enum TransmissionMode { SEND, RECEIVE };

class Transmission
{
  public:
    Transmission(std::string &dest_ip, size_t out_msg_count,
                 size_t in_msg_count, Queue<MainEvent> &main_queue,
                 Queue<OutEvent> &out_queue, uint16_t min_ack_id,
                 uint16_t min_msg_id);
    Transmission(size_t in_msg_count, Queue<MainEvent> &main_queue,
                 Queue<OutEvent> &out_queue, uint16_t min_ack_id,
                 uint16_t min_msg_id);

    ~Transmission();

    /* Sending specific data: */
    void send_header_msg(const std::string f_name, const size_t size);

    /* Closing connection: */
    void send_checksum_msg(bool match);

    void send_msg(const std::byte *data, size_t length);
    void receive_msg(MainEvent ev);
    void set_ack(MainEvent ev);
    void check_resends(MainEvent ev);

    /* Checking if done: */

    bool is_complete();

    /* Sending things: */
    void send_header_transmission();
    void start_stream_file(const std::string &filename, std::size_t chunk_size);
    void continue_stream_file(std::size_t chunk_size);

    /* Receiving things: */
    void prep_receive_file(const std::string &f_name);
    void receive_stream_file(std::vector<MainEvent> evs);

    void run_main_body(std::function<void(std::vector<MainEvent>)> iter_func);

    Queue<MainEvent> &main_queue;
    Queue<OutEvent> &out_queue;

    std::unordered_map<uint16_t, SentMessage> sent_msgs;
    std::unordered_map<uint16_t, RecvdMessage> recvd_msgs;

    std::string dest_ip;
    std::string src_ip;
    size_t in_msg_count;
    size_t out_msg_count;

    uint16_t min_ack_id;
    uint16_t min_msg_id;

    TransmissionMode mode;

    std::ifstream file;
    std::ofstream file_o;

  private:
    bool done{false};
    bool sent_checksum{false};

    void check_completion();
};

#endif /* __TRANSMISSION__ */