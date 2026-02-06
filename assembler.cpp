/**
 * Simple assembler for 16-bit GPR CPU.
 */

#include "assembler.h"
#include <sstream>
#include <map>
#include <fstream>

// NOTE: not bad but good use case for string_view which is even cheaper then const string&
//     also prefer to use a switch statement here but does not really matter
static int getOpcode(const std::string& mnem) {
    if (mnem == "HALT") return 0;
    if (mnem == "MOVI") return 1;
    if (mnem == "MOV")  return 2;
    if (mnem == "LOAD") return 3;
    if (mnem == "STORE") return 4;
    if (mnem == "ADD")  return 5;
    if (mnem == "SUB")  return 6;
    if (mnem == "AND")  return 7;
    if (mnem == "OR")   return 8;
    if (mnem == "XOR")  return 9;
    if (mnem == "NOT")  return 10;
    if (mnem == "SHL")  return 11;
    if (mnem == "SHR")  return 12;
    if (mnem == "JMP")  return 13;
    if (mnem == "JZ")   return 14;
    if (mnem == "NOP")  return 15;
    return -1;
}

// NOTE: not needed std::string provies API's to handle this
static char toUpperChar(char c) {
    if (c >= 'a' && c <= 'z') return static_cast<char>(c - 32);
    return c;
}

static std::string toUpper(std::string s) {
    // NOTE: not needed you cna just call s.method() I forogt the method but there is one for that
    for (char& c : s) c = toUpperChar(c);
    return s;
}

static uint16_t parseNumber(const std::string& s) {
    size_t idx = 0;
    unsigned long v = std::stoul(s, &idx, 0);
    return static_cast<uint16_t>(v & 0xFFFFu);
}

