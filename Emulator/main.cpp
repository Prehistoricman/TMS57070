#include <cstdio>
#include <iostream> //cout, etc.
#include <fstream> //std::ifstream
#include <algorithm> //max
#include <cstring> //memcpy
#include <cstdint>
#include <cassert>
using namespace std;

#include "TMS57070.h"

constexpr uint32_t PMEM_MAX_WORDS = 0x1FF;
constexpr uint32_t CMEM_MAX_WORDS = 0x0FF;

TMS57070::Emulator dsp;

static uint32_t ifstream_length(std::ifstream *stream) {
    streampos prev_pos = stream->tellg();
    if (prev_pos == -1) {
        return 0;
    }

    stream->seekg(0, stream->end);
    streampos length = stream->tellg();
    if (length == -1) {
        return 0;
    }

    stream->seekg(prev_pos);
    return (uint32_t)length;
}

int main() { //int argc, char* argv[]
    cout << "Hello World!\n";

    //Load dsp.PMEM
    ifstream PMEMFile("D:/Documents/OneDrive/Documents/Digitech XP/Emulator/PMEM.bin", std::ios::binary);
    ifstream CMEMFile("D:/Documents/OneDrive/Documents/Digitech XP/Emulator/CMEM.bin", std::ios::binary);

    uint32_t pmem_length = ifstream_length(&PMEMFile);
    uint32_t cmem_length = ifstream_length(&CMEMFile);
    printf("pmem_length (bytes) = %X\n", pmem_length);
    printf("cmem_length (bytes) = %X\n", cmem_length);
    pmem_length = std::min(pmem_length / 4, PMEM_MAX_WORDS);
    cmem_length = std::min(cmem_length / 3, CMEM_MAX_WORDS);
    printf("pmem_length (words) = %X\n", pmem_length);
    printf("cmem_length (words) = %X\n", cmem_length);

    uint8_t readBuffer[4];
    for (uint32_t i = 0; i < pmem_length; i++) {
        PMEMFile.read((char*)readBuffer, 4);
        uint32_t word = readBuffer[0] << 24 | readBuffer[1] << 16 | readBuffer[2] << 8 | readBuffer[3];
        dsp.PMEM[i] = word;
    }

    for (uint32_t i = 0; i < cmem_length; i++) {
        CMEMFile.read((char*)readBuffer, 3);
        uint32_t word = readBuffer[0] << 16 | readBuffer[1] << 8 | readBuffer[2];
        dsp.CMEM[i] = word;
        //printf("Loading CMEM with %X at %X\n", word, i);
    }

    dsp.reset();
    dsp.step();
    dsp.step();
    dsp.step();
    dsp.sample_in(TMS57070::Channel::in_1L, 0x696969);
    for (uint16_t i = 0; i < 60; i++) { //TODO: input and output
        dsp.step();
    }
    return 0;
}
