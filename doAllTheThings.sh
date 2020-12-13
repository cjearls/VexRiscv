source sourceDirectories.sh
./compileHardwareDriver.sh && ./createNewBuildroot.sh && cd $VEXRISCV_ROOT && ./generateLiteXVexRiscv.sh && ./copyCompiledVexFiles.sh && ./synthesizeBitStream.sh
