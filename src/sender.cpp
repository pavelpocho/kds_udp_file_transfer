#include "sender.h"

Sender::Sender(int dest_port)
{
    /* Step 1: Create socket. */

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Error: Socket creation failed" << std::endl;
    }

    /* Step 2: Set up destination address. */

    memset(&dest_addr, 0, sizeof(dest_addr));

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port); /* Destination address... */
}

Sender::~Sender() { close(sockfd); }

void Sender::set_dest_ip(std::string dest_ip)
{
    if (this->dest_ip != dest_ip) {
        inet_pton(AF_INET, dest_ip.c_str(), &dest_addr.sin_addr);
        this->dest_ip = dest_ip;
    }
}

bool Sender::send_packet(const std::byte *data, int data_len)
{
    /* Step 3: Send data... */

    ssize_t sent_bytes =
        sendto(sockfd, data, data_len, MSG_DONTWAIT,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (sent_bytes < 0) {
        std::cerr << "Send failed!" << std::endl;
        return false;
    } else {
        return true;
    }
}