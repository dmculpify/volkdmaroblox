#include "dma_helper.h"
#include "VolkDMA/process.hh"
#include "VolkDMA/dma.hh"
#include "VolkDMA/inputstate.hh"
#include <vector>
#include <algorithm>
#include <memory>

static Process* s_process = nullptr;
static DMA* s_dma = nullptr;
static std::unique_ptr<InputState> s_input_state;

Process* get_process() {
    return s_process;
}

DMA* get_dma() {
    return s_dma;
}

void set_dma_and_process(DMA* dma, Process* process) {
    s_dma = dma;
    s_process = process;
    s_input_state.reset();
    if (dma && dma->handle)
        s_input_state = std::make_unique<InputState>(*dma);
}

InputState* get_input_state() {
    return s_input_state.get();
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

struct roblox_string_header_t {
    std::uint64_t data_ptr;
    char _pad[8];
    std::int32_t length;
};

static constexpr size_t k_max_username_bytes = 64;

void read_strings_scatter(Process* p, const std::vector<std::uint64_t>& name_ptrs, std::vector<std::string>& out) {
    out.clear();
    if (!p || name_ptrs.empty())
        return;
    out.resize(name_ptrs.size(), "Unknown");
    std::vector<roblox_string_header_t> headers(name_ptrs.size(), { 0, 0 });
    void* scatter = p->create_scatter(0);
    if (!scatter)
        return;
    for (size_t i = 0; i < name_ptrs.size(); ++i) {
        if (name_ptrs[i] > 0x10000 && name_ptrs[i] < 0x7FFFFFFFFFFF && p->is_valid_address(name_ptrs[i]))
            p->add_read_scatter(scatter, name_ptrs[i], &headers[i], sizeof(roblox_string_header_t));
    }
    p->execute_scatter(scatter);
    p->close_scatter(scatter);

    std::vector<std::uint64_t> read_addrs;
    std::vector<size_t> read_indices;
    std::vector<char> buffers(name_ptrs.size() * k_max_username_bytes, 0);
    for (size_t i = 0; i < name_ptrs.size(); ++i) {
        const auto& h = headers[i];
        if (h.length <= 0 || h.length > 255) continue;
        size_t to_read = static_cast<size_t>(std::min(h.length, static_cast<std::int32_t>(k_max_username_bytes)));
        std::uint64_t addr = (h.length >= 16) ? h.data_ptr : name_ptrs[i];
        if (!p->is_valid_address(addr)) continue;
        read_addrs.push_back(addr);
        read_indices.push_back(i);
    }
    if (read_addrs.empty())
        return;
    scatter = p->create_scatter(0);
    if (!scatter)
        return;
    for (size_t r = 0; r < read_addrs.size(); ++r) {
        size_t i = read_indices[r];
        const auto& h = headers[i];
        size_t to_read = static_cast<size_t>(std::min(h.length, static_cast<std::int32_t>(k_max_username_bytes)));
        p->add_read_scatter(scatter, read_addrs[r], &buffers[i * k_max_username_bytes], to_read);
    }
    p->execute_scatter(scatter);
    p->close_scatter(scatter);

    for (size_t r = 0; r < read_addrs.size(); ++r) {
        size_t i = read_indices[r];
        const auto& h = headers[i];
        size_t len = static_cast<size_t>(std::min(h.length, static_cast<std::int32_t>(k_max_username_bytes)));
        const char* buf = &buffers[i * k_max_username_bytes];
        out[i] = std::string(buf, len);
        if (out[i].empty() || out[i].length() > 64)
            out[i] = "Unknown";
    }
}
