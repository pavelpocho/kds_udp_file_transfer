#include "checksum_transmitter.h"
#include "file_transmitter.h"
#include "header_transmitter.h"
#include "receiver.h"
#include "sender.h"
#include "transmitter.h"
#include "utils.h"
#include <math.h>

#define UINT8(n) static_cast<uint8_t>(n)
#define FLOAT(n) static_cast<float>(n)

/** Defining controls for behaviour */

volatile bool stop = false;
volatile bool sending = false;
volatile uint32_t next_id = 0;
volatile uint32_t ack_count = 1;

/** Global parameters */

std::string dest_ip;
std::string f_name;

/** Signal queues */

Queue<MainEvent> main_queue;
Queue<OutEvent> out_queue;
std::condition_variable timeout_cv;

/** Helper declarations */

std::string get_own_ip_addr();
void process_args(int argc, char *argv[]);
void terminate(int s);
void setup_sigint_handler();

int fails = 0;

/**************************************************************************/
/* -------------------------- Main definitions -------------------------- */
/**************************************************************************/

void out_thread_main()
{
    Sender sender{sending ? SENDER_TARGET_PORT : RECEIVER_TARGET_PORT};

    while (!stop) {
        std::vector<OutEvent> evs = out_queue.wait_nonempty();
        for (OutEvent ev : evs) {
            std::vector<std::byte> packet;
            msg2packet(packet, ev.msg_id, ev.type, ev.content);

            sender.set_dest_ip(ev.dest_ip);
            sender.send_packet(packet);
        }
    }
}

void in_thread_main()
{
    Receiver receiver{sending ? SENDER_LOCAL_PORT : RECEIVER_LOCAL_PORT};

    while (!stop) {
        std::vector<std::byte> packet;
        std::string recvd_ip = receiver.listen_for_packets(packet);

        /* If no packet received, continue to prevent blocking. */
        if (recvd_ip == "")
            continue;

        MainEvent me = {.content{}, .msg_id{0}, .origin_ip{recvd_ip}, .type{}};
        bool crc_match = packet2msg(packet, me.msg_id, me.type, me.content);

        OutEvent oe = {.content = std::vector<std::byte>{std::byte{
                           crc_match ? UINT8(255) : UINT8(0)}},
                       .msg_id = me.msg_id,
                       .dest_ip = recvd_ip,
                       .type = OutEventType::O_ACK};

        /* If this is a message, send an ACK. */
        if (me.type == MainEventType::M_MSG) {
            for (uint32_t i = 0; i < ack_count; ++i)
                out_queue.push(oe);
        }

        /* If CRC matches, pass upwards. */
        if (crc_match)
            main_queue.push(me);
    }
}

void timeout_thread_main(int delay_us)
{
    while (!stop) {
        std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
        MainEvent e{std::vector<std::byte>{}, 0, "", MainEventType::M_TIO};
        main_queue.push(e);
    }
}

/* Main sending and transmitting logic: */

bool sending_logic()
{
    using namespace std::chrono;
    size_t size = get_file_size(f_name);

    /* 1. Send header: Info about file (name, size) */
    {
        HeaderTransmitter header_transm{dest_ip,   1, 0, main_queue,
                                        out_queue, 0, 0};

        header_transm.send_header_msg(extract_file_name(f_name), size);
        header_transm.run_main_body([](std::vector<MainEvent> _) { (void)_; });

        if (stop)
            return true;
    }

    /* 2. Send file + receive confirmation of checksum */
    {
        /* Number of packets the file requires. +1 is for checksum. */
        uint32_t f_pckt_n = (uint32_t)ceil(size / (float)DATA_LEN) + 1;

        FileTransmitter file_transm{dest_ip,   f_pckt_n, 1, main_queue,
                                    out_queue, 1,        0, f_pckt_n};

        std::string sha = get_sha(f_name);

        auto start = high_resolution_clock::now();

        ack_count = 10;

        file_transm.start_stream_file(f_name, DATA_LEN);
        file_transm.run_main_body(
            [&file_transm, &sha](std::vector<MainEvent> _) {
                (void)_;
                file_transm.continue_stream_file(DATA_LEN, sha);
            });

        if (stop)
            return true;

        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        auto speed = size / FLOAT(duration.count()) * 1000.0f; // [kB / s]

        bool checksum_match = file_transm.receive_checksum_confirmation_msg();
        if (!checksum_match)
            std::cout << "File transfer failed. Retrying..." << std::endl;
        else
            std::cout << "File transfer complete. (Time "
                      << FLOAT(duration.count()) / 1000000.0f
                      << "s, Speed: " << speed << " kB/s, " << f_pckt_n
                      << " packets.)" << std::endl;

        return checksum_match;
    }
}

