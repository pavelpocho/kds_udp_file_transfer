#ifndef __SENDER__
#define __SENDER__

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

class Sender
{
  public:
    Sender(int dest_port);

    ~Sender();

    /**
     * @brief Set the destination IP address to the provided value.
     *
     * @param dest_ip
     */
    void set_dest_ip(std::string dest_ip);

    /**
     * @brief Send packet of bytes of certain length.
     *
     * @param data
     * @param data_len
     * @return true
     * @return false
     */
    bool send_packet(std::vector<std::byte> packet);

  private:
    int sockfd;

    struct sockaddr_in dest_addr;

    std::string dest_ip;
};

#endif /* __SENDER__ */