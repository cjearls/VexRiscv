#include <stdio.h>
#include "src/main/c/emulator/src/riscv.h"

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
	return 0;
}
