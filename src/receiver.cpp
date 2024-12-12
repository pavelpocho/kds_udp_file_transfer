#include "receiver.h"

Receiver::Receiver(int own_port)
{
    /* Create socket. */

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Error: Socket creation failed" << std::endl;
    }

    /* Set up own and receiver address */

    recv_addr_len = sizeof(recv_addr);
    memset(&own_addr, 0, sizeof(own_addr));
    memset(&recv_addr, 0, sizeof(recv_addr));

    own_addr.sin_family = AF_INET;
    own_addr.sin_addr.s_addr = INADDR_ANY;
    own_addr.sin_port = htons(own_port);

    /* Bind socket to be able to listen on specific port. */

    if (bind(sockfd, (const struct sockaddr *)&own_addr, sizeof(own_addr)) <
        0) {
        std::cerr << "Binding socket failed.";
        close(sockfd);
    }

    /* Set socket blocking timeout to maintain responsiveness. */

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

Receiver::~Receiver() { close(sockfd); }

std::string Receiver::listen_for_packets(std::vector<std::byte> &bytes)
{
    memset(buffer, 0, PACKET_LEN);
    ssize_t recvd_bytes =
        recvfrom(sockfd, buffer, PACKET_LEN, 0, (struct sockaddr *)&recv_addr,
                 &recv_addr_len);

    if (recvd_bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return "";
    else if (recvd_bytes < 0)
        throw std::runtime_error("recvfrom failed.");
    else {
        bytes = std::vector<std::byte>{(std::byte *)buffer,
                                       (std::byte *)buffer + recvd_bytes};
        return inet_ntoa(recv_addr.sin_addr);
    }
}