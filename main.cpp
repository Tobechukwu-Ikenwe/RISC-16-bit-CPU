/**
 * 16-bit GPR CPU Emulator - Load and run .asm programs
 *
 * Usage: gpr_emulator [program.asm]
 * If no file given, runs program.asm in current directory.
 * Trace is enabled by default.
 */

// NOTE: fix path to gpr_cpu.h
#include "cpu/gpr_cpu.h"
#include "assembler.h"
#include <string>
#include <iostream>
#include <iomanip>

static void printTraceHeader() {
    std::cout << "\n  PC    | R0    R1    R2    R3    R4    R5    R6    R7    | Z C N | Instruction\n";
    std::cout << "--------+--------------------------------------------------+-------+----------------\n";
}

// NOTE: prefer to use the C++ style version instead of char** -> char[]
int main(int argc, char** argv) {
   // NOTE: string_view is a perfect use
    const char* asmPath = "addition.asm";
    if (argc >= 2)
        asmPath = argv[1];

    Bus bus;
    GPRCPU cpu(bus);

    AssembleResult ar = assembleFile(asmPath, bus.getMemory(), MEMORY_SIZE);
    if (!ar.ok) {
        std::cerr << "Assembly error at line " << ar.lineNum << ": " << ar.error << "\n";
        return 1;
    }

    // Optional: place operands at 0x100 and 0x101 for math programs
    std::cout << "Operand A at 0x100 (decimal or 0x...): ";
    std::string sa;
    std::getline(std::cin, sa);
    if (!sa.empty()) {
        uint16_t a = static_cast<uint16_t>(std::stoul(sa, nullptr, 0));
        bus.write(0x100, a);
        std::cout << "Operand B at 0x101 (decimal or 0x...): ";
        std::string sb;
        std::getline(std::cin, sb);
        if (!sb.empty()) {
            uint16_t b = static_cast<uint16_t>(std::stoul(sb, nullptr, 0));
            bus.write(0x101, b);
        }
    }

    cpu.trace(true);

    std::cout << "\n=== 16-bit GPR CPU Emulator ===\n";
    std::cout << "Program: " << asmPath << "\n";
    printTraceHeader();

    size_t cycles = 0;
    while (cpu.step())
        cycles++;

    std::cout << "\n--- HALTED ---\n";
    std::cout << "Total cycles: " << cycles << "\n";
    std::cout << "R0: " << cpu.getState().R[0] << " (0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.getState().R[0] << std::dec << ")\n";
    uint16_t result = bus.read(0x102);
    std::cout << "Result at 0x102: " << std::dec << result << " (0x" << std::hex << std::setw(4) << std::setfill('0') << result << std::dec << ")\n";

    return 0;
}
