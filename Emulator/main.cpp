#include <cstdio>
#include <iostream> //cout, etc.
#include <fstream> //std::ifstream
#include <algorithm> //max
#include <cstring> //memcpy
#include <cstdint>
#include <cassert>
#include <chrono> //For high resolution clock
using namespace std;

#include "TMS57070.h"
#include "TMS57070_MAC.h"

#include "wave/file.h" //https://github.com/audionamix/wave

//Mode 1 is used for my automatic emulation verification process.
//Mode 2 is the normal mode where there is an input WAV file and output WAV
#define MODE 2

constexpr uint32_t PMEM_MAX_WORDS = 0x1FF;
constexpr uint32_t CMEM_MAX_WORDS = 0x1FF;
constexpr uint32_t PMEM_INJECT_MAGIC = 0xFEEDBEE5; //used for my automatic emulation verification process.

TMS57070::Emulator dsp;

std::vector<float> outSamples;
std::vector<float> inSamples;

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

void dsp_sample_out(TMS57070::Channel channel, int32_t value) {
    //printf("Sample out. Channel: %d Value: %X\n", (int)channel, value);
    if (channel == TMS57070::Channel::out_1L) {
        outSamples.push_back((float)value / INT24_MAX);
    }
}

int32_t dsp_ext_io_in(uint32_t address) {
    return 0xFFFFFF;
}

void dsp_ext_io_out(int32_t value, uint32_t address) {
    printf("External IO output: %X address: %X\n", value, address);
}

