#include "dma_helper.h"
#include "VolkDMA/process.hh"
#include "VolkDMA/dma.hh"
#include <vector>

static Process* s_process = nullptr;
static DMA* s_dma = nullptr;

Process* get_process() {
    return s_process;
}

void set_dma_and_process(DMA* dma, Process* process) {
    s_dma = dma;
    s_process = process;
}

std::string read_string(Process* p, std::uint64_t address) {
    if (!p || !p->is_valid_address(address))
        return "Unknown";
    std::int32_t string_length = p->read<std::int32_t>(address + 0x10);
    if (string_length <= 0 || string_length > 255)
        return "Unknown";
    std::uint64_t string_address = (string_length >= 16) ? p->read<std::uint64_t>(address) : address;
    std::vector<char> buffer(static_cast<size_t>(string_length) + 1, 0);
    if (!p->read(string_address, buffer.data(), static_cast<size_t>(string_length)))
        return "Unknown";
    return std::string(buffer.data(), static_cast<size_t>(string_length));
}
