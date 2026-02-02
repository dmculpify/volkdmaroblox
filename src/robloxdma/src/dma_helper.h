#pragma once
#include <cstdint>
#include <string>

class Process;

Process* get_process();
void set_dma_and_process(class DMA* dma, Process* process);

std::string read_string(Process* p, std::uint64_t address);
