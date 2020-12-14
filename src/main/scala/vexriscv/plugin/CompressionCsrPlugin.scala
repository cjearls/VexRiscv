package vexriscv

import spinal.core._
import spinal.lib.io.TriStateArray
import spinal.lib.{Flow, master}
import vexriscv.plugin.{CsrInterface, Plugin}
import vexriscv.{DecoderService, Stageable, VexRiscv} 
class LzwCompressor() extends BlackBox {
  val io = new Bundle{
    val clk = in Bool
    val reset = in Bool
    val stop = in Bool
    val in_valid = in Bool
    val out_ready = in Bool
    val in_bits = in UInt(8 bits)
    val in_ready = out Bool
    val out_valid = out Bool
    val out_bits = out UInt(9 bits)
  }

  // map the current clock domain to the io.clk pin
  mapClockDomain(clock=io.clk, reset=io.reset)
  // Set the path to look for the necessary dependency.
  addRTLPath(s"${sys.env("VEXRISCV_ROOT")}/lzCompressNew.v")
}

class LzwDecompressor() extends BlackBox {
  val io = new Bundle{
    val clk = in Bool
    val reset = in Bool
    val in_valid = in Bool
    val out_ready = in Bool
    val in_bits = in UInt(9 bits)
    val in_ready = out Bool
    val out_valid = out Bool
    val out_bits = out UInt(16 bits)
    val dataOutLength = out UInt(2 bits)
  }

  // map the current clock domain to the io.clk pin
  mapClockDomain(clock=io.clk, reset=io.reset)
  // Set the path to look for the necessary dependency.
  addRTLPath(s"${sys.env("VEXRISCV_ROOT")}/lzDecompressNew.v")
}

// This code was copied from an example, the actual Csr for interfacing with the 
// compression and decompression blackbox hasn't been implemented yet.
class CompressionCsrPlugin extends Plugin[VexRiscv]{
  override def build(pipeline: VexRiscv): Unit = {
    import pipeline._
    import pipeline.config._

    pipeline plug new Area{
      val instructionCounter = Reg(UInt(32 bits))
      val cycleCounter = Reg(UInt(32 bits))
      // When you write to the compressor or decompressor inputs, it sends that input to the compressor or decompressor 
      // for one clock cycle.
      val compressorInputs = Reg(UInt(11 bits))
      val compressorOutputs = Reg(UInt(32 bits))
      val decompressorInputs = Reg(UInt(11 bits))
      val decompressorOutputs = Reg(UInt(32 bits))
      // These registers determine whether the compressor or decompressor inputs are written.
      val writeCompressorInputs = Reg(Bool)
      val writeDecompressorInputs = Reg(Bool)

      cycleCounter := cycleCounter + 1
      when(writeBack.arbitration.isFiring) {
        instructionCounter := instructionCounter + 1
      }

      val compressor = new LzwCompressor
      when(writeCompressorInputs){
        compressor.io.stop <> compressorInputs(0)
        compressor.io.in_valid <> compressorInputs(1)
        compressor.io.out_ready <> compressorInputs(2)
        compressor.io.in_bits <> (compressorInputs>>3)
      }.otherwise{
        compressor.io.stop <> Bool(false)
        compressor.io.in_valid <> Bool(false)
        compressor.io.out_ready <> Bool(false)
        compressor.io.in_bits <> 0
      }

      val decompressor = new LzwDecompressor
      when(writeDecompressorInputs){
        decompressor.io.in_valid <> decompressorInputs(0)
        decompressor.io.out_ready <> decompressorInputs(1)
        decompressor.io.in_bits <> (decompressorInputs>>2)
      }.otherwise{
        decompressor.io.in_valid <> Bool(false)
        decompressor.io.out_ready <> Bool(false)
        decompressor.io.in_bits <> 0
      }

      val csrService = pipeline.service(classOf[CsrInterface])
      csrService.rw(0x8FA, compressorInputs)
      writeCompressorInputs := Bool(false)
      csrService.onWrite(0x8FA){
        writeCompressorInputs := Bool(true)
      }
      csrService.rw(0x8FB, compressorOutputs)
      csrService.rw(0x8FC, decompressorInputs)
      writeDecompressorInputs := Bool(false)
      csrService.onWrite(0x8FC){
        writeDecompressorInputs := Bool(true)
      }
      csrService.rw(0x8FD, decompressorOutputs)
      csrService.rw(0x8FE, instructionCounter)
      csrService.rw(0x8FF, cycleCounter)
    }
  }
}

