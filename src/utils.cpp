#include "utils.h"

void msg2packet(std::byte *packet, uint16_t id, OutEventType type, Content &con)
{
    packet[0] = (std::byte)type; /* 0 - Message, 1 - Ack */
    memcpy(packet + 1, &id, sizeof(id));
    memcpy(packet + 1 + sizeof(id), con.get_data(), con.length);
    size_t data_len = 1 + sizeof(id) + con.length;

    /* Calculate CRC from the previous content of the packet.*/
    uint32_t crc = CRC::Calculate(packet, data_len, CRC::CRC_32());
    memcpy(packet + data_len, &crc, sizeof(crc));
}

bool packet2msg(std::byte *packet, uint16_t *id, MainEventType *type,
                std::byte *data)
{
    *type = (MainEventType)packet[0];
    memcpy(id, packet + 1, 2);
    memcpy(data, packet + 3, DATA_LEN);

    size_t data_len = 3 + DATA_LEN;
    uint32_t target_crc = 0;
    memcpy(&target_crc, packet + data_len, sizeof(target_crc));

    uint32_t crc = CRC::Calculate(packet, data_len, CRC::CRC_32());
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