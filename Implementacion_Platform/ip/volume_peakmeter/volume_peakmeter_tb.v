`timescale 1ns/1ps

module volume_peakmeter_tb;

// ================================================================
// Señales del DUT
// ================================================================
reg        clk;
reg        reset;
reg  [2:0] address;
reg        read;
wire [31:0] readdata;
reg        write;
reg  [31:0] writedata;
reg        enc_a;
reg        enc_b;
reg        enc_sw;
wire [9:0] leds;

// ================================================================
// Instancia del modulo bajo prueba
// ================================================================
volume_peakmeter dut (
    .clk       (clk),
    .reset     (reset),
    .address   (address),
    .read      (read),
    .readdata  (readdata),
    .write     (write),
    .writedata (writedata),
    .enc_a     (enc_a),
    .enc_b     (enc_b),
    .enc_sw    (enc_sw),
    .leds      (leds)
);

// Clock 50MHz
initial clk = 0;
always #10 clk = ~clk;

// ================================================================
// Tareas Avalon-MM
// ================================================================
task avalon_write;
    input [2:0] addr;
    input [31:0] data;
    begin
        @(posedge clk);
        address   <= addr;
        writedata <= data;
        write     <= 1;
        @(posedge clk);
        write     <= 0;
    end
endtask

task avalon_read;
    input [2:0] addr;
    begin
        @(posedge clk);
        address <= addr;
        read    <= 1;
        @(posedge clk);
        read    <= 0;
        $display("[READ] addr=0x%0X data=0x%08X", addr, readdata);
    end
endtask

// ================================================================
// Encoder giro horario (1 detent, tiempos >2ms)
// ================================================================
task encoder_cw;
    begin
        enc_b = 1; enc_a = 1; #3_000_000;
        enc_a = 0;             #3_000_000;
        enc_b = 0;             #3_000_000;
        enc_a = 1;             #3_000_000;
        enc_b = 1;             #3_000_000;
    end
endtask

// ================================================================
// Encoder giro antihorario
// ================================================================
task encoder_ccw;
    begin
        enc_b = 1; enc_a = 1; #3_000_000;
        enc_b = 0;             #3_000_000;
        enc_a = 0;             #3_000_000;
        enc_b = 1;             #3_000_000;
        enc_a = 1;             #3_000_000;
    end
endtask

// ================================================================
// Simular press del boton (activo bajo, >20ms)
// ================================================================
task press_button;
    begin
        enc_sw = 0;       // presionar
        #25_000_000;      // 25ms > debounce 20ms
        enc_sw = 1;       // soltar
        #25_000_000;
    end
endtask

integer j;

initial begin
    // Reset
    reset    = 1;
    write    = 0;
    read     = 0;
    enc_a    = 1;
    enc_b    = 1;
    enc_sw   = 1;
    address  = 0;
    writedata = 0;
    #100;
    reset = 0;
    #50;

    // ============================================================
    $display("\n=== TEST 1: Estado de reproduccion ===");
    avalon_write(3'h1, 32'h1); // PLAY
    avalon_read(3'h1);          // debe leer 0x1
    $display("[LEDS] leds[9]=%b (debe parpadear a 1Hz)", leds[9]);
    #200;
    avalon_write(3'h1, 32'h2); // PAUSE
    avalon_read(3'h1);          // debe leer 0x2
    #200;
    avalon_write(3'h1, 32'h0); // STOP
    avalon_read(3'h1);          // debe leer 0x0

    // ============================================================
    $display("\n=== TEST 2: Volumen inicial ===");
    avalon_read(3'h2); // debe leer 16 (0x10)

    // ============================================================
    $display("\n=== TEST 3: Encoder giro horario (volumen sube) ===");
    encoder_cw;
    encoder_cw;
    #200;
    avalon_read(3'h2); // debe ser > 16

    // ============================================================
    $display("\n=== TEST 4: Encoder giro antihorario (volumen baja) ===");
    encoder_ccw;
    encoder_ccw;
    encoder_ccw;
    encoder_ccw;
    #200;
    avalon_read(3'h2); // debe ser < valor anterior

    // ============================================================
    $display("\n=== TEST 5: Peak meter con muestras de audio ===");
    avalon_write(3'h1, 32'h1); // PLAY
    for (j = 0; j < 20; j = j+1) begin
        avalon_write(3'h3, 32'h7FFF);
        #40;
    end
    avalon_read(3'h0); // STATUS: debe mostrar LEDs encendidos

    // ============================================================
    $display("\n=== TEST 6: Filtro activo (bit 2 de CTRL) ===");
    avalon_write(3'h1, 32'h5); // play_state=01 + filter_active=1
    avalon_read(3'h1);          // debe leer 0x5
    $display("[LEDS] leds[0]=%b (debe ser 1 = filtro activo)", leds[0]);
    #200;
    avalon_write(3'h1, 32'h1); // apagar filtro, mantener play
    avalon_read(3'h1);
    $display("[LEDS] leds[0]=%b (debe ser 0 = filtro inactivo)", leds[0]);

    // ============================================================
    $display("\n=== TEST 7: Boton del encoder ===");
    avalon_read(3'h4);   // debe ser 0
    press_button;
    avalon_read(3'h4);   // debe ser 1
    avalon_write(3'h4, 32'h1);  // W1C: limpiar
    avalon_read(3'h4);   // debe ser 0
    
    // ============================================================
    $display("\n=== TEST 8: Verificar LEDs finales ===");
    $display("[LEDS] leds[9:0] = %010b", leds);
    $display("       leds[9]   = %b (estado reproduccion)", leds[9]);
    $display("       leds[8:1] = %08b (peak meter)", leds[8:1]);
    $display("       leds[0]   = %b (filtro activo)", leds[0]);

    #5_000_000;
    $display("\n=== SIMULACION COMPLETA ===");
    $finish;
end

// Monitor LEDs
always @(leds) begin
    $display("[t=%0t] LEDs cambiaron: %010b", $time, leds);
end

endmodule