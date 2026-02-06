/**
 * 16-bit GPR CPU Emulator - Header
 * Educational RISC-style CPU with 8 GPRs, PC, and Flags.
 */

#ifndef GPR_CPU_H
#define GPR_CPU_H

#include <cstdint>
#include <cstddef>

// =============================================================================
// MEMORY & BUS
// =============================================================================

// NOTE: nice use of constexpr
/** 64KB addressable memory (2^16 = 65536 words, each 16 bits) */
constexpr size_t MEMORY_SIZE = 65536;

/**
 * Bus: Simple abstraction for memory reads/writes.
 * Decouples the CPU from raw memory and allows future expansion (e.g., MMIO).
 */
class Bus {
public:
    Bus();
    ~Bus();

    /** Read 16-bit word at address. Returns 0 if address out of range. */
    uint16_t read(uint16_t address) const;

    /** Write 16-bit word at address. No-op if address out of range. */
    void write(uint16_t address, uint16_t value);

    /** Direct pointer to memory for loading programs (use with care). */
    uint16_t* getMemory() { return memory; }
    const uint16_t* getMemory() const { return memory; }

private:
   // always intialize private mems eitheir _memory or m_memory
    uint16_t* memory;
};

// =============================================================================
// FLAGS REGISTER (16-bit, we use 3 bits: Zero, Carry, Negative)
// =============================================================================

/** Flag bit positions in the 16-bit flags register */
constexpr uint16_t FLAG_ZERO    = (1 << 0);  // bit 0: result was zero
constexpr uint16_t FLAG_CARRY   = (1 << 1);  // bit 1: carry/borrow from ALU
constexpr uint16_t FLAG_NEGATIVE = (1 << 2); // bit 2: result negative (bit 15 set)

// =============================================================================
// INSTRUCTION OPCODES (4-bit opcode in bits 15-12 of instruction)
// =============================================================================

// NOTE: good use of enum class
enum class Opcode : uint8_t {
    HALT = 0,
    MOVI,   // Move immediate (9-bit) into register
    MOV,    // Copy register: Rd = Rs
    LOAD,   // Rd = memory[Rs]
    STORE,  // memory[Rs] = Rd
    ADD,
    SUB,
    AND,
    OR,
    XOR,
    NOT,
    SHL,    // Shift left by 1
    SHR,    // Shift right logical by 1
    JMP,    // PC = Rs (jump to address in Rs)
    JZ,     // If Zero flag set, PC = Rs
    NOP
};

// =============================================================================
// CPU STATE
// =============================================================================

struct CPUState {
   // prefer to use std::array here
    uint16_t R[8];       // General Purpose Registers R0-R7
    uint16_t PC;         // Program Counter (next instruction address)
    uint16_t FLAGS;      // Flags: Zero, Carry, Negative
    bool halted;         // True after HALT instruction
};

/**
 * 16-bit GPR CPU: Implements Fetch-Decode-Execute cycle and full ISA.
 */
class GPRCPU {
public:
    GPRCPU(Bus& bus);

    /** Reset CPU: clear registers, PC=0, clear flags, not halted. */
    void reset();

    /** Execute one FDE cycle. Returns false if CPU is halted. */
    bool step();

    /** Run until HALT. Returns number of cycles executed. */
    size_t run();

    /** Access current state (for debugger/trace). */
    const CPUState& getState() const { return state; }
    CPUState& getState() { return state; }

    /** Trace: print registers, PC, flags, and current instruction. */
    void trace(bool enable) { tracing = enable; }
    bool isTracing() const { return tracing; }

private:
    // NOTE: private member _ or m_
    Bus& bus;
    CPUState state;
    bool tracing;

    // --- Decoding helpers (bitwise masking and shifting) ---
    // Instruction format: [15:12] opcode, [11:9] Rd, [8:6] Rs, [5:0] extra/imm
    // For MOVI: [15:12]=opcode, [11:9]=Rd, [8:0]=9-bit immediate

    /** Extract 4-bit opcode from bits 15-12: right-shift by 12, mask with 0xF. */
    static uint8_t decodeOpcode(uint16_t inst);

    /** Extract 3-bit destination register (bits 11-9): shift right 9, mask 0x7. */
    static uint8_t decodeRd(uint16_t inst);

    /** Extract 3-bit source register (bits 8-6): shift right 6, mask 0x7. */
    static uint8_t decodeRs(uint16_t inst);

    /** Extract 9-bit immediate (bits 8-0) for MOVI: mask with 0x1FF. */
    static uint16_t decodeImm9(uint16_t inst);

    /** Update Zero and Negative flags from 16-bit result. Clear Carry. */
    void setResultFlags(uint16_t result);

    /** Update Zero, Carry, and Negative after ADD. */
    void setAddFlags(uint16_t a, uint16_t b, uint16_t result);

    /** Update Zero, Carry, and Negative after SUB (Carry = !borrow). */
    void setSubFlags(uint16_t a, uint16_t b, uint16_t result);

    /** Execute one instruction (after fetch and decode). */
    void execute(uint16_t instruction);
};

#endif // GPR_CPU_H
