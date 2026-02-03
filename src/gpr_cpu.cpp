/**
 * 16-bit GPR CPU Emulator - Implementation
 */

#include "gpr_cpu.h"
#include <iostream>
#include <iomanip>

// =============================================================================
// BUS
// =============================================================================

Bus::Bus() {
    memory = new uint16_t[MEMORY_SIZE]();
}

Bus::~Bus() {
    delete[] memory;
}

uint16_t Bus::read(uint16_t address) const {
    // address is 16-bit so 0..65535; cast to size_t for comparison with MEMORY_SIZE
    if (static_cast<size_t>(address) < MEMORY_SIZE)
        return memory[address];
    return 0;
}

void Bus::write(uint16_t address, uint16_t value) {
    if (static_cast<size_t>(address) < MEMORY_SIZE)
        memory[address] = value;
}

// =============================================================================
// DECODE HELPERS (Bitwise operations for instruction decoding)
// =============================================================================
// In C++, we use:
//   - Right shift (>>) to move a bit field to the least significant bits.
//   - Bitwise AND (&) with a mask to keep only those bits (mask = (1<<n)-1 for n bits).

uint8_t GPRCPU::decodeOpcode(uint16_t inst) {
    // Opcode is in bits 15-12. Shift right by 12 so bits 15-12 become 3-0.
    // Mask with 0xF (binary 1111) to keep only 4 bits.
    return static_cast<uint8_t>((inst >> 12) & 0xFu);
}

uint8_t GPRCPU::decodeRd(uint16_t inst) {
    // Rd is in bits 11-9. Shift right by 9, mask with 0x7 (111) for 3 bits.
    return static_cast<uint8_t>((inst >> 9) & 0x7u);
}

uint8_t GPRCPU::decodeRs(uint16_t inst) {
    // Rs is in bits 8-6. Shift right by 6, mask with 0x7.
    return static_cast<uint8_t>((inst >> 6) & 0x7u);
}

uint16_t GPRCPU::decodeImm9(uint16_t inst) {
    // 9-bit immediate is in bits 8-0. Mask with 0x1FF (9 ones in binary).
    return inst & 0x1FFu;
}

// =============================================================================
// FLAG UPDATES
// =============================================================================

void GPRCPU::setResultFlags(uint16_t result) {
    state.FLAGS &= ~(FLAG_ZERO | FLAG_CARRY | FLAG_NEGATIVE); // Clear all first
    if (result == 0)
        state.FLAGS |= FLAG_ZERO;
    if (result & 0x8000u)  // Bit 15 set = negative in 16-bit signed view
        state.FLAGS |= FLAG_NEGATIVE;
}

void GPRCPU::setAddFlags(uint16_t a, uint16_t b, uint16_t result) {
    state.FLAGS &= ~(FLAG_ZERO | FLAG_CARRY | FLAG_NEGATIVE);
    if (result == 0)
        state.FLAGS |= FLAG_ZERO;
    if (result & 0x8000u)
        state.FLAGS |= FLAG_NEGATIVE;
    // Carry: overflow from bit 15 (sum of two 16-bit values produced 17-bit result)
    if ((a + b) > 0xFFFFu)
        state.FLAGS |= FLAG_CARRY;
}

void GPRCPU::setSubFlags(uint16_t a, uint16_t b, uint16_t result) {
    state.FLAGS &= ~(FLAG_ZERO | FLAG_CARRY | FLAG_NEGATIVE);
    if (result == 0)
        state.FLAGS |= FLAG_ZERO;
    if (result & 0x8000u)
        state.FLAGS |= FLAG_NEGATIVE;
    // Carry here means "no borrow": a >= b. So carry set when a >= b.
    if (a >= b)
        state.FLAGS |= FLAG_CARRY;
}

// =============================================================================
// CPU CONSTRUCTION & RESET
// =============================================================================

GPRCPU::GPRCPU(Bus& bus) : bus(bus), tracing(false) {
    reset();
}

void GPRCPU::reset() {
    for (unsigned i = 0; i < 8; ++i)
        state.R[i] = 0;
    state.PC = 0;
    state.FLAGS = 0;
    state.halted = false;
}

// =============================================================================
// FETCH-DECODE-EXECUTE (one step)
// =============================================================================

