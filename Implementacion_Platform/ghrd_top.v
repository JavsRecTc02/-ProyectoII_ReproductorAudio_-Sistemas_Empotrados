`define ENABLE_HPS

module ghrd_top(

      ///////// ADC /////////
      inout              ADC_CS_N,
      output             ADC_DIN,
      input              ADC_DOUT,
      output             ADC_SCLK,

      ///////// AUD /////////
      input              AUD_ADCDAT,
      inout              AUD_ADCLRCK,
      inout              AUD_BCLK,
      output             AUD_DACDAT,
      inout              AUD_DACLRCK,
      output             AUD_XCK,

      ///////// CLOCK2 /////////
      input              CLOCK2_50,

      ///////// CLOCK3 /////////
      input              CLOCK3_50,

      ///////// CLOCK4 /////////
      input              CLOCK4_50,

      ///////// CLOCK /////////
      input              CLOCK_50,

      ///////// DRAM /////////
      output      [12:0] DRAM_ADDR,
      output      [1:0]  DRAM_BA,
      output             DRAM_CAS_N,
      output             DRAM_CKE,
      output             DRAM_CLK,
      output             DRAM_CS_N,
      inout       [15:0] DRAM_DQ,
      output             DRAM_LDQM,
      output             DRAM_RAS_N,
      output             DRAM_UDQM,
      output             DRAM_WE_N,

      ///////// FAN /////////
      output             FAN_CTRL,

      ///////// FPGA /////////
      output             FPGA_I2C_SCLK,
      inout              FPGA_I2C_SDAT,

      ///////// GPIO /////////
      inout     [35:0]         GPIO_0,
      inout     [35:0]         GPIO_1,

      ///////// HEX0 /////////
      output      [6:0]  HEX0,

      ///////// HEX1 /////////
      output      [6:0]  HEX1,

      ///////// HEX2 /////////
      output      [6:0]  HEX2,

      ///////// HEX3 /////////
      output      [6:0]  HEX3,

      ///////// HEX4 /////////
      output      [6:0]  HEX4,

      ///////// HEX5 /////////
      output      [6:0]  HEX5,

`ifdef ENABLE_HPS
      ///////// HPS /////////
      inout              HPS_CONV_USB_N,
      output      [14:0] HPS_DDR3_ADDR,
      output      [2:0]  HPS_DDR3_BA,
      output             HPS_DDR3_CAS_N,
      output             HPS_DDR3_CKE,
      output             HPS_DDR3_CK_N,
      output             HPS_DDR3_CK_P,
      output             HPS_DDR3_CS_N,
      output      [3:0]  HPS_DDR3_DM,
      inout       [31:0] HPS_DDR3_DQ,
      inout       [3:0]  HPS_DDR3_DQS_N,
      inout       [3:0]  HPS_DDR3_DQS_P,
      output             HPS_DDR3_ODT,
      output             HPS_DDR3_RAS_N,
      output             HPS_DDR3_RESET_N,
      input              HPS_DDR3_RZQ,
      output             HPS_DDR3_WE_N,
      output             HPS_ENET_GTX_CLK,
      inout              HPS_ENET_INT_N,
      output             HPS_ENET_MDC,
      inout              HPS_ENET_MDIO,
      input              HPS_ENET_RX_CLK,
      input       [3:0]  HPS_ENET_RX_DATA,
      input              HPS_ENET_RX_DV,
      output      [3:0]  HPS_ENET_TX_DATA,
      output             HPS_ENET_TX_EN,
      inout       [3:0]  HPS_FLASH_DATA,
      output             HPS_FLASH_DCLK,
      output             HPS_FLASH_NCSO,
      inout              HPS_GSENSOR_INT,
      inout              HPS_I2C1_SCLK,
      inout              HPS_I2C1_SDAT,
      inout              HPS_I2C2_SCLK,
      inout              HPS_I2C2_SDAT,
      inout              HPS_I2C_CONTROL,
      inout              HPS_KEY,
      inout              HPS_LED,
      inout              HPS_LTC_GPIO,
      output             HPS_SD_CLK,
      inout              HPS_SD_CMD,
      inout       [3:0]  HPS_SD_DATA,
      output             HPS_SPIM_CLK,
      input              HPS_SPIM_MISO,
      output             HPS_SPIM_MOSI,
      inout              HPS_SPIM_SS,
      input              HPS_UART_RX,
      output             HPS_UART_TX,
      input              HPS_USB_CLKOUT,
      inout       [7:0]  HPS_USB_DATA,
      input              HPS_USB_DIR,
      input              HPS_USB_NXT,
      output             HPS_USB_STP,
