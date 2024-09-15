#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>

#include "main.h"

#include "dynarmic/interface/A32/a32.h"
#include "dynarmic/interface/A32/config.h"

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

class MD380Environment final : public Dynarmic::A32::UserCallbacks {
public:
    u64 ticks_left = 0;
    Dynarmic::A32::Jit* cpu;
    uint8_t* firmware{}; // At 0x0800C000
    std::array<u8, 0x20000> sram{}; // At 0x20000000
    std::array<u8, 0x20000> tcram{}; // At 0x10000000
    std::array<u8, 0x10000> stack{}; // At 0x21000000

    u8 MemoryRead8(u32 vaddr) override {
        if (vaddr >= 0x0800C000 && vaddr < 0x0800C000 + 0x100000) {
            return firmware[vaddr - 0x0800C000];
        }
        if (vaddr >= 0x20000000 && vaddr < 0x20000000 + sram.size()) {
            return sram[vaddr - 0x20000000];
        }
        if (vaddr >= 0x10000000 && vaddr < 0x10000000 + tcram.size()) {
            return tcram[vaddr - 0x10000000];
        }
        if (vaddr >= 0x21000000 && vaddr < 0x21000000 + stack.size()) {
            return stack[vaddr - 0x21000000];
        }
        // #udf 0 to halt
        if (vaddr == 0x30000000) return 0xde;
        if (vaddr == 0x30000001) return 0x00;
        return 0;
    }

    u16 MemoryRead16(u32 vaddr) override {
        return u16(MemoryRead8(vaddr)) | u16(MemoryRead8(vaddr + 1)) << 8;
    }

    u32 MemoryRead32(u32 vaddr) override {
        return u32(MemoryRead16(vaddr)) | u32(MemoryRead16(vaddr + 2)) << 16;
    }

    u64 MemoryRead64(u32 vaddr) override {
        return u64(MemoryRead32(vaddr)) | u64(MemoryRead32(vaddr + 4)) << 32;
    }

    void MemoryWrite8(u32 vaddr, u8 value) override {
        if (vaddr >= 0x0800C000 && vaddr < 0x0800C000 + 0x100000) {
            firmware[vaddr - 0x0800C000] = vaddr;
            return;
        }
        if (vaddr >= 0x20000000 && vaddr < 0x20000000 + sram.size()) {
            sram[vaddr - 0x20000000] = value;
            return;
        }
        if (vaddr >= 0x10000000 && vaddr < 0x10000000 + tcram.size()) {
            tcram[vaddr - 0x10000000] = value;
            return;
        }
        if (vaddr >= 0x21000000 && vaddr < 0x21000000 + stack.size()) {
            stack[vaddr - 0x21000000] = value;
            return;
        }
    }

    void MemoryWrite16(u32 vaddr, u16 value) override {
        MemoryWrite8(vaddr, u8(value));
        MemoryWrite8(vaddr + 1, u8(value >> 8));
    }

    void MemoryWrite32(u32 vaddr, u32 value) override {
        MemoryWrite16(vaddr, u16(value));
        MemoryWrite16(vaddr + 2, u16(value >> 16));
    }

    void MemoryWrite64(u32 vaddr, u64 value) override {
        MemoryWrite32(vaddr, u32(value));
        MemoryWrite32(vaddr + 4, u32(value >> 32));
    }

    void InterpreterFallback(u32 pc, size_t num_instructions) override {
        // This is never called in practice.
        std::terminate();
    }

    void CallSVC(u32 swi) override {
    }

    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override {
        cpu->HaltExecution();
    }

    void AddTicks(u64 ticks) override {
    }

    u64 GetTicksRemaining() override {
        return ticks_left;
    }
};

class MD380Emulator {
    private:
        MD380Environment env;
        Dynarmic::A32::UserConfig user_config;
        Dynarmic::A32::Jit cpu;
    public:
        MD380Emulator(uint8_t* firmware, uint8_t* sram) :
            env{}, user_config{.callbacks = &env}, cpu{user_config} {
            env.cpu = &cpu;
            env.firmware = firmware;
            std::copy(sram, sram + 0x20000, env.sram.begin());
        }

