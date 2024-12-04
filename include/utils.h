#ifndef __UTILS__
#define __UTILS__

/* CRC setting #defines go here. */
/* CRC library: https://github.com/d-bahr/CRCpp */

#include "CRC.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <filesystem>
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

/* If launched to receive files, will send packets to ORIGIN_PORT. */
#define ORIGIN_PORT 24824
/* If program launched to send file, will send packets to DEST_PORT. */
#define DEST_PORT 24825

#define DATA_LEN 1015       // bytes
#define PACKET_LEN 1024     // bytes
#define CRC_LEN 4           // bytes
#define RESEND_DELAY 100000 // [us] How long to wait before resending a packet.
#define MAX_RETRIES 20      // Maximum number of times to send a packet.
#define WINDOW_SIZE 4       // How many packets to have "in the air"

/** Declaring controls for behaviour */

extern volatile bool stop;
extern volatile bool sending;
extern volatile uint32_t next_id;

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
typedef struct {
    std::vector<std::byte> content; /* If rcvd, has content of msg. */
    uint32_t msg_id;                /* Message ID of rcvd/ackd msg. */
    std::string origin_ip;          /* Origin IP of incoming packet.*/
    MainEventType type;             /* MSG / ACK / TIO. */
} MainEvent;

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
typedef struct {
    std::vector<std::byte> content; /* If rcvd, has content of msg. */
    uint32_t msg_id;                /* Message ID of rcvd/ackd msg. */
    std::string dest_ip;            /* Origin IP of incoming packet.*/
    OutEventType type;              /* MSG / ACK / TIO. */
} OutEvent;

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
    std::vector<std::byte> content;
    uint32_t id;
    uint8_t retries;
    time_p sent_at;
} SentMessage;

typedef struct {
    std::vector<std::byte> content;
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
void msg2packet(std::vector<std::byte> &packet, uint32_t id, OutEventType type,
                std::vector<std::byte> &data);

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
bool packet2msg(std::vector<std::byte> &packet, uint32_t &id,
                MainEventType &type, std::vector<std::byte> &data);

/**
 * @brief Get the file size.
 *
 * @param f_name
 * @return size_t
 */
size_t get_file_size(const std::string &f_name);

std::string extract_file_name(const std::string &file_path);

std::string get_sha(const std::string &f_path);

#endif /* __UTILS__ */