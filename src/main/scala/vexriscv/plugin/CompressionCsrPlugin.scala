package vexriscv

import spinal.core._
import spinal.lib.io.TriStateArray
import spinal.lib.{Flow, master}
import vexriscv.plugin.{CsrInterface, Plugin}
import vexriscv.{DecoderService, Stageable, VexRiscv} 
class lzCompressNew() extends BlackBox {
  val io = new Bundle{
    val clock = in Bool
    val reset = in Bool
    val io_stop = in Bool
    val io_in_valid = in Bool
    val io_out_ready = in Bool
    val io_in_bits = in UInt(8 bits)
    val io_in_ready = out Bool
    val io_out_valid = out Bool
    val io_out_bits = out UInt(9 bits)
  }

  // map the current clock domain to the io.clk pin
  mapClockDomain(clock=io.clock)
  // Set the path to look for the necessary dependency.
  addRTLPath(s"${sys.env("VEXRISCV_ROOT")}/lzCompressNew.v")
  noIoPrefix()
}

class lzDecompressNew() extends BlackBox {
  val io = new Bundle{
    val clock = in Bool
    val reset = in Bool
    val io_in_valid = in Bool
    val io_out_ready = in Bool
    val io_in_bits = in UInt(9 bits)
    val io_in_ready = out Bool
    val io_out_valid = out Bool
    val io_out_bits = out UInt(16 bits)
    val io_dataOutLength = out UInt(2 bits)
  }

  // map the current clock domain to the io.clk pin
  mapClockDomain(clock=io.clock)
  // Set the path to look for the necessary dependency.
  addRTLPath(s"${sys.env("VEXRISCV_ROOT")}/lzDecompressNew.v")
  noIoPrefix()
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
      val compressorOutputs = UInt(11 bits)
      val decompressorInputs = Reg(UInt(11 bits))
      val decompressorOutputs = UInt(20 bits)
      // These registers determine whether the compressor or decompressor inputs are written.
      val writeCompressorInputs = Reg(Bool)
      val writeDecompressorInputs = Reg(Bool)
      val compressorResetSignal = Reg(Bool)
      val decompressorResetSignal = Reg(Bool)
      // This register is needed to keep the "stop" signal asserted once it is asserted for the first time so the output values can be read.
      val stopSignalReceived = Reg(Bool)

      cycleCounter := cycleCounter + 1
      when(writeBack.arbitration.isFiring) {
        instructionCounter := instructionCounter + 1
      }

      val compressor = new lzCompressNew
      when(stopSignalReceived){
        compressor.io.io_stop <> Bool(true)
        compressor.io.io_in_valid <> Bool(false)
        compressor.io.io_out_ready <> Bool(false)
        compressor.io.io_in_bits <> 0
      }
      .elsewhen(writeCompressorInputs){
        stopSignalReceived := compressorInputs(0)
        compressor.io.io_stop <> compressorInputs(0)
        compressor.io.io_in_valid <> compressorInputs(1)
        compressor.io.io_out_ready <> compressorInputs(2)
        compressor.io.io_in_bits <> (compressorInputs>>3)
      }.otherwise{
        compressor.io.io_stop <> Bool(false)
        compressor.io.io_in_valid <> Bool(false)
        compressor.io.io_out_ready <> Bool(false)
        compressor.io.io_in_bits <> 0
      }
      compressorResetSignal <> compressor.io.reset
      compressorOutputs := Cat(compressor.io.io_out_bits, compressor.io.io_out_valid, compressor.io.io_in_ready).asUInt

      val decompressor = new lzDecompressNew
      when(writeDecompressorInputs){
        decompressor.io.io_in_valid <> decompressorInputs(0)
        decompressor.io.io_out_ready <> decompressorInputs(1)
        decompressor.io.io_in_bits <> (decompressorInputs>>2)
      }.otherwise{
        decompressor.io.io_in_valid <> Bool(false)
        decompressor.io.io_out_ready <> Bool(false)
        decompressor.io.io_in_bits <> 0
      }
      decompressorResetSignal <> decompressor.io.reset
      decompressorOutputs := Cat(decompressor.io.io_dataOutLength, decompressor.io.io_out_bits, decompressor.io.io_out_valid, decompressor.io.io_in_ready).asUInt

      val csrService = pipeline.service(classOf[CsrInterface])
      csrService.rw(0x8FC, compressorInputs)
      writeCompressorInputs := Bool(false)
      csrService.onWrite(0x8FC){
        writeCompressorInputs := Bool(true)
      }
      csrService.r(0xCFE, compressorOutputs)
      csrService.rw(0x8FD, decompressorInputs)
      writeDecompressorInputs := Bool(false)
      csrService.onWrite(0x8FD){
        writeDecompressorInputs := Bool(true)
      }
      csrService.r(0xCFF, decompressorOutputs)
      csrService.rw(0x8FE, instructionCounter)
      csrService.rw(0x8FF, cycleCounter)

      val pointlessRegister = Reg(UInt(32 bits))
      pointlessRegister := 0

      // Reading these registers resets the compressor and decompressor, respectively
      csrService.r(0xCED, pointlessRegister)
      csrService.r(0xCEE, pointlessRegister)
      
      // Reset the  compressor
      compressorResetSignal := Bool(false)
      decompressorResetSignal := Bool(false)
      csrService.onRead(0xCED){
        compressorResetSignal := Bool(true) 
        stopSignalReceived := Bool(false)
      }
      // Reset the decompressor
      csrService.onRead(0xCEE){
        decompressorResetSignal := Bool(true) 
      }
    }
  }
}

