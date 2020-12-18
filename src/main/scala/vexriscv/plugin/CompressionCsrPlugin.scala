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
      val byteNumber = 4096
      val instructionCounter = Reg(UInt(32 bits))
      val cycleCounter = Reg(UInt(32 bits))
      // When you write to the compressor or decompressor inputs, it sends that input to the compressor or decompressor 
      // for one clock cycle.
      val compressorInputs = Reg(UInt(8 bits))
      val compressorOutputs = UInt(9 bits)
      val decompressorInputs = Reg(UInt(9 bits))
      val decompressorOutputs = UInt(16 bits)
      // These registers determine whether the compressor or decompressor inputs are written.
      val writeCompressorInputs = Reg(Bool)
      val writeDecompressorInputs = Reg(Bool)
      val compressorResetSignal = Reg(Bool)
      val decompressorResetSignal = Reg(Bool)
      // This register is needed to keep the "stop" signal asserted once it is asserted for the first time so the output values can be read.
      val stopSignalReceived = Reg(Bool)
      // These registers are used to index into the compressor and decompressor buffers to be sure the correct data is being accessed.
      val compressorInputIndex = Reg(UInt(14 bits))
      val compressorOutputIndex = Reg(UInt(14 bits))
      val decompressorInputIndex = Reg(UInt(14 bits))
      val decompressorOutputIndex = Reg(UInt(14 bits))
      // These registers are used to access the compressor buffers for the compressor and decompressor themselves to keep them fed with values.
      val compressorInputAccessIndex = Reg(UInt(14 bits))
      val decompressorInputAccessIndex = Reg(UInt(14 bits))
      val compressorOutputAccessIndex = Reg(UInt(14 bits))
      val decompressorOutputAccessIndex = Reg(UInt(14 bits))
      // These registers are used as buffers for the compressors and decompressors to compute values faster.
      val compressorInputBuffer = Mem(UInt(8 bits), byteNumber)
      val compressorOutputBuffer = Mem(UInt(9 bits), byteNumber)
      val decompressorInputBuffer = Mem(UInt(9 bits), byteNumber)
      val decompressorOutputBuffer = Mem(UInt(16 bits), byteNumber)

      cycleCounter := cycleCounter + 1
      when(writeBack.arbitration.isFiring) {
        instructionCounter := instructionCounter + 1
      }

      val compressor = new lzCompressNew
      // This takes care of writing the data to the compressor input buffer.
      when(writeCompressorInputs && compressorInputIndex < byteNumber){
        compressorInputBuffer.write(compressorInputIndex(11 downto 0), compressorInputs)
        compressorInputIndex := compressorInputIndex + 1
      }

      // This takes care of inputting data from the compressor input buffer into the compressor itself
      compressor.io.io_stop <> Bool(false)
      compressor.io.io_in_valid <> Bool(false)
      compressor.io.io_in_bits <> 0
      when(compressorInputIndex > compressorInputAccessIndex && compressorInputAccessIndex < byteNumber){
        compressor.io.io_in_valid <> Bool(true)
        compressor.io.io_in_bits := compressorInputBuffer(compressorInputAccessIndex(11 downto 0))
        when(compressor.io.io_in_ready){
          compressorInputAccessIndex := compressorInputAccessIndex + 1
        }
      }

      // This takes care of getting output data from compressor and putting into the output buffer.
      compressor.io.io_out_ready <> Bool(true)
      when(compressor.io.io_out_valid){
        compressorInputBuffer(compressorOutputAccessIndex(11 downto 0)) := compressor.io.io_out_bits
        compressorOutputAccessIndex := compressorOutputAccessIndex + 1
      }

      // This sets the compressor output to the current index in the output buffer
      compressorOutputs := compressorOutputBuffer(compressorOutputIndex(11 downto 0))
      compressorResetSignal <> compressor.io.reset

      val decompressor = new lzDecompressNew
      // This takes care of writing the data to the decompressor input buffer.
      when(writeDecompressorInputs && decompressorOutputIndex < byteNumber){
        decompressorInputBuffer(decompressorInputIndex(11 downto 0)) := decompressorInputs
        decompressorInputIndex := decompressorInputIndex + 1
      }

      // This takes care of inputting data from the decompressor input buffer into the decompressor itself
      decompressor.io.io_in_valid <> Bool(false)
      decompressor.io.io_in_bits <> 0
      when(decompressorInputIndex > decompressorInputAccessIndex && decompressorInputAccessIndex < byteNumber){
        decompressor.io.io_in_valid <> Bool(true)
        decompressor.io.io_in_bits := decompressorInputBuffer(decompressorInputAccessIndex(11 downto 0))
        when(decompressor.io.io_in_ready){
          decompressorInputAccessIndex := decompressorInputAccessIndex + 1
        }
      }

      // This takes care of getting output data from decompressor and putting into the output buffer.
      decompressor.io.io_out_ready <> Bool(true)
      when(decompressor.io.io_out_valid){
        decompressorInputBuffer(decompressorOutputAccessIndex(11 downto 0)) := decompressor.io.io_out_bits
        decompressorOutputAccessIndex := decompressorOutputAccessIndex + 1
      }

      // This sets the decompressor output to the current index in the output buffer
      decompressorOutputs := decompressorOutputBuffer(decompressorOutputIndex(11 downto 0))
      decompressorResetSignal <> decompressor.io.reset


      val csrService = pipeline.service(classOf[CsrInterface])
      
      // This allows for writing to the compressor inputs.
      csrService.rw(0x8FC, compressorInputs)
      writeCompressorInputs := Bool(false)
      csrService.onWrite(0x8FC){
        writeCompressorInputs := Bool(true)
      }
      
      // This allows reading the compressor outputs.
      csrService.r(0xCFE, compressorOutputs)
      csrService.onRead(0xCFE){
        compressorOutputIndex := compressorOutputIndex + 1
      }

      // This allows writing the decompressor's inputs.
      csrService.rw(0x8FD, decompressorInputs)
      writeDecompressorInputs := Bool(false)
      csrService.onWrite(0x8FD){
        writeDecompressorInputs := Bool(true)
      }
      // This allows reading the decompressor's outputs.
      csrService.r(0xCFF, decompressorOutputs)

      // This allows for reading and writing to the instruction counter and cycle counter respectively.
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
        compressorInputIndex := 0
        compressorOutputIndex := 0
        compressorInputAccessIndex := 0
        compressorOutputAccessIndex := 0
        compressorResetSignal := Bool(true) 
        stopSignalReceived := Bool(false)
      }
      // Reset the decompressor
      csrService.onRead(0xCEE){
        decompressorInputIndex := 0
        decompressorOutputIndex := 0
        decompressorInputAccessIndex := 0
        decompressorOutputAccessIndex := 0
        decompressorResetSignal := Bool(true) 
      }
    }
  }
}

