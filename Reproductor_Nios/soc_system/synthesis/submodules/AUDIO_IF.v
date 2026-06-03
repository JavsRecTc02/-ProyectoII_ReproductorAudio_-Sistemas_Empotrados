// ============================================================================
// Copyright (c) 2012 by Terasic Technologies Inc.
// ============================================================================
//
// Permission:
//
//   Terasic grants permission to use and modify this code for use
//   in synthesis for all Terasic Development Boards and Altera Development 
//   Kits made by Terasic.  Other use of this code, including the selling 
//   ,duplication, or modification of any portion is strictly prohibited.
//
// Disclaimer:
//
//   This VHDL/Verilog or C/C++ source code is intended as a design reference
//   which illustrates how these types of functions can be implemented.
//   It is the user's responsibility to verify their design for
//   consistency and functionality through the use of formal
//   verification methods.  Terasic provides no warranty regarding the use 
//   or functionality of this code.
//
// ============================================================================
//           
//  Terasic Technologies Inc
//  9F., No.176, Sec.2, Gongdao 5th Rd, East Dist, Hsinchu City, 30070. Taiwan
//
//
//
//                     web: http://www.terasic.com/
//                     email: support@terasic.com
//
// ============================================================================

/*

Function: 
	WOLFSON WM8731 controller
	
I2C Configuration Requirements:
	Master Mode
	I2S, 16-bits
	
Clock:
	18.432MHz to XTI/MCLK pin of WM8731
	
Revision:
	1.0, 10/22/2007, Init by Richard
	
Compatibility:
	Quartus 7.2

*/

//`include "./AUDIO_ADC.v"
//`include "./AUDIO_DAC.v"
//`include "./audio_fifo.v"


module AUDIO_IF(
    avs_s1_clk,
    avs_s1_reset,
    avs_s1_address,
    avs_s1_read,
    avs_s1_readdata,
    avs_s1_write,
    avs_s1_writedata,
    //
    avs_s1_export_BCLK,
    avs_s1_export_DACLRC,
    avs_s1_export_DACDAT,
    avs_s1_export_ADCLRC,
    avs_s1_export_ADCDAT,
    avs_s1_export_XCK
);

/*****************************************************************************
 *                           Constant Declarations                           *
 *****************************************************************************/
`define DAC_LFIFO_ADDR  3'd0
`define DAC_RFIFO_ADDR  3'd1
`define ADC_LFIFO_ADDR  3'd2
`define ADC_RFIFO_ADDR  3'd3
`define CMD_ADDR        3'd4
`define STATUS_ADDR     3'd5


/*****************************************************************************
 *                             Port Declarations                             *
 *****************************************************************************/
input                           avs_s1_clk;
input                           avs_s1_reset;
input       [2:0]               avs_s1_address;
input                           avs_s1_read;
output      [15:0]              avs_s1_readdata;
input                           avs_s1_write;
input       [15:0]              avs_s1_writedata;

// audio conduit
input                           avs_s1_export_BCLK;
input                           avs_s1_export_DACLRC;
output                          avs_s1_export_DACDAT;
input                           avs_s1_export_ADCLRC;
input                           avs_s1_export_ADCDAT;
output                          avs_s1_export_XCK;


/*****************************************************************************
 *                 Internal wires and registers Declarations                 *
 *****************************************************************************/
// host
reg     [15:0]                  reg_readdata;
reg                             fifo_clear;

// dac fifo
wire                            dacfifo_full;
reg                             dacfifo_write;
reg     [31:0]                  dacfifo_writedata;

// staging registers for stereo sample
reg     [15:0]                  dac_left_sample;
reg     [15:0]                  dac_right_sample;
reg                             dac_left_valid;
reg                             dac_right_valid;

// adc fifo
wire                            adcfifo_empty;
reg                             adcfifo_read;
wire    [31:0]                  adcfifo_readdata;
reg     [31:0]                  data32_from_adcfifo;
reg     [31:0]                  data32_from_adcfifo_2;


/*****************************************************************************
 *                             Sequential logic                              *
 *****************************************************************************/

/*
 * FIFO clear pulse.
 */
