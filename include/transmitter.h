#ifndef __TRANSMITTER__
#define __TRANSMITTER__

#include "utils.h"

enum TransmitterMode { SEND, RECEIVE };

class Transmitter
{
  public:
    Transmitter(std::string &dest_ip, size_t out_msg_count, size_t in_msg_count,
                Queue<MainEvent> &main_queue, Queue<OutEvent> &out_queue,
                uint32_t min_ack_id, uint32_t min_msg_id);
    Transmitter(size_t in_msg_count, Queue<MainEvent> &main_queue,
                Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                uint32_t min_msg_id);

    ~Transmitter();

    /* Base sending/receiving: */

    void send_msg(std::vector<std::byte> &data);
    void receive_msg(MainEvent ev);
    void resend_msg(SentMessage &msg);
    void set_ack(MainEvent ev);
    void check_resends();

    /* Main loop: */
    void run_main_body(std::function<void(std::vector<MainEvent>)> iter_func);

    /* Queue REFERENCES and message hash tables: */
    Queue<MainEvent> &main_queue;
    Queue<OutEvent> &out_queue;
    std::unordered_map<uint32_t, SentMessage> sent_msgs;
    std::unordered_map<uint32_t, RecvdMessage> recvd_msgs;

    /* Src/destination info: */
    std::string dest_ip;
    std::string src_ip;

    /* Target message numbers: */
    size_t in_msg_count;
    size_t out_msg_count;

    /* Minimum ack/in message ids to work with: */
    uint32_t min_ack_id;
    uint32_t min_msg_id;

    TransmitterMode mode;

  protected:
    bool done{false};

  private:
    void check_completion();
};

#endif /* __TRANSMITTER */