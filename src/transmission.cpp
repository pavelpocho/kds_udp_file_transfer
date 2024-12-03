#include "transmission.h"

// Transmission workflow:
// 1. SENDING DATA
// a.

Transmission::Transmission(std::string &dest_ip, size_t out_msg_count,
                           size_t in_msg_count, Queue<MainEvent> &main_queue,
                           Queue<OutEvent> &out_queue, uint16_t min_ack_id,
                           uint16_t min_msg_id)
    : Transmission(in_msg_count, main_queue, out_queue, min_ack_id, min_msg_id)
{
    this->dest_ip = dest_ip;
    this->out_msg_count = out_msg_count;
    this->mode = TransmissionMode::SEND;
}

Transmission::Transmission(size_t in_msg_count, Queue<MainEvent> &main_queue,
                           Queue<OutEvent> &out_queue, uint16_t min_ack_id,
                           uint16_t min_msg_id)
    : main_queue{main_queue}, out_queue{out_queue}
{
    this->dest_ip = "";
    this->in_msg_count = in_msg_count;
    this->out_msg_count = 0;
    this->mode = TransmissionMode::RECEIVE;
    this->min_ack_id = min_ack_id;
    this->min_msg_id = min_msg_id;
}

Transmission::~Transmission()
{
    if (file_o.is_open()) {
        file_o.close();
    }
}

void Transmission::send_header_msg(const std::string f_name, const size_t size)
{
    std::size_t max_ch = (std::size_t)(std::min((int)f_name.length(), 128));
    std::string str = "%*%HEADER%*%" + f_name.substr(0, max_ch) + "%*%";
    std::byte data[DATA_LEN];
    memcpy(data, str.c_str(), str.length());
    memcpy(data + str.length(), &size, sizeof(size_t));
    send_msg(data, DATA_LEN);
}

void Transmission::send_checksum_msg(bool match)
{
    std::string val = (match ? "YES" : "NO");
    std::string str = "%*%CHKSUM%*%" + val + "%*%";
    std::byte data[DATA_LEN];
    memcpy(data, str.c_str(), str.length());
    send_msg(data, DATA_LEN);
}

void Transmission::send_msg(const std::byte *data, size_t length)
{
    std::cout << "Just before sending, len of sent is " << sent_msgs.size()
              << std::endl;
    std::cout << "Sending a message with id " << next_id << std::endl;
    const uint16_t id = next_id++;
    SentMessage sent_message{.ackd = false,
                             .content = Content(data, length),
                             .id = id,
                             .retries = 1,
                             .sent_at =
                                 std::chrono::high_resolution_clock::now()};

    sent_msgs[id] = sent_message;
    OutEvent e{sent_message.content, id, dest_ip, OutEventType::O_MSG};
    out_queue.push(e);
};

void Transmission::receive_msg(MainEvent ev)
{
    // If received duplicate of message which failed to be
    // properly acknowledged before, discard it:
    if (ev.msg_id < this->min_msg_id) {
        return;
    }
    std::cout << "Recvd size: " << recvd_msgs.size() << std::endl;
    std::cout << "Recvd and ackd: " << ev.msg_id << std::endl;
    if (recvd_msgs.find(ev.msg_id) == recvd_msgs.end()) {
        this->src_ip = ev.origin_ip;
        recvd_msgs[ev.msg_id] = RecvdMessage{
            .content = ev.content,
            .received_at = std::chrono::high_resolution_clock::now(),
        };
    }
    std::cout << "Checking completion from receiving msg..." << std::endl;
    check_completion();
}

// TODO: The acks use way too much bandwidth...
// TODO: And this function will need to be changed when the
// size of it is reduced...
void Transmission::set_ack(MainEvent ev)
{
    // If received duplicate of message which failed to be
    // properly acknowledged before, discard it:
    // This check is here like three times...
    if (ev.msg_id < this->min_ack_id) {
        return;
    }
    std::cout << "Got ack for msg id " << ev.msg_id << std::endl;
    sent_msgs[ev.msg_id].ackd = (int)ev.content.get_data()[0] > 0;
    std::cout << "Checking completion from receiving ACK..." << std::endl;
    check_completion();
}

void Transmission::check_completion()
{
    /* Check for completion. */
    bool all_ackd = true;
    for (const auto &msg : sent_msgs) {
        if (!msg.second.ackd) {
            all_ackd = false;
            break;
        }
    }

    std::cout << "Recvd size: " << recvd_msgs.size()
              << ", in msg count: " << this->in_msg_count << std::endl;

    std::cout << "Sent size: " << sent_msgs.size()
              << ", out msg count: " << this->out_msg_count << std::endl;

    for (auto s : sent_msgs) {
        std::cout << "Already send message ID: " << s.second.id << std::endl;
    }

    std::cout << "Are all ackd?: " << all_ackd << std::endl;

    this->done = recvd_msgs.size() == this->in_msg_count &&
                 sent_msgs.size() == this->out_msg_count && all_ackd;
}