bool GPRCPU::step() {
    if (state.halted)
        return false;

    // --- FETCH: Read instruction at PC from memory via bus ---
    uint16_t instruction = bus.read(state.PC);

    if (tracing) {
        std::cout << "\n--- Cycle @ PC=0x" << std::hex << std::setw(4) << std::setfill('0') << state.PC << " ---\n";
        std::cout << "  Instruction: 0x" << std::setw(4) << instruction << "\n";
        std::cout << "  R0=" << std::setw(4) << state.R[0] << " R1=" << std::setw(4) << state.R[1]
                  << " R2=" << std::setw(4) << state.R[2] << " R3=" << std::setw(4) << state.R[3]
                  << " R4=" << std::setw(4) << state.R[4] << " R5=" << std::setw(4) << state.R[5]
                  << " R6=" << std::setw(4) << state.R[6] << " R7=" << std::setw(4) << state.R[7] << "\n";
        std::cout << "  FLAGS: Z=" << ((state.FLAGS & FLAG_ZERO) ? 1 : 0)
                  << " C=" << ((state.FLAGS & FLAG_CARRY) ? 1 : 0)
                  << " N=" << ((state.FLAGS & FLAG_NEGATIVE) ? 1 : 0) << "\n";
        std::cout << std::dec;
    }

    // --- DECODE: Advance PC to next instruction (most instructions are 1 word) ---
    state.PC += 1;

    // --- EXECUTE: Perform the operation ---
    execute(instruction);

    return !state.halted;
}

