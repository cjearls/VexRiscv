source sourceDirectories.sh
./createNewBuildroot.sh && cd $VEXRISCV_ROOT && ./generateLiteXVexRiscv.sh && ./copyCompiledVexFiles.sh && ./synthesizeBitStream.sh
