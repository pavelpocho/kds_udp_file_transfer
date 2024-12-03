#ifndef __UTILS__
#define __UTILS__

/* CRC setting #defines go here. */
/* CRC library: https://github.com/d-bahr/CRCpp */

#include "CRC.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <signal.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

/* If launched to receive filess, will send packets to ORIGIN_PORT. */
#define ORIGIN_PORT 24824
/* If program launched to send file, will send packets to DEST_PORT. */
#define DEST_PORT 24825

#define DATA_LEN 1017       // bytes
#define PACKET_LEN 1024     // bytes
#define CRC_LEN 4           // bytes
#define RESEND_DELAY 100000 // [us] How long to wait before resending a packet.

/** Declaring controls for behaviour */

extern volatile bool stop;
extern volatile bool sending;
extern volatile uint16_t next_id;

/** Possible types of main event types:
 *  - Received message
 *  - Acknowledged message
 *  - Timeout check
 */
enum MainEventType { M_MSG, M_ACK, M_TIO };

/** Possible types of out event types:
 *  - Received message
 *  - Acknowledged message
 */
enum OutEventType { O_MSG, O_ACK };

/**
 * @brief Message content class with length.
 *
 */
class Content
{
  public:
    Content() { this->data = NULL; };

    Content(const std::byte *data, size_t length) : length{length}
    {
        this->data = new std::byte[length];
        memcpy(this->data, data, length);
    };

    Content(const Content &other) : length{other.length}
    {
        this->data = new std::byte[length];
        memcpy(this->data, other.data, length);
    }

    ~Content() { delete[] this->data; }

    // Copy Assignment Operator
    Content &operator=(const Content &other)
    {
        if (this == &other) { // Self-assignment check
            return *this;
        }

        // Free existing memory
        delete[] data;

        // Copy the data
        length = other.length;
        this->data = new std::byte[length];
        memcpy(this->data, other.data, length);

        return *this;
    }

    const std::byte *get_data() { return data; }
    size_t length;

  private:
    std::byte *data;
};

/**
 * @brief MainEvents are the only way to trigger main thread work.
 * Use cases:
 *
 *  - Received a message that needs to be saved for processing.
 *
 *  - Received a message acknowledgement that needs to be logged.
 *
 *  - Periodic timeout check from timeout_thread to ensure no
 * messages have been lost.
 */
class MainEvent
{
  public:
    MainEvent(Content content, int msg_id, std::string origin_ip,
              MainEventType type)
    {
        this->content = content;
        this->msg_id = msg_id;
        this->type = type;
        this->origin_ip = origin_ip;
    };

    ~MainEvent() {}

    Content content;       /* If rcvd, has content of msg. */
    int msg_id;            /* Message ID of rcvd/ackd msg. */
    std::string origin_ip; /* Origin IP of incoming packet.*/
    MainEventType type;    /* MSG / ACK / TIO. */
};

/**
 * @brief OutEvents are the only way to trigger out_thread work.
 * Use cases:
 *
 *  - Message needs to be sent. In this case, the main thread will
 * use this struct to pass relevant info.
 *
 *  - Acknowledgement of message receipt needs to be sent. In this case,
 * the receiving thread will use this struct to pass relevant info.
 * Sending message acks is done without involving the main thread.
 *
 */
class OutEvent
{
  public:
    OutEvent(Content content, int msg_id, std::string dest_ip,
             OutEventType type)
    {
        this->content = content;
        this->msg_id = msg_id;
        this->type = type;
        this->dest_ip = dest_ip;
    };

    ~OutEvent() {}

    Content content;     /* If rcvd, has content of msg. */
    int msg_id;          /* Message ID of rcvd/ackd msg. */
    std::string dest_ip; /* Origin IP of incoming packet.*/
    OutEventType type;   /* MSG / ACK / TIO. */
};

/**
 * @brief Generic queue suited for multi-threaded work.
 */
template <typename T> struct Queue {
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cond;

    void push(const T &item)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(item);
        }
        cond.notify_one();
    }

    std::vector<T> wait_nonempty()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cond.wait(lock, [this] { return !queue.empty() || stop; });
        std::vector<T> list;
        while (!queue.empty())
            list.push_back(pop());

        return list;
    }

    T pop()
    {
        T item = queue.front();
        queue.pop();
        return item;
    }

    bool empty()
    {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};

typedef std::chrono::_V2::system_clock::time_point time_p;

typedef struct {
    bool ackd;
    Content content;
    uint16_t id;
    uint8_t retries;
    time_p sent_at;
} SentMessage;

typedef struct {
    Content content;
    time_p received_at;
} RecvdMessage;

/**
 * @brief Message to packet encoding: Rules:
 *
 * - First byte indicates if it's data or ack.
 *
 * - Second, third byte is 16-bit ID.
 *
 * - Then DATA_LEN bytes of data.
 *
 * - Then 4 bytes of CRC from all previous pieces.
 *
 * Total length of packet is `1 + 2 + DATA_LEN +4 =
 * DATA_LEN + 7 = PACKET_LEN.`
 *
 * @param packet
 * @param id
 * @param type
 * @param content
 */
void msg2packet(std::byte *packet, uint16_t id, OutEventType type,
                Content &con);

/**
 * @brief Packet to message decoding: Rules:
 *
 * - Decode per rules from msg2packet.
 *
 * @param packet
 * @param id
 * @param type
 * @param content
 *
 * @return bool If CRC matches.
 */
bool packet2msg(std::byte *packet, uint16_t *id, MainEventType *type,
                std::byte *data);

/**
 * @brief Get the file size.
 *
 * @param f_name
 * @return size_t
 */
size_t get_file_size(const std::string &f_name);

#endif /* __UTILS__ */