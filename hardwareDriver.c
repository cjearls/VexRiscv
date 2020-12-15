#include <stdio.h>
#include "src/main/c/emulator/src/riscv.h"
#include <stdbool.h>

#define CHARACTERS 4096
#define CHARACTER_BITS 8
#define DEBUG true

struct compressorOutputs
{
	bool inReady;
	bool outValid;
	size_t outBits;
};

struct decompressorOutputs
{
	bool inReady;
	bool outValid;
	size_t outBits;
	size_t dataOutLength;
};

static inline void writeCompressorInputs(bool stop, bool inValid, bool outReady, size_t inBits)
{
	csr_write(0x8FC, stop | (inValid << 1) | (outReady << 2) | (inBits << 3));
}

static inline void writeDecompressorInputs(bool inValid, bool outReady, size_t inBits)
{
	csr_write(0x8FD, inValid | (outReady << 1) | (inBits << 2));
}

static inline struct compressorOutputs readCompressorOutputs()
{
	struct compressorOutputs outputs;
	size_t rawData = csr_read(0xCFE);
	outputs.inReady = 1 & rawData;
	outputs.outValid = 1 & (rawData >> 1);
	outputs.outBits = rawData >> 2;
	return outputs;
}

static inline struct decompressorOutputs readDecompressorOutputs()
{
	struct decompressorOutputs outputs;
	size_t rawData = csr_read(0xCFF);
	outputs.inReady = 1 & rawData;
	outputs.outValid = 1 & (rawData >> 1);
	outputs.outBits = ((1 << 16) - 1) & (rawData >> 2);
	outputs.dataOutLength = rawData >> 18;
	return outputs;
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

	// reading outputs from compressor and decompressor
	printf("compressor: inready=%d, outvalid=%d, outbits=%d\ndecompressor: inready=%d, outvalid=%d, outbits=%d, dataoutlength=%d\n",
		   readCompressorOutputs().inReady,
		   readCompressorOutputs().outValid,
		   readCompressorOutputs().outBits,
		   readDecompressorOutputs().inReady,
		   readDecompressorOutputs().outValid,
		   readDecompressorOutputs().outBits,
		   readDecompressorOutputs().dataOutLength);


	FILE *filePointer;
	filePointer = fopen("lzTestFile.txt", "rb");
	if (filePointer == NULL)
	{
		printf("Error, filepointer is null\n");
		return -1;
	}

	char inCharacterArray[CHARACTERS];
	char outCharacterArray[CHARACTERS];
	size_t readBytes = fread(inCharacterArray, 1, CHARACTERS, filePointer);
	while (readBytes == CHARACTERS)
	{
		// This sets the output array to all incorrect values so it will be obvious if a value isn't written or is written
		// wrong later when the check is done.
		for (size_t index = 0; index < CHARACTERS; index++)
		{
			outCharacterArray[index] = 7;
		}

		printf("compressor reset = %d\n", resetCompressor());
		printf("decompressor reset = %d\n", resetDecompressor());

		// This is used to iterate through all the input characters and put them into the compressor.
		size_t currentInCharacterIndex = 0;
		// This is used to iterate through all the output characters and put them into the output array.
		size_t currentOutCharacterIndex = 0;

		writeCycles(0);
		writeInstructions(0);
		size_t compressorCycleLatency = readCycles();
		size_t compressorInstructionLatency = readInstructions();

		while (currentOutCharacterIndex < CHARACTERS)
		{
			struct compressorOutputs compOut = readCompressorOutputs();
			struct decompressorOutputs decompOut = readDecompressorOutputs();

			// Feed in the next character to the compressor input
			if (compOut.inReady && currentInCharacterIndex < CHARACTERS)
			{
#if DEBUG
				printf("Inputting %d to compressor\n", inCharacterArray[currentInCharacterIndex]);
#endif
				writeCompressorInputs(false, true, false, inCharacterArray[currentInCharacterIndex]);
				currentInCharacterIndex++;
			}
			else if (compOut.outValid && decompOut.inReady)
			{
#if DEBUG
				printf("compressor out and decompressor input is %d\n", compOut.outBits);
#endif
				writeCompressorInputs(false, false, true, 0);
				writeDecompressorInputs(true, false, compOut.outBits);
			}
			else if (currentInCharacterIndex >= CHARACTERS && !compOut.outValid && decompOut.inReady)
			{
				// This moves the compressor into the "stopSignalReceived" state where it outputs any remaining characters
				// in its buffer.
				writeCompressorInputs(true, false, false, 0);
			}
			else if (decompOut.outValid)
			{
#if DEBUG
				printf("decompressor output is valid, outputting bits %d of dataOutLength %d\n", decompOut.outBits, decompOut.dataOutLength);
#endif
				writeDecompressorInputs(false, true, 0);
				for (size_t index = 0; index < decompOut.dataOutLength; index++)
				{
					outCharacterArray[currentOutCharacterIndex] = decompOut.outBits >> (CHARACTER_BITS * (decompOut.dataOutLength - 1 - index));
					currentOutCharacterIndex++;
				}
			}
		}

		compressorCycleLatency = readCycles() - compressorCycleLatency;
		compressorInstructionLatency = readInstructions() - compressorInstructionLatency;

		printf("compressor cycle latency was %d, and instruction latency was %d\n", compressorCycleLatency, compressorInstructionLatency);

		// This checks if the input equals the output, and prints if they are unequal.
		for (size_t index = 0; index < CHARACTERS; index++)
		{
			if (inCharacterArray[index] != outCharacterArray[index])
			{
				printf("Array index %d does not match: in=%d, out=%d\n", index, inCharacterArray[index], outCharacterArray[index]);
			}
		}

		size_t readBytes = fread(inCharacterArray, 1, CHARACTERS, filePointer);
	}

	fclose(filePointer);

	return 0;
}
