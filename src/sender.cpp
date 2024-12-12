#include "sender.h"

Sender::Sender(int dest_port)
{
    /* Create socket. */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        std::cerr << "Error: Socket creation failed" << std::endl;

    /* Set destination port. */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
}

Sender::~Sender() { close(sockfd); }

void Sender::set_dest_ip(std::string dest_ip)
{
    /* Set destination IP address if not already set. */
    if (this->dest_ip == dest_ip)
        return;

    inet_pton(AF_INET, dest_ip.c_str(), &dest_addr.sin_addr);
    this->dest_ip = dest_ip;
}

bool Sender::send_packet(std::vector<std::byte> packet)
{
    /* Send packet (bytes) of defined length. */
    ssize_t sent_bytes =
        sendto(sockfd, &packet[0], packet.size(), MSG_DONTWAIT,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (sent_bytes < 0)
        std::cerr << "Send failed!" << std::endl;

    return sent_bytes >= 0;
}