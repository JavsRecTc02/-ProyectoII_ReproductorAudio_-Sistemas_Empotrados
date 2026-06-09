`timescale 1ns/1ps

module volume_peakmeter_tb;

// ================================================================
// Señales del DUT (Device Under Test)
// ================================================================
reg        clk;
reg        reset;
reg  [1:0] address;
reg        read;
wire [31:0] readdata;
reg        write;
reg  [31:0] writedata;
reg        enc_a;
reg        enc_b;
wire [9:0] leds;

// ================================================================
// Instancia del módulo bajo prueba
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
    .leds      (leds)
);

// ================================================================
// Clock: 50MHz → periodo 20ns
// ================================================================
initial clk = 0;
always #10 clk = ~clk;

// ================================================================
// Tarea: escribir registro Avalon-MM
// ================================================================
task avalon_write;
    input [1:0] addr;
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

// ================================================================
// Tarea: leer registro Avalon-MM
// ================================================================
task avalon_read;
    input [1:0] addr;
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
// Tarea: simular giro del encoder (horario = volumen sube)
// Cuadratura: A baja primero, B=1 → giro horario
// ================================================================
// Giro horario: A baja con B=1, espera >2ms entre cambios
task encoder_cw;
    integer i;
    begin
        for (i = 0; i < 1; i = i+1) begin
            enc_b = 1; enc_a = 1;
            #3_000_000;      // 3ms estable
            enc_a = 0;       // flanco bajante A, B=1 → CW
            #3_000_000;
            enc_b = 0;
            #3_000_000;
            enc_a = 1;
            #3_000_000;
            enc_b = 1;
            #3_000_000;
        end
    end
endtask

// ================================================================
// Tarea: simular giro antihorario (volumen baja)
// ================================================================
task encoder_ccw;
    integer i;
    begin
        for (i = 0; i < 1; i = i+1) begin
            enc_b = 1; enc_a = 1;
            #3_000_000;
            enc_b = 0;       // flanco bajante B, A=1 → CCW
            #3_000_000;
            enc_a = 0;
            #3_000_000;
            enc_b = 1;
            #3_000_000;
            enc_a = 1;
            #3_000_000;
        end
    end
endtask

// ================================================================
// Stimulus principal
// ================================================================
integer j;
initial begin
    // Reset inicial
    reset    = 1;
    write    = 0;
    read     = 0;
    enc_a    = 1;
    enc_b    = 1;
    address  = 0;
    writedata = 0;
    #100;
    reset = 0;
    #50;

    $display("\n=== TEST 1: Estado de reproduccion ===");
    avalon_write(2'h1, 32'h1); // PLAY
    avalon_read(2'h1);          // debe leer 0x1
    #200;
    avalon_write(2'h1, 32'h2); // PAUSE
    avalon_read(2'h1);          // debe leer 0x2
    #200;
    avalon_write(2'h1, 32'h0); // STOP
    avalon_read(2'h1);          // debe leer 0x0

    $display("\n=== TEST 2: Volumen inicial ===");
    avalon_read(2'h2); // debe leer 16 (valor inicial 50%)

    $display("\n=== TEST 3: Encoder giro horario (volumen sube) ===");
    encoder_cw;
    encoder_cw;
    #200;
    avalon_read(2'h2); // debe ser > 16

    $display("\n=== TEST 4: Encoder giro antihorario (volumen baja) ===");
    encoder_ccw;
    encoder_ccw;
    encoder_ccw;
    encoder_ccw;
    #200;
    avalon_read(2'h2); // debe ser < valor anterior

    $display("\n=== TEST 5: Peak meter con muestras de audio ===");
    avalon_write(2'h1, 32'h1); // PLAY state
    // Enviar muestras grandes (simula audio fuerte)
    for (j = 0; j < 20; j = j+1) begin
        avalon_write(2'h3, 32'h7FFF); // muestra máxima positiva
        #40;
    end
    avalon_read(2'h0); // STATUS: debe mostrar LEDs encendidos

    $display("\n=== TEST 6: Verificar LEDs ===");
    $display("[LEDS] leds[9:0] = %010b", leds);
    $display("       leds[9]   = %b (estado reproduccion)", leds[9]);
    $display("       leds[8:1] = %08b (peak meter)", leds[8:1]);
    $display("       leds[0]   = %b (vol extremo)", leds[0]);

    #5_000_000;
    $display("\n=== SIMULACION COMPLETA ===");
    $finish;
end

// Monitor continuo de LEDs
always @(leds) begin
    $display("[t=%0t] LEDs cambiaron: %010b", $time, leds);
end

endmodule