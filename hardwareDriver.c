#include <stdio.h>
#include "src/main/c/emulator/src/riscv.h"
#include <stdbool.h>

#define CHARACTERS 4096
#define CHARACTER_BITS 8

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

void writeCompressorInputs(bool stop, bool inValid, bool outReady, size_t inBits)
{
	csr_write(0x8FC, stop | (inValid << 1) | (outReady << 2) | (inBits << 3));
}

void writeDecompressorInputs(bool inValid, bool outReady, size_t inBits)
{
	csr_write(0x8FD, inValid | (outReady << 1) | (inBits << 2));
}

struct compressorOutputs readCompressorOutputs()
{
	struct compressorOutputs outputs;
	size_t rawData = csr_read(0xCFE);
	outputs.inReady = 1 & rawData;
	outputs.outValid = 1 & (rawData >> 1);
	outputs.outBits = rawData >> 2;
	return outputs;
}

struct decompressorOutputs readDecompressorOutputs()
{
	struct decompressorOutputs outputs;
	size_t rawData = csr_read(0xCFF);
	outputs.inReady = 1 & rawData;
	outputs.outValid = 1 & (rawData >> 1);
	outputs.outBits = ((1 << 16) - 1) & (rawData >> 2);
	outputs.dataOutLength = rawData >> 18;
	return outputs;
}

int resetCompressor()
{
	return csr_read(0xCED);
}

int resetDecompressor()
{
	return csr_read(0xCEE);
}

int main()
{
	// This reads the instruction count register.
	printf("Hello World!\n");
	printf("The current cycle count is %d, and the current instruction count is %d\n", csr_read(0x8FF), csr_read(0x8FE));
	csr_write(0x8FF, 0);
	int cycle1 = csr_read(0x8FF);
	int cycle2 = csr_read(0x8FF);
	csr_write(0x8FE, 0);
	int instruction1 = csr_read(0x8FE);
	int instruction2 = csr_read(0x8FE);
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

	printf("compressor reset = %d\n", resetCompressor());
	printf("decompressor reset = %d\n", resetDecompressor());

	char inCharacterArray[CHARACTERS];
	char outCharacterArray[CHARACTERS];
	// This is the easiest compression to perform, all zeroes.
	for (size_t index = 0; index < CHARACTERS; index++)
	{
		inCharacterArray[index] = 0;
	}

	// This is used to iterate through all the input characters and put them into the compressor.
	size_t currentInCharacterIndex = 0;
	// This is used to iterate through all the output characters and put them into the output array.
	size_t currentOutCharacterIndex = 0;
	while (currentInCharacterIndex < CHARACTERS)
	{
		struct compressorOutputs compOut = readCompressorOutputs();
		struct decompressorOutputs decompOut = readDecompressorOutputs();

		// Feed in the next character to the compressor input
		if (compOut.inReady)
		{
			printf("Inputting %d to compressor\n", inCharacterArray[currentInCharacterIndex]);
			writeCompressorInputs(false, true, false, inCharacterArray[currentInCharacterIndex]);
			currentInCharacterIndex++;
		}
		else if (compOut.outValid && decompOut.inReady)
		{
			printf("compressor out and decompressor input is %d", compOut.outBits);
			writeCompressorInputs(false, false, true, 0);
			writeDecompressorInputs(true, false, compOut.outBits);
		}
		else if (decompOut.outValid)
		{
			printf("decompressor output is valid, outputting bits %d of dataOutLength %d", decompOut.outBits, decompOut.dataOutLength);
			writeDecompressorInputs(false, true, 0);
			for (size_t index = 0; index < decompOut.dataOutLength; index++)
			{
				outCharacterArray[currentOutCharacterIndex + index] = decompOut.outBits >> (CHARACTER_BITS * (decompOut.dataOutLength - 1 - index));
				currentOutCharacterIndex++;
			}
		}
	}

	return 0;
}