void GPRCPU::execute(uint16_t instruction) {
    uint8_t op = decodeOpcode(instruction);
    uint8_t rd = decodeRd(instruction);
    uint8_t rs = decodeRs(instruction);
    uint16_t imm9 = decodeImm9(instruction);

    switch (static_cast<Opcode>(op)) {
        case Opcode::HALT:
            state.halted = true;
            if (tracing) std::cout << "  [EXEC] HALT\n";
            break;

        case Opcode::MOVI: {
            // Rd = 9-bit immediate (zero-extended to 16 bits)
            state.R[rd] = imm9;
            setResultFlags(state.R[rd]);
            if (tracing) std::cout << "  [EXEC] MOVI R" << static_cast<unsigned>(rd) << ", " << imm9 << "\n";
            break;
        }

        case Opcode::MOV: {
            state.R[rd] = state.R[rs];
            setResultFlags(state.R[rd]);
            if (tracing) std::cout << "  [EXEC] MOV R" << static_cast<unsigned>(rd) << ", R" << static_cast<unsigned>(rs) << "\n";
            break;
        }

        case Opcode::LOAD: {
            uint16_t addr = state.R[rs];
            state.R[rd] = bus.read(addr);
            setResultFlags(state.R[rd]);
            if (tracing) std::cout << "  [EXEC] LOAD R" << static_cast<unsigned>(rd) << ", (R" << static_cast<unsigned>(rs)
                << ")  ; R" << static_cast<unsigned>(rd) << " = mem[0x" << std::hex << std::setw(4) << std::setfill('0') << addr
                << "] = 0x" << state.R[rd] << std::dec << "\n";
            break;
        }

        case Opcode::STORE: {
            uint16_t addr = state.R[rs];
            bus.write(addr, state.R[rd]);
            if (tracing) std::cout << "  [EXEC] STORE R" << static_cast<unsigned>(rd) << ", (R" << static_cast<unsigned>(rs)
                << ")  ; mem[0x" << std::hex << std::setw(4) << std::setfill('0') << addr << "] = 0x" << state.R[rd] << std::dec << "\n";
            break;
        }

        case Opcode::ADD: {
            uint16_t a = state.R[rd], b = state.R[rs];
            uint16_t result = a + b;
            state.R[rd] = result;
            setAddFlags(a, b, result);
            if (tracing) std::cout << "  [EXEC] ADD R" << static_cast<unsigned>(rd) << ", R" << static_cast<unsigned>(rs)
                << "  ; R" << static_cast<unsigned>(rd) << " = 0x" << std::hex << std::setw(4) << a << " + 0x" << b << " = 0x" << result << std::dec << "\n";
            break;
        }

        case Opcode::SUB: {
            uint16_t a = state.R[rd], b = state.R[rs];
            uint16_t result = a - b;
            state.R[rd] = result;
            setSubFlags(a, b, result);
            if (tracing) std::cout << "  [EXEC] SUB R" << static_cast<unsigned>(rd) << ", R" << static_cast<unsigned>(rs)
                << "  ; R" << static_cast<unsigned>(rd) << " = 0x" << std::hex << std::setw(4) << a << " - 0x" << b << " = 0x" << result << std::dec << "\n";
            break;
        }

        case Opcode::AND: {
            state.R[rd] = state.R[rd] & state.R[rs];
            setResultFlags(state.R[rd]);
            if (tracing) std::cout << "  [EXEC] AND R" << static_cast<unsigned>(rd) << ", R" << static_cast<unsigned>(rs) << "\n";
            break;
        }

        case Opcode::OR: {
            state.R[rd] = state.R[rd] | state.R[rs];
            setResultFlags(state.R[rd]);
            if (tracing) std::cout << "  [EXEC] OR R" << static_cast<unsigned>(rd) << ", R" << static_cast<unsigned>(rs) << "\n";
            break;
        }

        case Opcode::XOR: {
            state.R[rd] = state.R[rd] ^ state.R[rs];
            setResultFlags(state.R[rd]);
            if (tracing) std::cout << "  [EXEC] XOR R" << static_cast<unsigned>(rd) << ", R" << static_cast<unsigned>(rs) << "\n";
            break;
        }

        case Opcode::NOT: {
            state.R[rd] = ~state.R[rs];
            setResultFlags(state.R[rd]);
            if (tracing) std::cout << "  [EXEC] NOT R" << static_cast<unsigned>(rd) << ", R" << static_cast<unsigned>(rs)
                << "  ; R" << static_cast<unsigned>(rd) << " = ~R" << static_cast<unsigned>(rs) << "\n";
            break;
        }

        case Opcode::SHL: {
            uint16_t val = state.R[rd];
            state.R[rd] = val << 1;
            state.FLAGS &= ~(FLAG_ZERO | FLAG_CARRY | FLAG_NEGATIVE);
            if (state.R[rd] == 0) state.FLAGS |= FLAG_ZERO;
            if (state.R[rd] & 0x8000u) state.FLAGS |= FLAG_NEGATIVE;
            if (val & 0x8000u) state.FLAGS |= FLAG_CARRY; // bit 15 was set, so it carried out
            if (tracing) std::cout << "  [EXEC] SHL R" << static_cast<unsigned>(rd) << "  ; R" << static_cast<unsigned>(rd)
                << " = 0x" << std::hex << std::setw(4) << std::setfill('0') << val << " << 1 = 0x" << state.R[rd] << std::dec << "\n";
            break;
        }

        case Opcode::SHR: {
            uint16_t val = state.R[rd];
            state.R[rd] = val >> 1;
            state.FLAGS &= ~(FLAG_ZERO | FLAG_CARRY | FLAG_NEGATIVE);
            if (state.R[rd] == 0) state.FLAGS |= FLAG_ZERO;
            if (state.R[rd] & 0x8000u) state.FLAGS |= FLAG_NEGATIVE;
            if (val & 1u) state.FLAGS |= FLAG_CARRY; // bit 0 was set, carried out
            if (tracing) std::cout << "  [EXEC] SHR R" << static_cast<unsigned>(rd) << "  ; R" << static_cast<unsigned>(rd)
                << " = 0x" << std::hex << std::setw(4) << std::setfill('0') << val << " >> 1 = 0x" << state.R[rd] << std::dec << "\n";
            break;
        }

        case Opcode::JMP: {
            state.PC = state.R[rs];
            if (tracing) std::cout << "  [EXEC] JMP R" << static_cast<unsigned>(rs) << "  ; PC = 0x" << std::hex << std::setw(4) << state.PC << std::dec << "\n";
            break;
        }

        case Opcode::JZ: {
            if (state.FLAGS & FLAG_ZERO) {
                state.PC = state.R[rs];
                if (tracing) std::cout << "  [EXEC] JZ R" << static_cast<unsigned>(rs) << "  ; Z=1, PC = 0x" << std::hex << std::setw(4) << state.PC << std::dec << "\n";
            } else {
                if (tracing) std::cout << "  [EXEC] JZ R" << static_cast<unsigned>(rs) << "  ; Z=0, no jump\n";
            }
            break;
        }

        case Opcode::NOP:
        default:
            if (tracing) std::cout << "  [EXEC] NOP\n";
            break;
    }
}

// =============================================================================
// RUN (until HALT)
// =============================================================================

size_t GPRCPU::run() {
    size_t cycles = 0;
    while (step())
        ++cycles;
    return cycles;
}
