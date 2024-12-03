#include "receiver.h"

Receiver::Receiver(int own_port)
{
    /* Step 1: Create socket. */

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Error: Socket creation failed" << std::endl;
    }

    /* Step 2: Set up own and receiver address */

    recv_addr_len = sizeof(recv_addr);
    memset(&own_addr, 0, sizeof(own_addr));
    memset(&recv_addr, 0, sizeof(recv_addr));

    own_addr.sin_family = AF_INET;
    own_addr.sin_addr.s_addr = INADDR_ANY;
    own_addr.sin_port = htons(own_port);

    /* Step 3: Bind socket. */

    if (bind(sockfd, (const struct sockaddr *)&own_addr, sizeof(own_addr)) <
        0) {
        std::cerr << "Binding socket failed.";
        close(sockfd);
    }

    /* Step 4: Set socket options (block timeout to for responsiveness...) */

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

Receiver::~Receiver() { close(sockfd); }

std::string Receiver::listen_for_packets(std::byte *buffer, size_t buffer_size)
{
    memset(buffer, 0, buffer_size);

    ssize_t recv_bytes =
        recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr *)&recv_addr,
                 &recv_addr_len);

    if (recv_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return "";
        } else {
            std::cout << strerror(errno) << std::endl;
            perror("recvfrom error");
            throw std::runtime_error("recvfrom failed.");
        }
    } else {
        return inet_ntoa(recv_addr.sin_addr);
    }
}