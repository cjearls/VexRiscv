#include <stdio.h>
#include "src/main/c/emulator/src/riscv.h"
#include <stdbool.h>

struct compressorOutputs {
	bool inReady;
	bool outValid;
	unsigned int outBits;
};

struct decompressorOutputs {
	bool inReady;
	bool outValid;
	unsigned int outBits;
	unsigned int dataOutLength;
};

void writeCompressorInputs(bool stop, bool inValid, bool outReady, unsigned int inBits){
	csr_write(0x8FC, stop | (inValid<<1) | (outReady<<2) | (inBits << 3));
}

void writeDecompressorInputs(bool inValid, bool outReady, unsigned int inBits){
	csr_write(0x8FD, inValid | (outReady << 1) | (inBits << 2));
}

struct compressorOutputs readCompressorOutputs(){
	struct compressorOutputs outputs;
	unsigned int rawData = csr_read(0xCFE);
	outputs.inReady = 1 | rawData;
	outputs.outValid = 1 | (rawData >> 1);
	outputs.outBits = rawData >> 2;
	return outputs;
}

struct decompressorOutputs readDecompressorOutputs(){
	struct decompressorOutputs outputs;
	unsigned int rawData = csr_read(0xCFF);
	outputs.inReady = 1 | rawData;
	outputs.outValid = 1 | (rawData >> 1);
	outputs.outBits = ((1<<16)-1) | (rawData >> 2);
	outputs.dataOutLength = rawData >> 18;
	return outputs;
}

int main() {
	// This reads the instruction count register.
	printf("Hello World!\n");
	printf("The current cycle count is %d, and the current instruction count is %d", csr_read(0x8FF), csr_read(0x8FE));
	csr_write(0x8FF, 0);
	int cycle1 = csr_read(0x8FF);
	int cycle2 = csr_read(0x8FF);
	csr_write(0x8FE, 0);
	int instruction1 = csr_read(0x8FE);
	int instruction2= csr_read(0x8FE);
	printf("Cycle1: %d\nCycle2: %d\nTime1: %d\nTime2: %d\n Cycle difference: %d, Time difference: %d\n", cycle1, cycle2, instruction1, instruction2, cycle2-cycle1, instruction2-instruction1);

	// reading outputs from compressor and decompressor
	printf("compressor: inready=%d, outvalid=%d, outbits=%d\ndecompressor: inready=%d, outvalid=%d, outbits=%d, dataoutlength=%d\n",
	readCompressorOutputs().inReady,
	readCompressorOutputs().outValid,
	readCompressorOutputs().outBits,
	readDecompressorOutputs().inReady,
	readDecompressorOutputs().outValid,
	readDecompressorOutputs().outBits,
	readDecompressorOutputs().dataOutLength);

	return 0;
}