bool Transmission::is_complete() { return done; }

void Transmission::check_resends(MainEvent ev)
{
    auto now = std::chrono::high_resolution_clock::now();
    for (const auto &msg : sent_msgs) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            now - msg.second.sent_at);
        if (duration.count() > RESEND_DELAY && !msg.second.ackd) {
            std::cout << "Message " << msg.second.id << " overdue" << std::endl;
            OutEvent e{msg.second.content, msg.second.id, dest_ip,
                       OutEventType::O_MSG};
            out_queue.push(e);
        }
    }
}

void Transmission::send_header_transmission() {}

// TODO: Split into start and continue... to keep N see below how...
void Transmission::start_stream_file(const std::string &filename,
                                     std::size_t chunk_size)
{
    file = std::ifstream{filename, std::ios::binary};
    if (!file.is_open())
        throw std::runtime_error("Couldn't open file :( " + filename);

    std::vector<std::byte> buffer(chunk_size);
    for (int i = 0; i < 10; ++i) {
        file.read(reinterpret_cast<char *>(buffer.data()), chunk_size);
        std::size_t bytes_read = file.gcount();

        if (bytes_read > 0) {
            // TODO: Change this once variable DATA_SIZE done...
            send_msg(buffer.data(), chunk_size);
        }
    }

    std::cout << "So far sent: " << file.tellg() << " bytes" << std::endl;
}

// TODO: Split into start and continue... to keep N see below how...
void Transmission::continue_stream_file(std::size_t chunk_size)
{
    // TODO: Actual N logic!!!!!!!
    if (file.tellg() == -1) {
        if (this->sent_checksum) {
            return;
        }
        const char *checksum = "HELLO CHECKSUM";
        send_msg((std::byte *)checksum, chunk_size);
        this->sent_checksum = true;
        return;
    }
    std::vector<std::byte> buffer(chunk_size);
    for (int i = 0; i < 1; ++i) {
        file.read(reinterpret_cast<char *>(buffer.data()), chunk_size);
        std::size_t bytes_read = file.gcount();

        std::cout << "Sending: " << bytes_read << " bytes" << std::endl;

        if (bytes_read > 0) {
            // TODO: Change this once variable DATA_SIZE done...
            send_msg(buffer.data(), chunk_size);
        }
    }

    std::cout << "So far sent: " << file.tellg() << " bytes" << std::endl;

    if (file.peek() == EOF) {
        std::cout << "Closing file!" << std::endl;
        file.close();
    }
}

void Transmission::prep_receive_file(const std::string &f_name)
{
    std::string file_path = "./data/" + f_name;
    std::cout << "FPATH:" << file_path << std::endl;
    file_o.open(file_path, std::ios::binary | std::ios::out);
    if (!file_o.is_open()) {
        throw std::runtime_error("Couldn't open file for writing :( ");
    }
}

void Transmission::receive_stream_file(std::vector<MainEvent> evs)
{
    if (!file_o.is_open()) {
        throw std::runtime_error("File for writing not open...");
    }

    for (MainEvent ev : evs) {
        // TODO: These checks are duplicate... fix that...
        if (ev.type != MainEventType::M_MSG || ev.msg_id < this->min_msg_id)
            continue;

        // TODO: FIX! THIS WILL RECEIVE THE CHECKSUM AS WELL...
        auto &c = ev.content;
        // TODO: FIX! THE LAST CHUNK WILL MAKE THE FILE TOO LONG!
        // TODO: It's much worse... it puts strings from the program in
        // there....
        std::cout << "Writing to file, len: " << c.length << std::endl;
        file_o.write(reinterpret_cast<const char *>(c.get_data()), c.length);
    }
}

void Transmission::run_main_body(
    std::function<void(std::vector<MainEvent>)> iter_func)
{
    while (!this->is_complete() && !stop) {
        std::vector<MainEvent> evs = main_queue.wait_nonempty();
        if (this->is_complete() || stop) {
            break;
        }
        for (MainEvent ev : evs) {
            switch (ev.type) {
            case MainEventType::M_MSG:
                this->receive_msg(ev);
                break;
            case MainEventType::M_ACK:
                this->set_ack(ev);
                break;
            case MainEventType::M_TIO:
                this->check_resends(ev);
                break;
            }
        }

        iter_func(evs);
    }
}