        void AmbeUnpackFrame(uint8_t* ambeFrame) {
            int ambei = 0;
            short* ambe = (short*) &env.sram[ambe_inbuffer - 0x20000000];
            for(int i = 1;i < 7;i++) { // Skip first byte.
                for(int j = 0;j < 8;j++) {
                    ambe[ambei++] = (ambeFrame[i]>>(7-j))&1; // MSBit first
                }
            }
            ambe[ambei++]=ambeFrame[7]&1;//Final bit in its own frame as LSBit.
        }
        void AmbeDecodeExecute(uint32_t outbuf_addr, int slot) {
            cpu.Regs()[0] = outbuf_addr;
            cpu.Regs()[1] = 80;
            cpu.Regs()[2] = ambe_inbuffer;
            cpu.Regs()[3] = 0;

            // Rest of the arguments on the stack
            uint32_t sp = 0x21000000 + env.stack.size() - 0x1000;
            sp -= 4;
            env.MemoryWrite32(sp, ambe_mystery);
            sp -= 4;
            env.MemoryWrite32(sp, slot);
            sp -= 4;
            env.MemoryWrite32(sp, 0);

            cpu.Regs()[13] = sp; // SP
            cpu.Regs()[14] = 0x30000000; // LR
            cpu.Regs()[15] = ambe_decode_wav; // PC
            cpu.SetCpsr(0x00000030); // Thumb

            env.ticks_left = 1000000000;
            cpu.Run();
        }
        void AmbeDecodeFrame(uint8_t* ambeFrame, int16_t* audioOutput) {
            AmbeUnpackFrame(ambeFrame);
            AmbeDecodeExecute(ambe_outbuffer0, 0);
            int16_t* outbuf0 = (int16_t*) &env.sram[ambe_outbuffer0 - 0x20000000];
            std::copy(outbuf0, outbuf0 + 80, audioOutput);
            AmbeDecodeExecute(ambe_outbuffer1, 1);
            int16_t* outbuf1 = (int16_t*) &env.sram[ambe_outbuffer1 - 0x20000000];
            std::copy(outbuf1, outbuf1 + 80, audioOutput + 80);
        }
        void AmbePackFrame(int16_t* ambeUnpacked, uint8_t* ambeFrame) {
            ambeFrame[0] = 0;
            ambeFrame[1] = ambeUnpacked[7] | ambeUnpacked[6] << 1 | ambeUnpacked[5] << 2 | ambeUnpacked[4] << 3 | ambeUnpacked[3] << 4 | ambeUnpacked[2] << 5 | ambeUnpacked[1] << 6 | ambeUnpacked[0] << 7;
            ambeFrame[2] = ambeUnpacked[15] | ambeUnpacked[14] << 1 | ambeUnpacked[13] << 2 | ambeUnpacked[12] << 3 | ambeUnpacked[11] << 4 | ambeUnpacked[10] << 5 | ambeUnpacked[9] << 6 | ambeUnpacked[8] << 7;
            ambeFrame[3] = ambeUnpacked[23] | ambeUnpacked[22] << 1 | ambeUnpacked[21] << 2 | ambeUnpacked[20] << 3 | ambeUnpacked[19] << 4 | ambeUnpacked[18] << 5 | ambeUnpacked[17] << 6 | ambeUnpacked[16] << 7;
            ambeFrame[4] = ambeUnpacked[31] | ambeUnpacked[30] << 1 | ambeUnpacked[29] << 2 | ambeUnpacked[28] << 3 | ambeUnpacked[27] << 4 | ambeUnpacked[26] << 5 | ambeUnpacked[25] << 6 | ambeUnpacked[24] << 7;
            ambeFrame[5] = ambeUnpacked[39] | ambeUnpacked[38] << 1 | ambeUnpacked[37] << 2 | ambeUnpacked[36] << 3 | ambeUnpacked[35] << 4 | ambeUnpacked[34] << 5 | ambeUnpacked[33] << 6 | ambeUnpacked[32] << 7;
            ambeFrame[6] = ambeUnpacked[47] | ambeUnpacked[46] << 1 | ambeUnpacked[45] << 2 | ambeUnpacked[44] << 3 | ambeUnpacked[43] << 4 | ambeUnpacked[42] << 5 | ambeUnpacked[41] << 6 | ambeUnpacked[40] << 7;
            ambeFrame[7] = ambeUnpacked[48];
        }
        void AmbeEncodeExecute(uint32_t inbuf_addr, int slot) {
            cpu.Regs()[0] = ambe_outbuffer;
            cpu.Regs()[1] = 0;
            cpu.Regs()[2] = inbuf_addr;
            cpu.Regs()[3] = 0x50;

            // Rest of the arguments on the stack
            uint32_t sp = 0x21000000 + env.stack.size() - 0x1000;
            sp -= 4;
            env.MemoryWrite32(sp, ambe_en_mystery);
            sp -= 4;
            env.MemoryWrite32(sp, 0x2000);
            sp -= 4;
            env.MemoryWrite32(sp, slot);
            sp -= 4;
            env.MemoryWrite32(sp, 0x1840);

            cpu.Regs()[13] = sp; // SP
            cpu.Regs()[14] = 0x30000000; // LR
            cpu.Regs()[15] = ambe_encode_thing; // PC
            cpu.SetCpsr(0x00000030); // Thumb

            env.ticks_left = 1000000000;
            cpu.Run();
        }
        void AmbeEncodeFrame(int16_t* audioInput, uint8_t* ambeFrame) {
            int16_t* inbuf0 = (int16_t*) &env.sram[wav_inbuffer0 - 0x20000000];
            int16_t* inbuf1 = (int16_t*) &env.sram[wav_inbuffer1 - 0x20000000];
            std::copy(audioInput, audioInput + 80, inbuf0);
            std::copy(audioInput + 80, audioInput + 160, inbuf1);
            int16_t* ambe = (int16_t*) &env.sram[ambe_outbuffer - 0x20000000];
            std::fill(ambe, ambe + 49, 0);
            AmbeEncodeExecute(wav_inbuffer0, 0);
            AmbeEncodeExecute(wav_inbuffer1, 1);
            AmbePackFrame(ambe, ambeFrame);
        }
};


