# TMS57070
Tools &amp; docs for the TI DSP TMS57070 including IDA disassembler

## Documentation
See the [wiki tab](https://github.com/Prehistoricman/TMS57070/wiki)


## IDA processor module
**TMS57070_disasm.py**
  * Install in IDA_dir/procs/
  * Developed for IDA 7.0/7.2
  * Not complete - currently missing:
    * Some addressing modes
    * Some opcodes (to be implemented)
    * Some opcodes (to be discovered)
  * Issues:
    * Data references are basically broken. They don't point to the right place in memory (PMEM rather than CMEM or DMEM) and don't track read/write properly
    * CMEM loading from file is partially broken. IDA doesn't fully understand the idea that this segment has a different word size (3 bytes vs 4 bytes for the code section)
    * If a loop is broken (by undefining code), the loop instruction (or last instruction of the loop) may need to be re-analysed to restore the loop
    * Some indirect address incrementing modes cannot be displayed (such as incrementing the DMEM pointer when the instruction doesn't read/write to DMEM)
    * Auto-comments spam the output window with errors. But hey, at least you get auto-comments (no thanks to IDA's poor documentation)
    * Reloading the input file is broken. IDA ruins the database in the process, but doesn't actually load in the new data.

## Emulator
* Developed in Visual Studio 2019 as a console project
* Pretty good accuracy and support
* Tested against the real hardware in an automated process
* Main omissions:
  * Some XMEM functionality
  * Some flag functionality
  * MACC accuracy with large numbers
  * Good command line interface
  * A readme!
* WAV file input and output courtesy of https://github.com/audionamix/wave
  * MIT license in the main file "wave/file.h"

## Sniffer
Arduino project for sniffing host interface traffic, for the purpose of dumping the PMEM and CMEM. For more information, [see the readme](Sniffer/readme.md)

## TMS57070_Uploader
Arduino project for TMS57070 interfacing and Qt project for TMS57070 control via the PC. Features:
* PMEM and CMEM live upload
* CMEM live view

For more information, [see the readme](TMS57070_Uploader/readme.md)