always @(posedge avs_s1_clk)
begin
    if (avs_s1_reset)
        fifo_clear <= 1'b0;
    else if (avs_s1_write && (avs_s1_address == `CMD_ADDR))
        fifo_clear <= avs_s1_writedata[0];
    else if (fifo_clear)
        fifo_clear <= 1'b0;
end


/*
 * Robust stereo write logic.
 *
 * Antes:
 * - Escribir LFIFO solo actualizaba [31:16].
 * - Escribir RFIFO actualizaba [15:0] y disparaba write.
 *
 * Problema:
 * Si el par L/R no llegaba perfecto, un canal podía quedar con dato viejo.
 *
 * Ahora:
 * - Guardamos left y right por separado.
 * - Marcamos left_valid/right_valid.
 * - Solo escribimos al FIFO cuando ambos canales están disponibles.
 *
 * Formato hacia AUDIO_DAC:
 *   dacfifo_writedata[31:16] = left
 *   dacfifo_writedata[15:0]  = right
 */
always @(posedge avs_s1_clk)
begin
    if (avs_s1_reset || fifo_clear)
    begin
        dacfifo_write     <= 1'b0;
        dacfifo_writedata <= 32'd0;

        dac_left_sample   <= 16'd0;
        dac_right_sample  <= 16'd0;
        dac_left_valid    <= 1'b0;
        dac_right_valid   <= 1'b0;
    end
    else
    begin
        dacfifo_write <= 1'b0;

        if (avs_s1_write && (avs_s1_address == `DAC_LFIFO_ADDR))
        begin
            /*
             * Llega canal izquierdo.
             */
            dac_left_sample <= avs_s1_writedata;
            dac_left_valid  <= 1'b1;

            /*
             * Si el derecho ya estaba esperando, completar frame.
             */
            if (dac_right_valid)
            begin
                dacfifo_writedata <= {avs_s1_writedata, dac_right_sample};
                dacfifo_write     <= 1'b1;

                dac_left_valid    <= 1'b0;
                dac_right_valid   <= 1'b0;
            end
        end
        else if (avs_s1_write && (avs_s1_address == `DAC_RFIFO_ADDR))
        begin
            /*
             * Llega canal derecho.
             */
            dac_right_sample <= avs_s1_writedata;
            dac_right_valid  <= 1'b1;

            /*
             * Si el izquierdo ya estaba esperando, completar frame.
             */
            if (dac_left_valid)
            begin
                dacfifo_writedata <= {dac_left_sample, avs_s1_writedata};
                dacfifo_write     <= 1'b1;

                dac_left_valid    <= 1'b0;
                dac_right_valid   <= 1'b0;
            end
        end
    end
end


/*
 * Avalon-MM read response.
 */
always @(posedge avs_s1_clk)
begin
    if (avs_s1_reset || fifo_clear)
    begin
        reg_readdata        <= 16'd0;
        data32_from_adcfifo <= 32'd0;
    end
    else if (avs_s1_read && (avs_s1_address == `STATUS_ADDR))
    begin
        /*
         * bit 0 = DAC FIFO full
         * bit 1 = ADC FIFO empty
         */
        reg_readdata <= {14'd0, adcfifo_empty, dacfifo_full};
    end
    else if (avs_s1_read && (avs_s1_address == `ADC_LFIFO_ADDR))
    begin
        reg_readdata <= data32_from_adcfifo[31:16];
    end
    else if (avs_s1_read && (avs_s1_address == `ADC_RFIFO_ADDR))
    begin
        reg_readdata        <= data32_from_adcfifo[15:0];
        data32_from_adcfifo <= data32_from_adcfifo_2;
    end
end


/*
 * Read audio data from ADC FIFO.
 */
always @(posedge avs_s1_clk)
begin
    if (avs_s1_reset || fifo_clear)
    begin
        adcfifo_read          <= 1'b0;
        data32_from_adcfifo_2 <= 32'd0;
    end
    else if ((avs_s1_address == `ADC_LFIFO_ADDR) && avs_s1_read && !adcfifo_empty)
    begin
        adcfifo_read <= 1'b1;
    end
    else if (adcfifo_read)
    begin
        data32_from_adcfifo_2 <= adcfifo_readdata;
        adcfifo_read          <= 1'b0;
    end
    else
    begin
        adcfifo_read <= 1'b0;
    end
end


/*****************************************************************************
 *                            Combinational logic                            *
 *****************************************************************************/
assign avs_s1_readdata   = reg_readdata;
assign avs_s1_export_XCK = avs_s1_clk;


/*****************************************************************************
 *                              Internal Modules                             *
 *****************************************************************************/

AUDIO_DAC DAC_Instance(
    // host
    .clk        (avs_s1_clk),
    .reset      (avs_s1_reset),
    .write      (dacfifo_write),
    .writedata  (dacfifo_writedata),
    .full       (dacfifo_full),
    .clear      (fifo_clear),

    // dac
    .bclk       (avs_s1_export_BCLK),
    .daclrc     (avs_s1_export_DACLRC),
    .dacdat     (avs_s1_export_DACDAT)
);


AUDIO_ADC ADC_Instance(
    // host
    .clk        (avs_s1_clk),
    .reset      (avs_s1_reset),
    .read       (adcfifo_read),
    .readdata   (adcfifo_readdata),
    .empty      (adcfifo_empty),
    .clear      (fifo_clear),

    // adc
    .bclk       (avs_s1_export_BCLK),
    .adclrc     (avs_s1_export_ADCLRC),
    .adcdat     (avs_s1_export_ADCDAT)
);


defparam
    DAC_Instance.DATA_WIDTH = 32;

defparam
    ADC_Instance.DATA_WIDTH = 32;

endmodule


