sbt "runMain vexriscv.GenCoreDefault --externalInterruptArray=true --csrPluginConfig=linux-minimal" && cat lzCompressNew.v >> VexRiscv.v && cat lzDecompressNew.v >> VexRiscv.v 