static bool parseReg(const std::string& s, uint8_t& r) {
    std::string t = s;
    while (t.size() >= 2 && t[0] == '(' && t.back() == ')')
        t = t.substr(1, t.size() - 2);  // (R0) -> R0
    if (t.size() < 2 || (t[0] != 'R' && t[0] != 'r')) return false;
    int n = std::stoi(t.substr(1), nullptr, 10);
    if (n < 0 || n > 7) return false;
    r = static_cast<uint8_t>(n);
    return true;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string stripComment(const std::string& line) {
    size_t p = line.find(';');
    if (p != std::string::npos)
        return trim(line.substr(0, p));
    return trim(line);
}

static void tokenize(const std::string& line, std::vector<std::string>& tokens) {
    tokens.clear();
    std::string cur;
    for (size_t i = 0; i <= line.size(); ++i) {
        char c = (i < line.size()) ? line[i] : ' ';
        if ((c == ' ' || c == '\t' || c == '\r' || c == '\n') || c == ',') {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
}

static uint16_t encMOVI(uint8_t rd, uint16_t imm9) {
    return (1u << 12) | ((rd & 7u) << 9) | (imm9 & 0x1FFu);
}
static uint16_t encRR(uint8_t op, uint8_t rd, uint8_t rs) {
    return ((op & 15u) << 12) | ((rd & 7u) << 9) | ((rs & 7u) << 6);
}

// NOTE: again does not need to be on the heap it is not that large
AssembleResult assemble(const std::string& source, uint16_t* mem, size_t memSize) {
    AssembleResult res{true, "", 0};
    std::map<std::string, uint16_t> labels;

    // First pass: collect labels and compute instruction addresses
    std::istringstream iss1(source);
    std::string line;
    size_t lineNum = 0;
    uint16_t pc = 0;

    while (std::getline(iss1, line)) {
        ++lineNum;
        // NOTE: you are jumping to stripComment everytime getline gets called?
        // Maybe we can check for the comment first instead of jumping to a new branch everytime
        // Ahhh I see now so we can do like if strip(comment) alkso instead of returning an empty
        // string instead opt to return just true or false
        std::string rest = stripComment(line);
        if (rest.empty()) continue;

        if (rest.back() == ':') {
            std::string name = trim(rest.substr(0, rest.size() - 1));
            // NOTE: again use the string internal api
            if (!name.empty()) labels[toUpper(name)] = pc;
            continue;
        }

        std::vector<std::string> tok;
        tokenize(rest, tok);
        if (tok.empty()) continue;

        std::string cmd = toUpper(tok[0]);

        if (cmd == ".ORG") {
            if (tok.size() < 2) {
                // NOTE: seprate lines here
                res.ok = false; res.error = ".ORG requires address"; res.lineNum = lineNum;
                return res;
            }
            pc = parseNumber(tok[1]);
            continue;
        }
        if (cmd == ".WORD") {
            if (tok.size() == 1) {
                res.ok = false; res.error = ".WORD requires value"; res.lineNum = lineNum;
                return res;
            }
            if (tok.size() == 2) {
                pc++;  // .WORD value at current pc
            }
            continue;
        }

        int op = getOpcode(cmd);
        if (op >= 0) {
            pc++;
            continue;
        }
        res.ok = false; res.error = "Unknown: " + cmd; res.lineNum = lineNum;
        return res;
    }

    // Second pass: emit
    pc = 0;
    iss1.clear();
    iss1.str(source);
    lineNum = 0;

    while (std::getline(iss1, line)) {
        ++lineNum;
        std::string rest = stripComment(line);
        if (rest.empty()) continue;

        if (rest.back() == ':') continue;

        std::vector<std::string> tok;
        tokenize(rest, tok);
        if (tok.empty()) continue;

        std::string cmd = toUpper(tok[0]);

        if (cmd == ".ORG") {
            if (tok.size() >= 2) pc = parseNumber(tok[1]);
            continue;
        }
        if (cmd == ".WORD") {
            if (tok.size() >= 2) {
                uint16_t val = parseNumber(tok[1]);
                if (tok.size() >= 3) {
                    uint16_t addr = parseNumber(tok[1]);
                    val = parseNumber(tok[2]);
                    if (addr < memSize) mem[addr] = val;
                } else {
                    if (pc < memSize) mem[pc] = val;
                    pc++;
                }
            }
            continue;
        }

        int op = getOpcode(cmd);
        if (op < 0) {
            res.ok = false; res.error = "Unknown: " + cmd; res.lineNum = lineNum;
            return res;
        }

        if (pc >= memSize) {
            res.ok = false; res.error = "Program too large"; res.lineNum = lineNum;
            return res;
        }

        uint16_t inst = 0;

        switch (op) {
            case 0: inst = 0x0000; break;
            case 1: {
                if (tok.size() < 3) {
                    res.ok = false; res.error = "MOVI Rd, imm"; res.lineNum = lineNum;
                    return res;
                }
                uint8_t rd;
                if (!parseReg(tok[1], rd)) {
                    res.ok = false; res.error = "Invalid register"; res.lineNum = lineNum;
                    return res;
                }
                uint16_t imm;
                std::string immStr = tok[2];
                if (labels.count(toUpper(immStr)))
                    imm = labels[toUpper(immStr)];
                else
                    imm = parseNumber(immStr);
                imm &= 0x1FF;
                inst = encMOVI(rd, imm);
                break;
            }
            case 13: case 14: {  // JMP, JZ - accept label or register
                if (tok.size() < 2) {
                    res.ok = false; res.error = "JMP/JZ needs target"; res.lineNum = lineNum;
                    return res;
                }
                std::string arg = tok[1];
                uint8_t rs;
                if (parseReg(arg, rs)) {
                    inst = encRR(static_cast<uint8_t>(op), 0, rs);  // Rd unused
                } else {
                    uint16_t target = labels.count(toUpper(arg)) ? labels[toUpper(arg)] : parseNumber(arg);
                    if (target > 0x1FF) {
                        res.ok = false;
                        res.error = "Jump target > 511 (MOVI 9-bit limit); use register";
                        res.lineNum = lineNum;
                        return res;
                    }
                    mem[pc++] = encMOVI(7, target);       // MOVI R7, target
                    inst = encRR(static_cast<uint8_t>(op), 0, 7);   // JMP/JZ R7
                }
                break;
            }
            // NOTE: instead of saying the number I am sure you can use the enumclass a bit more readable
            case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
            case 10: case 11: case 12: {
                if (tok.size() < 2) {
                    res.ok = false; res.error = "Needs operands"; res.lineNum = lineNum;
                    return res;
                }
                uint8_t rd, rs = 0;
                if (!parseReg(tok[1], rd)) {
                    res.ok = false; res.error = "Invalid Rd"; res.lineNum = lineNum;
                    return res;
                }
                if (op == 10 || op == 11 || op == 12) {
                    rs = rd;
                } else if (tok.size() >= 3) {
                    std::string arg = tok[2];
                    if (parseReg(arg, rs)) { /* ok */ }
                    else {
                        uint16_t val = labels.count(toUpper(arg)) ? labels[toUpper(arg)] : parseNumber(arg);
                        rs = static_cast<uint8_t>(val & 7);
                    }
                }
                inst = encRR(static_cast<uint8_t>(op), rd, rs);
                break;
            }
            case 15: inst = 0xF000; break;
            default: break;
        }

        mem[pc++] = inst;
    }
    return res;
}

AssembleResult assembleFile(const char* path, uint16_t* mem, size_t memSize) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return AssembleResult{false, "Cannot open file", 0};
    std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    return assemble(source, mem, memSize);
}
