#ifndef __SENDER__
#define __SENDER__

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

class Sender
{
  public:
    Sender(int dest_port);
    ~Sender();
    void set_dest_ip(std::string dest_ip);
    bool send_packet(const std::byte *data, int data_len);

  private:
    int sockfd;
    struct sockaddr_in dest_addr;
    std::string dest_ip;
};

#endif /* __SENDER__ */