bool receiving_logic()
{
    /* 1. Receive header: Info about file (name, size) */

    std::string in_f_name{""};
    std::string src_ip{""};
    size_t in_size{0};
    uint32_t f_pckt_n{0};
    bool checksum_match{false};
    {
        HeaderTransmitter header_transm{1, main_queue, out_queue, 0, 0};
        header_transm.run_main_body([](std::vector<MainEvent> _) { (void)_; });
        src_ip = header_transm.src_ip;
        if (stop)
            return true;

        header_transm.receive_header_msg(in_f_name, in_size);
        std::cout << "Receiving file \"" << in_f_name << "\" ("
                  << static_cast<float>(in_size) / 1000.0f << " kB) from "
                  << src_ip << "..." << std::endl;
    }

    /* 2. Receive file */
    {
        /* Number of packets the file requires. +1 is for checksum. */
        f_pckt_n = (uint32_t)ceil(in_size / (float)DATA_LEN) + 1;
        FileTransmitter file_transm{f_pckt_n, main_queue, out_queue,
                                    0,        1,          f_pckt_n};

        file_transm.prep_receive_file(in_f_name);
        file_transm.run_main_body([&file_transm](std::vector<MainEvent> ev) {
            file_transm.receive_stream_file(ev);
        });

        if (stop)
            return true;

        /* Important for checksum to calculate correctly. */
        file_transm.close_write_file();

        std::string dest_md5 = get_sha(in_f_name);
        std::string src_md5 = file_transm.receive_checksum_msg();
        checksum_match = dest_md5 == src_md5;
    }

    /* 3. Send positive/negative checksum confirm */
    {
        ChecksumTransmitter chcksum_transm{
            src_ip, 1, 0, main_queue, out_queue, 0, f_pckt_n + 1};

        chcksum_transm.send_checksum_confirmation_msg(checksum_match);
        chcksum_transm.run_main_body([](std::vector<MainEvent> _) { (void)_; });

        if (stop)
            return true;

        if (!checksum_match)
            std::cout << "File transfer failed. Retrying..." << std::endl;
        else
            std::cout << "File transfer complete." << std::endl;

        return checksum_match;
    }
}

int main(int argc, char *argv[])
{
    process_args(argc, argv);

    std::thread out_thread{out_thread_main};
    std::thread in_thread{in_thread_main};
    std::thread timeout_thread{timeout_thread_main, RESEND_DELAY};

    setup_sigint_handler();

    bool done = false;
    do {
        next_id = 0;
        ack_count = 1;
        done = sending ? sending_logic() : receiving_logic();
    } while (!done);

    terminate(0);

    out_thread.join();
    in_thread.join();
    timeout_thread.join();

    std::cout << "Bye!" << std::endl;
    return 0;
};

/**************************************************************************/
/* -------------------------- Helper functions -------------------------- */
/**************************************************************************/

void process_args(int argc, char *argv[])
{
    if (argc == 3) {
        std::cout << "IP and file name specified, sending file." << std::endl;
        dest_ip = argv[1];
        f_name = argv[2];
        sending = true;
    } else if (argc == 1) {
        std::cout << "No file name or IP specified, listening..." << std::endl;
        std::string own_ip = get_own_ip_addr();
        sending = false;
        if (own_ip != ":(")
            std::cout << "Send files to IP: " << own_ip
                      << " to receive them here." << std::endl;
    } else {
        std::cout << "Error: Wrong number of arguments." << std::endl;
        std::cout << "Provide no arguments to listen for files." << std::endl;
        std::cout << "OR" << std::endl;
        std::cout << "Provide IP address and file name to transmit a file."
                  << std::endl;
        exit(1);
    }
}

std::string get_own_ip_addr()
{
    int sockfd;
    struct sockaddr_in own_addr, remote_addr;
    socklen_t addr_len = sizeof(own_addr);
    char local_ip[INET_ADDRSTRLEN];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        std::cerr << "Error: Socket creation failed" << std::endl;

    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &remote_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) <
        0) {
        std::cerr << "Failed to obtain own IP. (connect failed)" << std::endl;
        close(sockfd);
        return ":(";
    }

    if (getsockname(sockfd, (struct sockaddr *)&own_addr, &addr_len) < 0) {
        std::cerr << "Failed to obtain IP. (getsockname failed)" << std::endl;
        close(sockfd);
        return ":(";
    }

    inet_ntop(AF_INET, &own_addr.sin_addr, local_ip, sizeof(local_ip));
    close(sockfd);
    return local_ip;
}

void terminate(int s)
{
    if (stop)
        return;

    if (s > 0)
        std::cout << std::endl
                  << "Waiting for clean exit... (Signal: " << s << ")"
                  << std::endl;

    stop = true;
    main_queue.cond.notify_all();
    out_queue.cond.notify_all();
}

void setup_sigint_handler()
{
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = terminate;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
}