void decodeAmbeFile(MD380Emulator& emulator, char* infile, char* outfile) {

    std::ifstream ambefile(infile, std::ios::binary);
    std::ofstream wavfile(outfile, std::ios::binary);

    uint8_t header[4];
    ambefile.read((char*) header, 4);
    if (std::memcmp(header, ".amb", 4)) {
        throw std::runtime_error("Incorrect magic of " + std::string(infile));
    }

    uint8_t frame[8];
    int16_t outbuf[160];
    int frame_num = 0;
    while (ambefile.read((char*) frame, 8)) {
        if (frame[0] != 0) {
            std::cerr << "Warning: Frame " << frame_num << " has a bad status." << std::endl;
        }

        emulator.AmbeDecodeFrame(frame, outbuf);
        wavfile.write((char*) outbuf, 160 * 2);
        frame_num++;
    }
    std::cerr << "AMBE Decoded " << frame_num << " frames." << std::endl;
}

void encodeAmbeFile(MD380Emulator& emulator, char* infile, char* outfile) {

    std::ifstream wavfile(infile, std::ios::binary);
    std::ofstream ambefile(outfile, std::ios::binary);

    ambefile.write(".amb", 4);

    int16_t inbuf[160];
    uint8_t frame[8];
    int frame_num = 0;
    while (wavfile.read((char*) inbuf, 160 * 2)) {
        emulator.AmbeEncodeFrame(inbuf, frame);
        ambefile.write((char*) frame, 8);
        frame_num++;
    }
    std::cerr << "AMBE Encoded " << frame_num << " frames." << std::endl;
}

void decodeBenchmark(MD380Emulator& emulator) {
    int16_t audio[160];
    uint8_t frame[8];
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; i++) {
        emulator.AmbeDecodeFrame(frame, audio);
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cerr << "Decoded 1000 frames in " << elapsed << "ms. " << (160 * 1000 / 8) / elapsed << "x faster than realtime" << std::endl;
}

void encodeBenchmark(MD380Emulator& emulator) {
    int16_t audio[160];
    uint8_t frame[8];
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; i++) {
        emulator.AmbeEncodeFrame(audio, frame);
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cerr << "Encoded 1000 frames in " << elapsed << "ms. " << (160 * 1000 / 8) / elapsed << "x faster than realtime" << std::endl;
}

void usage() {
}

int main(int argc, char** argv) {
  MD380Emulator emulator{firmware, sram};
  std::string command = (argc > 1) ? argv[1] : "encode";

  if ( (((command == "decode") || (command == "encode")) && argc != 4) ||
       (((command == "decode_bench") || (command == "encode_bench")) && argc != 2) ) {
    std::cerr << "Usage: \n" ;
    std::cerr << "       " << argv[0] << " <decode|encode> <infile> <outfile>\n";
    std::cerr << "       " << argv[0] << " <decode_bench|encode_bench>" << std::endl;
    return 1;
  }

  if (command == "decode") {
    decodeAmbeFile(emulator, argv[2], argv[3]);
  } else if (command == "encode") {
    encodeAmbeFile(emulator, argv[2], argv[3]);
  } else if (command == "decode_bench") {
    decodeBenchmark(emulator);
  } else if (command == "encode_bench") {
    encodeBenchmark(emulator);
  } else {
    std::cerr << "Unknown command: " << argv[1] << std::endl;
    return 1;
  }
}
