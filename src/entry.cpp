#include "receiver.h"
#include "sender.h"
#include "transmission.h"
#include "utils.h"
#include <math.h>

/** Defining controls for behaviour */

volatile bool stop = false;
volatile bool sending = false;
volatile uint16_t next_id = 0;

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

// TODO: With current IDs, there is likely a small limit to the file size!
// TODO: Also with current sequential IDs, there isn't a way to find position
// in file. Make that work...

/**************************************************************************/
/* -------------------------- Main definitions -------------------------- */
/**************************************************************************/

void out_thread_main()
{
    Sender sender{!sending ? ORIGIN_PORT : DEST_PORT};

    while (!stop) {
        std::vector<OutEvent> evs = out_queue.wait_nonempty();
        for (OutEvent ev : evs) {
            std::byte packet[PACKET_LEN] = {(std::byte)0};
            msg2packet(packet, ev.msg_id, ev.type, ev.content);

            sender.set_dest_ip(ev.dest_ip);
            sender.send_packet(packet, PACKET_LEN);
        }
    }
}

void in_thread_main()
{
    std::chrono::high_resolution_clock::now();
    Receiver receiver{sending ? ORIGIN_PORT : DEST_PORT};

    while (!stop) {
        std::byte packet[PACKET_LEN];
        std::string recvd_ip = receiver.listen_for_packets(packet, PACKET_LEN);
        if (recvd_ip == "") /* No packet, would block. */
            continue;

        uint16_t id;
        MainEventType type;
        // TODO: Optimize, two memcpys here
        std::byte data[DATA_LEN];
        bool crc_match = packet2msg(packet, &id, &type, data);

        // TODO: Optimize, don't need full DATA_LEN
        std::byte datab[DATA_LEN];
        if (crc_match && type == MainEventType::M_MSG) {
            /* Send back success. */
            datab[0] = (std::byte)1;

            std::cout << "Successfuly received msg: " << id << std::endl;

            MainEvent me{Content(data, DATA_LEN), id, recvd_ip, type};
            main_queue.push(me);
            OutEvent e{Content(datab, DATA_LEN), id, recvd_ip,
                       OutEventType::O_ACK};
            out_queue.push(e);
        } else if (type == MainEventType::M_MSG) {
            /* Send back failure. */
            datab[0] = (std::byte)0;
            OutEvent e{Content(datab, DATA_LEN), id, recvd_ip,
                       OutEventType::O_ACK};
            out_queue.push(e);
        } else if (crc_match) {
            /* It's an ack, send that to main. */
            MainEvent me{Content(data, DATA_LEN), id, recvd_ip, type};
            main_queue.push(me);
        }
    }
}

void timeout_thread_main(int delay_us)
{
    while (!stop) {
        std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
        MainEvent e{Content(), 0, "", MainEventType::M_TIO};

        // TODO: Only do this if there messages in the air...
        main_queue.push(e);
    }
}

/* Main sending and transmitting logic: */

void sending_logic()
{
    size_t size = get_file_size(f_name);
    /* 1. Send header: Info about file (name, size) */

    {
        Transmission header_transm{dest_ip, 1, 0, main_queue, out_queue};

        header_transm.send_header_msg(extract_file_name(f_name), size);
        header_transm.run_main_body([](std::vector<MainEvent> ev) {});
        if (stop) {
            return;
        }
    }

    std::cout << "All 1 sent header messages were ackd." << std::endl;
    // std::cout << "Press enter to continue." << std::endl;
    // std::string s;
    // std::cin >> s;

    /* 2. Send file + receive confirmation of checksum */

    {
        // +1 is for checksum, +1 for ceil of file size
        // TODO: Fix edgecase: when file is multiple of DATA_LEN bytes long!
        size_t out_msg_count =
            (size_t)ceil(size / (float)DATA_LEN) + 1; // Depends on file size!
        std::cout << "File transmission will have len " << out_msg_count
                  << std::endl;
        Transmission file_transm{dest_ip, out_msg_count, 1, main_queue,
                                 out_queue};
        file_transm.start_stream_file(f_name, DATA_LEN);

        file_transm.run_main_body([&file_transm](std::vector<MainEvent> ev) {
            file_transm.continue_stream_file(DATA_LEN);
        });
        if (stop) {
            return;
        }

        std::string all_string =
            std::string((char *)file_transm.recvd_msgs[0].content.get_data());
        std::cout << "File chcksum confirm: " << all_string << std::endl;
    }

    std::cout << "All " << size / DATA_LEN + 1 + 1
              << "sent file messages were ackd." << std::endl;

    std::cout << "File transfer complete." << std::endl;
}

