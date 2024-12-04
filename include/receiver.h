#ifndef __RECEIVER__
#define __RECEIVER__

#include "utils.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

class Receiver
{
  public:
    Receiver(int own_port);

    ~Receiver();

    /**
     * @brief Listen for a packet. Will return after some time even if no
     * data was received.
     *
     * @param bytes
     * @return std::string IP of source. Empty if no data received.
     */
    std::string listen_for_packets(std::vector<std::byte> &bytes);

  private:
    int sockfd;

    struct sockaddr_in own_addr, recv_addr;

    socklen_t recv_addr_len;

    std::byte buffer[PACKET_LEN];
};

#endif /* __RECEIVER__ */