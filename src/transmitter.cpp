#include "transmitter.h"

Transmitter::Transmitter(std::string &dest_ip, size_t out_msg_count,
                         size_t in_msg_count, Queue<MainEvent> &main_queue,
                         Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                         uint32_t min_msg_id)
    : main_queue{main_queue}, out_queue{out_queue}, out_msg_count{out_msg_count}
{
    this->dest_ip = dest_ip;
    this->in_msg_count = in_msg_count;
    this->min_msg_id = min_msg_id;
    this->min_ack_id = min_ack_id;
    this->mode = TransmitterMode::SEND;
}

Transmitter::Transmitter(size_t in_msg_count, Queue<MainEvent> &main_queue,
                         Queue<OutEvent> &out_queue, uint32_t min_ack_id,
                         uint32_t min_msg_id)
    : main_queue{main_queue}, out_queue{out_queue}, out_msg_count{0}
{
    this->dest_ip = "";
    this->in_msg_count = in_msg_count;
    this->min_msg_id = min_msg_id;
    this->min_ack_id = min_ack_id;
    this->mode = TransmitterMode::RECEIVE;
}

Transmitter::~Transmitter() {}

void Transmitter::send_msg(std::vector<std::byte> &data)
{
    const uint32_t id = next_id++;
    SentMessage sent_message{.ackd = false,
                             .content = data,
                             .id = id,
                             .retries = 1,
                             .sent_at =
                                 std::chrono::high_resolution_clock::now()};

    sent_msgs[id] = sent_message;
    OutEvent e{sent_message.content, id, dest_ip, OutEventType::O_MSG};
    out_queue.push(e);
};

void Transmitter::receive_msg(MainEvent ev)
{
    this->src_ip = ev.origin_ip;
    recvd_msgs[ev.msg_id] = RecvdMessage{
        .content = ev.content,
        .received_at = std::chrono::high_resolution_clock::now(),
    };
    check_completion();
}

void Transmitter::set_ack(MainEvent ev)
{
    /* If this is an ACK for something that was not sent,
     * then it's a corrupted ACK and it will be missing somewhere... */
    if (sent_msgs.find(ev.msg_id) != sent_msgs.end()) {
        sent_msgs[ev.msg_id].ackd = (int)ev.content[0] > 128;
        if (sent_msgs[ev.msg_id].ackd) {
            sent_msgs[ev.msg_id].content = std::vector<std::byte>{0};
        }
        check_completion();
    }
}

void Transmitter::check_completion()
{
    /* Check for completion. */
    bool all_ackd = true;
    for (const auto &msg : sent_msgs) {
        if (!msg.second.ackd) {
            all_ackd = false;
            break;
        }
    }

    // std::cout << "Rec: " << recvd_msgs.size() << "/" << this->in_msg_count
    //           << std::endl;
    // std::cout << "Sen: " << sent_msgs.size() << "/" << this->out_msg_count
    //           << std::endl;
    // std::cout << "Ack: " << all_ackd << std::endl;

    this->done = recvd_msgs.size() == this->in_msg_count &&
                 sent_msgs.size() == this->out_msg_count && all_ackd;
}

void Transmitter::check_resends()
{
    using namespace std::chrono;
    auto now = high_resolution_clock::now();

    for (auto &msg : sent_msgs) {
        auto &m = msg.second;
        auto duration = duration_cast<microseconds>(now - m.sent_at);
        if (duration.count() > RESEND_DELAY && !m.ackd) {
            ++m.retries;
            if (m.retries > MAX_RETRIES) {
                throw std::runtime_error("Out of attempts for packet.");
            }
            OutEvent e{m.content, m.id, dest_ip, OutEventType::O_MSG};
            out_queue.push(e);
        }
    }
}

void Transmitter::run_main_body(
    std::function<void(std::vector<MainEvent>)> iter_func)
{
    while (!this->done && !stop) {
        std::vector<MainEvent> evs = main_queue.wait_nonempty();
        if (this->done || stop) {
            break;
        }
        for (MainEvent ev : evs) {
            switch (ev.type) {
            case MainEventType::M_MSG:
                /* Don't accept duplicate messages or messages intended for
                 * previous transmitters. */
                if (ev.msg_id >= this->min_msg_id &&
                    recvd_msgs.find(ev.msg_id) == recvd_msgs.end())
                    this->receive_msg(ev);
                break;
            case MainEventType::M_ACK:
                /* Ignore acknowledgements intended for previous
                 * transmitters.
                 */
                if (ev.msg_id >= this->min_ack_id &&
                    mode == TransmitterMode::SEND)
                    this->set_ack(ev);
                break;
            case MainEventType::M_TIO:
                this->check_resends();
                break;
            }
        }

        iter_func(evs);
    }
}