void receiving_logic()
{
    /* 1. Receive header: Info about file (name, size) */

    std::string in_f_name;
    size_t in_size;
    {
        Transmission header_transm{1, main_queue, out_queue, 0};
        std::cout << "Bout to run receiving of headers." << std::endl;
        header_transm.run_main_body([](std::vector<MainEvent> ev) {});
        if (stop) {
            return;
        }
        std::string all_string =
            std::string((char *)header_transm.recvd_msgs[0].content.get_data());
        size_t start = all_string.find("%*%", 9);
        size_t end = all_string.find("%*%", 12);
        in_f_name = all_string.substr(start + 3, end - start - 3);
        memcpy(&in_size,
               header_transm.recvd_msgs[0].content.get_data() + end + 3,
               sizeof(size_t));
    }

    std::cout << "File name: " << in_f_name << std::endl;
    std::cout << "Size: " << in_size << std::endl;

    // std::cout << "All 1 header msgs were received." << std::endl;
    // std::cout << "Press enter to continue." << std::endl;
    // std::string sq;
    // std::cin >> sq;

    /* 2. Receive file */

    std::string src_ip;

    {
        // +1 as a ceil, +1 for checksum
        // TODO: Fix edgecase: when file is multiple of DATA_LEN bytes long!
        size_t msg_count = (size_t)ceil(in_size / (float)DATA_LEN) +
                           1; // Depends on file size!
        Transmission file_transm{msg_count, main_queue, out_queue, 1};
        file_transm.prep_receive_file(in_f_name);
        file_transm.run_main_body([&file_transm](std::vector<MainEvent> ev) {
            file_transm.receive_stream_file(ev);
        });
        if (stop) {
            return;
        }

        src_ip = file_transm.src_ip;

        // TODO: Check checksum here from disk...

        std::cout << "TODO: Reconstruct file here... " << std::endl;
        std::cout << "TODO: Last msg is checksum... " << std::endl;
    }

    std::cout << "All " << in_size / DATA_LEN + 1 + 1
              << " file msgs were received." << std::endl;

    /* 3. Send positive/negative checksum confirm */

    {
        Transmission chcksum_transm{src_ip, 1, 0, main_queue, out_queue};

        chcksum_transm.send_checksum_msg(false);
        chcksum_transm.run_main_body([](std::vector<MainEvent> ev) {});
    }

    if (stop) {
        return;
    }

    // TODO: Sliding window algorithm.
    // TODO: Think about the IDs... careful about re-sending messages, they will
    // have a different ID in the current configuration!!!!

    // file.run_main_body([] {});

    // std::cout << "Press enter to continue." << std::endl;
    // std::string s;
    // std::cin >> s;
}

int main(int argc, char *argv[])
{
    process_args(argc, argv);

    std::thread out_thread{out_thread_main};
    std::thread in_thread{in_thread_main};
    std::thread timeout_thread{timeout_thread_main, RESEND_DELAY};

    setup_sigint_handler();

    if (sending)
        sending_logic();
    else
        receiving_logic();

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

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Error: Socket creation failed" << std::endl;
    }

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

    std::cout << std::endl
              << "Waiting for clean exit... (Signal: " << s << ")" << std::endl;

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

// Story (sending side):
// 1. File is loaded.
// 2. Main thread says: send header transmission.
// 3. Still on main thread: header transmission messages created, added to
// queue.
// 4. Out_thread: loads messages in queue which have not yet been sent and
// sends them.
// 5. In_thread: receives a packet (ack), signals main with event
// saying which msg ackd.
// 6. Main sets relevant msg as ackd, checks if all msgs ackd,
// if yes, transmission is complete.
// 7. When all msgs ackd., main loop moves on and sends data. Cycle repeats.

// Story (recv side):
// 1. In_thread: receives a packet, sends appropriate event (msg) to
// main.
// 2. In_thread: If msg and CRC fits, send back ack. Send event with msg to
// main.
// 3. Main: Process msg, if it's a START,LEN message, get ready to receive
// data.
// 4. Process repeats with data receiving.

// Key points:
// - Sent and received messages will be local to the main thread - only way
// to communicate with main thread is via Events
// - Main thread can send messages to OUT and receive events (msg/ack) from
// IN with finished message contents or acks.
// - Main deals with acks only on receiving side, it does not send them.

// Timeouts:
// - There will be a fourth thread (timeout thread). When any packets are
// in the air, this thread will periodically signal the main thread to make
// sure it checks if any packets are timing out. If yes, those will be
// resent.