`endif /*ENABLE_HPS*/

      ///////// IRDA /////////
      input              IRDA_RXD,
      output             IRDA_TXD,

      ///////// KEY /////////
      input       [3:0]  KEY,

      ///////// LEDR /////////
      output      [9:0]  LEDR,

      ///////// PS2 /////////
      inout              PS2_CLK,
      inout              PS2_CLK2,
      inout              PS2_DAT,
      inout              PS2_DAT2,

      ///////// SW /////////
      input       [9:0]  SW,

      ///////// TD /////////
      input              TD_CLK27,
      input      [7:0]  TD_DATA,
      input             TD_HS,
      output             TD_RESET_N,
      input             TD_VS,

      ///////// VGA /////////
      output      [7:0]  VGA_B,
      output             VGA_BLANK_N,
      output             VGA_CLK,
      output      [7:0]  VGA_G,
      output             VGA_HS,
      output      [7:0]  VGA_R,
      output             VGA_SYNC_N,
      output             VGA_VS
);

//=======================================================
//  REG/WIRE declarations
//=======================================================
  wire  hps_fpga_reset_n;
  wire [3:0] fpga_debounced_buttons;
  wire [8:0]  fpga_led_internal;
  wire [2:0]  hps_reset_req;
  wire        hps_cold_reset;
  wire        hps_warm_reset;
  wire        hps_debug_reset;
  wire [27:0] stm_hw_events;
  wire        fpga_clk_50;
  wire [27:0] hex_low_export;
  wire [13:0] hex_high_export;
  wire [9:0]  peakmeter_leds;

  assign LEDR = peakmeter_leds;
  assign stm_hw_events = {{4{1'b0}}, SW, fpga_led_internal, fpga_debounced_buttons};
  assign fpga_clk_50 = CLOCK_50;

  // 7 segmentos tiempo
  assign HEX0 = hex_low_export[6:0];
  assign HEX1 = hex_low_export[13:7];
  assign HEX2 = hex_low_export[20:14];
  assign HEX3 = hex_low_export[27:21];
  // 7 segmentos numero cancion
  assign HEX4 = hex_high_export[6:0];
  assign HEX5 = hex_high_export[13:7];

//=======================================================
//  Structural coding
//=======================================================
soc_system u0 (
          .clk_clk                               (CLOCK_50),
          .reset_reset_n                         (1'b1),
          // HPS DDR3
          .memory_mem_a                          (HPS_DDR3_ADDR),
          .memory_mem_ba                         (HPS_DDR3_BA),
          .memory_mem_ck                         (HPS_DDR3_CK_P),
          .memory_mem_ck_n                       (HPS_DDR3_CK_N),
          .memory_mem_cke                        (HPS_DDR3_CKE),
          .memory_mem_cs_n                       (HPS_DDR3_CS_N),
          .memory_mem_ras_n                      (HPS_DDR3_RAS_N),
          .memory_mem_cas_n                      (HPS_DDR3_CAS_N),
          .memory_mem_we_n                       (HPS_DDR3_WE_N),
          .memory_mem_reset_n                    (HPS_DDR3_RESET_N),
          .memory_mem_dq                         (HPS_DDR3_DQ),
          .memory_mem_dqs                        (HPS_DDR3_DQS_P),
          .memory_mem_dqs_n                      (HPS_DDR3_DQS_N),
          .memory_mem_odt                        (HPS_DDR3_ODT),
          .memory_mem_dm                         (HPS_DDR3_DM),
          .memory_oct_rzqin                      (HPS_DDR3_RZQ),
          // HPS Ethernet
          .hps_0_hps_io_hps_io_emac1_inst_TX_CLK (HPS_ENET_GTX_CLK),
          .hps_0_hps_io_hps_io_emac1_inst_TXD0   (HPS_ENET_TX_DATA[0]),
          .hps_0_hps_io_hps_io_emac1_inst_TXD1   (HPS_ENET_TX_DATA[1]),
          .hps_0_hps_io_hps_io_emac1_inst_TXD2   (HPS_ENET_TX_DATA[2]),
          .hps_0_hps_io_hps_io_emac1_inst_TXD3   (HPS_ENET_TX_DATA[3]),
          .hps_0_hps_io_hps_io_emac1_inst_RXD0   (HPS_ENET_RX_DATA[0]),
          .hps_0_hps_io_hps_io_emac1_inst_MDIO   (HPS_ENET_MDIO),
          .hps_0_hps_io_hps_io_emac1_inst_MDC    (HPS_ENET_MDC),
          .hps_0_hps_io_hps_io_emac1_inst_RX_CTL (HPS_ENET_RX_DV),
          .hps_0_hps_io_hps_io_emac1_inst_TX_CTL (HPS_ENET_TX_EN),
          .hps_0_hps_io_hps_io_emac1_inst_RX_CLK (HPS_ENET_RX_CLK),
          .hps_0_hps_io_hps_io_emac1_inst_RXD1   (HPS_ENET_RX_DATA[1]),
          .hps_0_hps_io_hps_io_emac1_inst_RXD2   (HPS_ENET_RX_DATA[2]),
          .hps_0_hps_io_hps_io_emac1_inst_RXD3   (HPS_ENET_RX_DATA[3]),
          // HPS QSPI
          .hps_0_hps_io_hps_io_qspi_inst_IO0     (HPS_FLASH_DATA[0]),
          .hps_0_hps_io_hps_io_qspi_inst_IO1     (HPS_FLASH_DATA[1]),
          .hps_0_hps_io_hps_io_qspi_inst_IO2     (HPS_FLASH_DATA[2]),
          .hps_0_hps_io_hps_io_qspi_inst_IO3     (HPS_FLASH_DATA[3]),
          .hps_0_hps_io_hps_io_qspi_inst_SS0     (HPS_FLASH_NCSO),
          .hps_0_hps_io_hps_io_qspi_inst_CLK     (HPS_FLASH_DCLK),
          // HPS SD card
          .hps_0_hps_io_hps_io_sdio_inst_CMD     (HPS_SD_CMD),
          .hps_0_hps_io_hps_io_sdio_inst_D0      (HPS_SD_DATA[0]),
          .hps_0_hps_io_hps_io_sdio_inst_D1      (HPS_SD_DATA[1]),
          .hps_0_hps_io_hps_io_sdio_inst_CLK     (HPS_SD_CLK),
          .hps_0_hps_io_hps_io_sdio_inst_D2      (HPS_SD_DATA[2]),
          .hps_0_hps_io_hps_io_sdio_inst_D3      (HPS_SD_DATA[3]),
          // HPS USB
          .hps_0_hps_io_hps_io_usb1_inst_D0      (HPS_USB_DATA[0]),
          .hps_0_hps_io_hps_io_usb1_inst_D1      (HPS_USB_DATA[1]),
          .hps_0_hps_io_hps_io_usb1_inst_D2      (HPS_USB_DATA[2]),
          .hps_0_hps_io_hps_io_usb1_inst_D3      (HPS_USB_DATA[3]),
          .hps_0_hps_io_hps_io_usb1_inst_D4      (HPS_USB_DATA[4]),
          .hps_0_hps_io_hps_io_usb1_inst_D5      (HPS_USB_DATA[5]),
          .hps_0_hps_io_hps_io_usb1_inst_D6      (HPS_USB_DATA[6]),
          .hps_0_hps_io_hps_io_usb1_inst_D7      (HPS_USB_DATA[7]),
          .hps_0_hps_io_hps_io_usb1_inst_CLK     (HPS_USB_CLKOUT),
          .hps_0_hps_io_hps_io_usb1_inst_STP     (HPS_USB_STP),
          .hps_0_hps_io_hps_io_usb1_inst_DIR     (HPS_USB_DIR),
          .hps_0_hps_io_hps_io_usb1_inst_NXT     (HPS_USB_NXT),
          // HPS SPI
          .hps_0_hps_io_hps_io_spim1_inst_CLK    (HPS_SPIM_CLK),
          .hps_0_hps_io_hps_io_spim1_inst_MOSI   (HPS_SPIM_MOSI),
          .hps_0_hps_io_hps_io_spim1_inst_MISO   (HPS_SPIM_MISO),
          .hps_0_hps_io_hps_io_spim1_inst_SS0    (HPS_SPIM_SS),
          // HPS UART
          .hps_0_hps_io_hps_io_uart0_inst_RX     (HPS_UART_RX),
          .hps_0_hps_io_hps_io_uart0_inst_TX     (HPS_UART_TX),
          // HPS I2C1
          .hps_0_hps_io_hps_io_i2c0_inst_SDA     (HPS_I2C1_SDAT),
          .hps_0_hps_io_hps_io_i2c0_inst_SCL     (HPS_I2C1_SCLK),
          // HPS I2C2
          .hps_0_hps_io_hps_io_i2c1_inst_SDA     (HPS_I2C2_SDAT),
          .hps_0_hps_io_hps_io_i2c1_inst_SCL     (HPS_I2C2_SCLK),
          // HPS GPIO
          .hps_0_hps_io_hps_io_gpio_inst_GPIO09  (HPS_CONV_USB_N),
          .hps_0_hps_io_hps_io_gpio_inst_GPIO35  (HPS_ENET_INT_N),
          .hps_0_hps_io_hps_io_gpio_inst_GPIO40  (HPS_LTC_GPIO),
          .hps_0_hps_io_hps_io_gpio_inst_GPIO48  (HPS_I2C_CONTROL),
          .hps_0_hps_io_hps_io_gpio_inst_GPIO53  (HPS_LED),
          .hps_0_hps_io_hps_io_gpio_inst_GPIO54  (HPS_KEY),
          .hps_0_hps_io_hps_io_gpio_inst_GPIO61  (HPS_GSENSOR_INT),
          // HPS reset
          .led_pio_external_connection_export     (fpga_led_internal),
          .dipsw_pio_external_connection_export   (SW),
          .button_pio_external_connection_export  (fpga_debounced_buttons),
          .hps_0_h2f_reset_reset_n                (hps_fpga_reset_n),
          .hps_0_f2h_cold_reset_req_reset_n       (~hps_cold_reset),
          .hps_0_f2h_debug_reset_req_reset_n      (~hps_debug_reset),
          .hps_0_f2h_stm_hw_events_stm_hwevents   (stm_hw_events),
          .hps_0_f2h_warm_reset_req_reset_n       (~hps_warm_reset),
          // Audio
          .audio_if_0_conduit_end_export_xck        (AUD_XCK),
          .audio_if_0_conduit_end_export_adcdat     (AUD_ADCDAT),
          .audio_if_0_conduit_end_export_adclrc     (AUD_ADCLRCK),
          .audio_if_0_conduit_end_export_dacdat     (AUD_DACDAT),
          .audio_if_0_conduit_end_export_daclrc     (AUD_DACLRCK),
          .audio_if_0_conduit_end_export_bclk       (AUD_BCLK),
          // I2C Audio
          .oc_i2c_master_0_conduit_start_export_scl (FPGA_I2C_SCLK),
          .oc_i2c_master_0_conduit_start_export_sda (FPGA_I2C_SDAT),
          // 7 segmentos
          .hex_low_pio_external_connection_export   (hex_low_export),
          .hex_high_pio_external_connection_export  (hex_high_export),
          // Volume & Peak Meter
          .volume_peakmeter_0_external_signals_enc_a  (GPIO_0[0]),
          .volume_peakmeter_0_external_signals_enc_b  (GPIO_0[1]),
          .volume_peakmeter_0_external_signals_enc_sw (GPIO_0[2]),
          .volume_peakmeter_0_external_signals_leds   (peakmeter_leds),
          // LCD I2C bidir
          .lcd_pio_external_connection_export         ({GPIO_0[4], GPIO_0[3]})
    );

// Debounce logic
debounce debounce_inst (
  .clk                                  (fpga_clk_50),
  .reset_n                              (hps_fpga_reset_n),
  .data_in                              (KEY),
  .data_out                             (fpga_debounced_buttons)
);
  defparam debounce_inst.WIDTH = 4;
  defparam debounce_inst.POLARITY = "LOW";
  defparam debounce_inst.TIMEOUT = 50000;
  defparam debounce_inst.TIMEOUT_WIDTH = 16;

// Source/Probe megawizard instance
hps_reset hps_reset_inst (
  .source_clk (fpga_clk_50),
  .source     (hps_reset_req)
);

altera_edge_detector pulse_cold_reset (
  .clk       (fpga_clk_50),
  .rst_n     (hps_fpga_reset_n),
  .signal_in (hps_reset_req[0]),
  .pulse_out (hps_cold_reset)
);
  defparam pulse_cold_reset.PULSE_EXT = 6;
  defparam pulse_cold_reset.EDGE_TYPE = 1;
  defparam pulse_cold_reset.IGNORE_RST_WHILE_BUSY = 1;

altera_edge_detector pulse_warm_reset (
  .clk       (fpga_clk_50),
  .rst_n     (hps_fpga_reset_n),
  .signal_in (hps_reset_req[1]),
  .pulse_out (hps_warm_reset)
);
  defparam pulse_warm_reset.PULSE_EXT = 2;
  defparam pulse_warm_reset.EDGE_TYPE = 1;
  defparam pulse_warm_reset.IGNORE_RST_WHILE_BUSY = 1;

altera_edge_detector pulse_debug_reset (
  .clk       (fpga_clk_50),
  .rst_n     (hps_fpga_reset_n),
  .signal_in (hps_reset_req[2]),
  .pulse_out (hps_debug_reset)
);
  defparam pulse_debug_reset.PULSE_EXT = 32;
  defparam pulse_debug_reset.EDGE_TYPE = 1;
  defparam pulse_debug_reset.IGNORE_RST_WHILE_BUSY = 1;

endmodule
