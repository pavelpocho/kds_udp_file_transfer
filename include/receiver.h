#ifndef __RECEIVER__
#define __RECEIVER__

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define RECV_BUFFER_SIZE 1024

class Receiver
{
  public:
    Receiver(int own_port);
    ~Receiver();

    /**
     * @brief
     *
     * @param buffer
     * @return std::string IP of source.
     */
    std::string listen_for_packets(std::byte *buffer, size_t buffer_size);

  private:
    int sockfd;
    struct sockaddr_in own_addr, recv_addr;
    socklen_t recv_addr_len;
};

#endif /* __RECEIVER__ */