#ifndef MAIN_H
#define MAIN_H

#include <cstdint>

extern unsigned char firmware[];
extern unsigned int firmware_len;

extern unsigned char sram[];
extern unsigned int sram_len;

const uint32_t ambe_decode_wav = 0x08051248;
const uint32_t ambe_inbuffer = 0x20011c8e;
const uint32_t ambe_outbuffer0 = 0x20011aa8;
const uint32_t ambe_outbuffer1 = 0x20011b48;
const uint32_t ambe_mystery = 0x20011224;

const uint32_t ambe_encode_thing = 0x08050d90;
const uint32_t wav_inbuffer0 = 0x2000de82;
const uint32_t wav_inbuffer1 = 0x2000df22;
const uint32_t ambe_outbuffer = 0x2000dfc6;
const uint32_t ambe_en_mystery = 0x2000c730;



#endif