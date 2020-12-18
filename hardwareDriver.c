#include <stdio.h>
#include "src/main/c/emulator/src/riscv.h"
#include <stdbool.h>

#define CHARACTERS 4096
#define CHARACTER_BITS 8
#define DEBUG true

static inline void writeCompressorInputs(size_t inBits)
{
	csr_write(0x8FC, inBits);
}

static inline void writeDecompressorInputs(size_t inBits)
{
	csr_write(0x8FD, inBits);
}

static inline size_t readCompressorOutputs()
{
	return csr_read(0xCFE);
}

static inline size_t readDecompressorOutputs()
{
	return csr_read(0xCFF);
}

static inline size_t resetCompressor()
{
	// The hardware is configured such that reading from this csr activates the reset signal.
	return csr_read(0xCED);
}

static inline size_t resetDecompressor()
{
	// The hardware is configured such that reading from this csr activates the reset signal.
	return csr_read(0xCEE);
}

static inline size_t readCycles()
{
	return csr_read(0x8FF);
}

static inline size_t readInstructions()
{
	return csr_read(0x8FE);
}

static inline void writeCycles(size_t newValue)
{
	return csr_write(0x8FF, newValue);
}

static inline void writeInstructions(size_t newValue)
{
	return csr_write(0x8FE, newValue);
}

int main()
{
	// This reads the instruction count register.
	printf("Hello World!\n");
	printf("The current cycle count is %d, and the current instruction count is %d\n", readCycles(), readInstructions());
	writeCycles(0);
	int cycle1 = readCycles();
	int cycle2 = readCycles();
	writeInstructions(0);
	int instruction1 = readInstructions();
	int instruction2 = readInstructions();
	printf("Cycle1: %d\nCycle2: %d\nInstruction1: %d\nInstruction2: %d\n Cycle difference: %d, Instruction difference: %d\n", cycle1, cycle2, instruction1, instruction2, cycle2 - cycle1, instruction2 - instruction1);

	FILE *filePointer;
	filePointer = fopen("lzTestFile.txt", "rb");
	if (filePointer == NULL)
	{
		printf("Error, filepointer is null\n");
		return -1;
	}

	char inCharacterArray[CHARACTERS];
	size_t intermediateCharacterArray[CHARACTERS];
	char outCharacterArray[CHARACTERS];
	size_t readBytes = fread(inCharacterArray, 1, CHARACTERS, filePointer);
	/*while (readBytes == CHARACTERS)
	{*/
		// This sets the output array to all incorrect values so it will be obvious if a value isn't written or is written
		// wrong later when the check is done.
		for (size_t index = 0; index < CHARACTERS; index++)
		{
			outCharacterArray[index] = 7;
		}

		printf("compressor reset = %d\n", resetCompressor());
		printf("decompressor reset = %d\n", resetDecompressor());

		writeCycles(0);
		writeInstructions(0);
		size_t compressorCycleLatency = readCycles();
		size_t compressorInstructionLatency = readInstructions();

		for(size_t index = 0; index < CHARACTERS; index++){
			writeCompressorInputs(inCharacterArray[index]);
		}

		for(size_t index = 0; index < CHARACTERS; index++){
			intermediateCharacterArray[index] = readCompressorOutputs();
		}

		compressorCycleLatency = readCycles() - compressorCycleLatency;
		compressorInstructionLatency = readInstructions() - compressorInstructionLatency;

		printf("compressor cycle latency was %d, and instruction latency was %d\n", compressorCycleLatency, compressorInstructionLatency);

		writeCycles(0);
		writeInstructions(0);
		size_t decompressorCycleLatency = readCycles();
		size_t decompressorInstructionLatency = readInstructions();

		for(size_t index = 0; index < CHARACTERS; index++){
			writeDecompressorInputs(intermediateCharacterArray[index]);
		}

		for(size_t index = 0; index < CHARACTERS; index++){
			outCharacterArray[index] = readDecompressorOutputs();
		}

		decompressorCycleLatency = readCycles() - decompressorCycleLatency;
		decompressorInstructionLatency = readInstructions() - decompressorInstructionLatency;

		printf("decompressor cycle latency was %d, and instruction latency was %d\n", decompressorCycleLatency, decompressorInstructionLatency);

		// This checks if the input equals the output, and prints if they are unequal.
		for (size_t index = 0; index < CHARACTERS; index++)
		{
			if (inCharacterArray[index] != outCharacterArray[index])
			{
				printf("Array index %d does not match: in=%d, out=%d\n", index, inCharacterArray[index], outCharacterArray[index]);
			}
		}
/*
		size_t readBytes = fread(inCharacterArray, 1, CHARACTERS, filePointer);
	}*/

	fclose(filePointer);

	return 0;
}
