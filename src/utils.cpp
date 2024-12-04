#include "utils.h"
#include "sha256.h"

void msg2packet(std::vector<std::byte> &packet, uint32_t id, OutEventType type,
                std::vector<std::byte> &data)
{
    packet.resize(1 + sizeof(id) + data.size() + CRC_LEN);
    packet[0] = (std::byte)type; /* 0 - Message, 1 - Ack */
    memcpy(&packet[1], &id, sizeof(id));
    memcpy(&packet[1 + sizeof(id)], &data[0], data.size());
    size_t data_len = 1 + sizeof(id) + data.size();

    /* Calculate CRC from the previous content of the packet.*/
    uint32_t crc = CRC::Calculate(&packet[0], data_len, CRC::CRC_32());
    memcpy(&packet[data_len], &crc, sizeof(crc));
}

bool packet2msg(std::vector<std::byte> &packet, uint32_t &id,
                MainEventType &type, std::vector<std::byte> &data)
{
    type = (MainEventType)packet[0];
    memcpy(&id, &packet[1], sizeof(id));

    /* Packet length is 1 + sizeof(id) + data_length + 4 (CRC) */

    size_t data_len = packet.size() - sizeof(id) - CRC_LEN - 1;
    data.resize(data_len);
    memcpy(&data[0], &packet[1 + sizeof(id)], data_len);

    uint32_t target_crc = 0;
    memcpy(&target_crc, &packet[1 + sizeof(id) + data_len], sizeof(target_crc));

    uint32_t crc =
        CRC::Calculate(&packet[0], 1 + sizeof(id) + data_len, CRC::CRC_32());
    return crc == target_crc;
}

size_t get_file_size(const std::string &f_name)
{
    std::ifstream file(f_name, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Couldn't open file :( " + f_name);
    }
    size_t size = file.tellg();
    file.close();
    return size;
}

std::string extract_file_name(const std::string &file_path)
{
    return std::filesystem::path(file_path).filename().string();
}

std::string get_sha(const std::string &f_path)
{
    std::ifstream file{f_path, std::ios::binary};
    if (!file.is_open())
        throw std::runtime_error("Couldn't open file for MD5 :(");

    SHA256 sha;
    std::vector<std::byte> buffer{1024};
    while (file.peek() != EOF) {
        file.read(reinterpret_cast<char *>(buffer.data()), 1024);
        std::size_t bytes_read = file.gcount();
        sha.add(buffer.data(), bytes_read);
    }
    return sha.getHash();
}