int main(int argc, char* argv[]) {
    uint32_t inject_word = 0;
    uint32_t replacement_word = 0;
    uint32_t replacement_pos = UINT32_MAX;
    if (argc <= 1) {
        //Nothing
    } else if (argc == 2) {
        inject_word = strtoul(argv[1], nullptr, 16);
    } else if (argc == 4) {
        inject_word = strtoul(argv[1], nullptr, 16);
        replacement_word = strtoul(argv[2], nullptr, 16);
        replacement_pos = strtoul(argv[3], nullptr, 16);
    } else {
        printf("Wrong argument count\n");
        return 1;
    }

    dsp.reset();
    dsp.register_sample_out_callback(dsp_sample_out);
    dsp.register_external_bus_in_callback(dsp_ext_io_in);
    dsp.register_external_bus_out_callback(dsp_ext_io_out);

#if MODE == 2
    //Load dsp.PMEM
    ifstream PMEMFile("D:/Documents/OneDrive/Documents/Digitech XP/Emulator/PMEM_XP100.bin", std::ios::binary);
    ifstream CMEMFile("D:/Documents/OneDrive/Documents/Digitech XP/Emulator/CMEM_XP100_+1oct.bin", std::ios::binary);

    uint32_t pmem_length = ifstream_length(&PMEMFile);
    uint32_t cmem_length = ifstream_length(&CMEMFile);
    pmem_length = std::min(pmem_length / 4, PMEM_MAX_WORDS);
    cmem_length = std::min(cmem_length / 3, CMEM_MAX_WORDS);
    if (pmem_length == 0) {
        printf("Warning: PMEM input file is empty or nonexistent\n");
    }

    //Read PMEM intput file into the array
    uint8_t readBuffer[4];
    for (uint32_t i = 0; i < pmem_length; i++) {
        PMEMFile.read((char*)readBuffer, 4);
        uint32_t word = readBuffer[0] << 24 | readBuffer[1] << 16 | readBuffer[2] << 8 | readBuffer[3];
        if (word == PMEM_INJECT_MAGIC) {
            dsp.PMEM[i] = inject_word;
        } else if (i == replacement_pos) {
            dsp.PMEM[i] = replacement_word;
        } else {
            dsp.PMEM[i] = word;
        }
    }

    //Read CMEM intput file into the array
    for (uint32_t i = 0; i < cmem_length; i++) {
        CMEMFile.read((char*)readBuffer, 3);
        uint32_t word = readBuffer[0] << 16 | readBuffer[1] << 8 | readBuffer[2];
        dsp.CMEM[i].value = word;
    }

    wave::File read_file;
    wave::Error err = read_file.Open("input.wav", wave::kIn);
    if (err) {
        printf("Something went wrong in open\n");
        return 1;
    }
    err = read_file.Read(&inSamples);
    if (err) {
        printf("Something went wrong in read\n");
        return 2;
    }
    uint32_t sample_rate = read_file.sample_rate();
    printf("Read input WAV\n");

    dsp.step();
    dsp.step();
    dsp.step();
    for (uint32_t i = 0; i < inSamples.size(); i++) { //sample_rate * 10
        dsp.sample_in(TMS57070::Channel::in_1L, (int32_t)(inSamples[i] * 0x7FFFFF));
        dsp.sample_in(TMS57070::Channel::in_1R, 0x450000); //Digitech XP series pedal input
        //dsp.sample_in(TMS57070::Channel::in_1R, 0x150000 + ((uint64_t)0x300000 * i) / (uint64_t)(sample_rate * 10)); //Vary pedal input over 10 seconds
        for (uint32_t i = 0; i < 512; i++) {
            dsp.step();
        }

        if (i % sample_rate == 0) {
            printf("%d seconds\n", i/sample_rate);
        }

        //Optionally print out some Digitech XP series values
        //printf("0C %X 0F %X 10 %X 11 %X 12 %X \n", dsp.CMEM[0x0C].value, dsp.CMEM[0x0F].value, dsp.CMEM[0x10].value, dsp.CMEM[0x11].value, dsp.CMEM[0x12].value);

        //Optionally print out a dump of all registers, CMEM, and DMEM
        //std::string report = dsp.reportState();
        //std::cout << report << "\r";
    }

    wave::File write_file;
    err = write_file.Open("output.wav", wave::kOut);
    if (err) {
        std::cout << "Something went wrong in out open" << std::endl;
        return 3;
    }
    write_file.set_sample_rate(sample_rate);
    write_file.set_bits_per_sample(read_file.bits_per_sample());
    write_file.set_channel_number(1);

    err = write_file.Write(outSamples);
    if (err) {
        std::cout << "Something went wrong in write" << std::endl;
        return 4;
    }

    //Build state string
    std::string report = dsp.reportState();
    //std::cout << report;

    ofstream reportFile("report.txt", std::ios::binary);
    reportFile.write(report.c_str(), report.size());
    reportFile.close();
#else
    //Load dsp.PMEM
    ifstream PMEMFile("D:/Documents/OneDrive/Documents/Digitech XP/Emulator/PMEM.bin", std::ios::binary);
    ifstream CMEMFile("D:/Documents/OneDrive/Documents/Digitech XP/Emulator/CMEM.bin", std::ios::binary);

    uint32_t pmem_length = ifstream_length(&PMEMFile);
    uint32_t cmem_length = ifstream_length(&CMEMFile);
    pmem_length = std::min(pmem_length / 4, PMEM_MAX_WORDS);
    cmem_length = std::min(cmem_length / 3, CMEM_MAX_WORDS);

    uint8_t readBuffer[4];
    for (uint32_t i = 0; i < pmem_length; i++) {
        PMEMFile.read((char*)readBuffer, 4);
        uint32_t word = readBuffer[0] << 24 | readBuffer[1] << 16 | readBuffer[2] << 8 | readBuffer[3];
        if (word == PMEM_INJECT_MAGIC) {
            dsp.PMEM[i] = inject_word;
        } else if (i == replacement_pos) {
            dsp.PMEM[i] = replacement_word;
        } else {
            dsp.PMEM[i] = word;
        }
    }

    for (uint32_t i = 0; i < cmem_length; i++) {
        CMEMFile.read((char*)readBuffer, 3);
        uint32_t word = readBuffer[0] << 16 | readBuffer[1] << 8 | readBuffer[2];
        dsp.CMEM[i].value = word;
    }

    dsp.CR0.value = 0xAA9BAD;
    dsp.CR1.value = 0x890100;
    dsp.CR2.value = 0x30FF00;
    dsp.CR3.value = 0xE68000;

    dsp.sample_in(TMS57070::Channel::in_1L, 0);
    dsp.sample_in(TMS57070::Channel::in_1R, 0); //Triggers test

    for (uint32_t i = 0; i < 8; i++) //Scan past RESET
        dsp.step();

    while (dsp.PC.value != 0xD) { //Wait for program to be done
        dsp.step();

        //std::string report = dsp.reportState();
        //std::cout << report;
    }

    //Build state string
    std::string report = dsp.reportState();
    //std::cout << report;

    ofstream reportFile("report.txt", std::ios::binary);
    reportFile.write(report.c_str(), report.size());
    reportFile.close();

#endif
    return 0;
}
