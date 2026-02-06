#pragma once
#include <cstdint>
#include <string>
#include <vector>

class Process;
class DMA;
class InputState;

Process* get_process();
DMA* get_dma();
void set_dma_and_process(class DMA* dma, Process* process);
InputState* get_input_state();

std::string read_string(Process* p, std::uint64_t address);

void read_strings_scatter(Process* p, const std::vector<std::uint64_t>& name_ptrs, std::vector<std::string>& out);
