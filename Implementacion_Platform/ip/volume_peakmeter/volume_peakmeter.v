module volume_peakmeter (
    // Avalon-MM Slave
    input        clk,
    input        reset,
    input  [2:0] address,
    input        read,
    output reg [31:0] readdata,
    input        write,
    input  [31:0] writedata,

    // Encoder KY-040
    input        enc_a,
    input        enc_b,
    input        enc_sw,

    // LEDs
    output reg [9:0] leds
);

// ================================================================
// 1. SINCRONIZACION DE SEÑALES ASINCRONAS
// Dos flip-flops para cruzar dominio de reloj de forma segura.
// Evita metaestabilidad.
// ================================================================
reg [1:0] enc_a_sync, enc_b_sync;
always @(posedge clk or posedge reset) begin
    if (reset) begin
        enc_a_sync <= 2'b11;
        enc_b_sync <= 2'b11;
    end else begin
        enc_a_sync <= {enc_a_sync[0], enc_a};
        enc_b_sync <= {enc_b_sync[0], enc_b};
    end
end
wire enc_a_s = enc_a_sync[1];
wire enc_b_s = enc_b_sync[1];

// ================================================================
// 2. DEBOUNCE POR CONTADOR (2ms a 50MHz = 100,000 ciclos)
// Principio: solo actualizamos el estado estable cuando la señal
// se mantiene constante por 100,000 ciclos consecutivos.
// Esto elimina completamente el bounce mecanico del KY-040.
// ================================================================
parameter DEBOUNCE_CYCLES = 100_000;

reg [16:0] debounce_cnt;
reg        enc_a_db, enc_b_db;   // señales debounced
reg        enc_a_last, enc_b_last;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        debounce_cnt <= 0;
        enc_a_db     <= 1'b1;
        enc_b_db     <= 1'b1;
        enc_a_last   <= 1'b1;
        enc_b_last   <= 1'b1;
    end else begin
        if (enc_a_s != enc_a_last || enc_b_s != enc_b_last) begin
            // Hubo cambio: reiniciar contador
            debounce_cnt <= 0;
            enc_a_last   <= enc_a_s;
            enc_b_last   <= enc_b_s;
        end else if (debounce_cnt < DEBOUNCE_CYCLES) begin
            debounce_cnt <= debounce_cnt + 1;
        end else begin
            // Señal estable por 2ms: aceptar
            enc_a_db <= enc_a_s;
            enc_b_db <= enc_b_s;
        end
    end
end

// ================================================================
// 3. DECODIFICACION POR FSM DE 4 ESTADOS (codigo Gray)
// 
// La secuencia valida del encoder en cuadratura:
//   CW:  00 -> 01 -> 11 -> 10 -> 00
//   CCW: 00 -> 10 -> 11 -> 01 -> 00
//
// Cualquier transicion que no siga esta secuencia es ruido
// y se ignora. Esto es robusto contra bounce residual.
//
// Estado codificado: {enc_b_db, enc_a_db}
// ================================================================
reg [1:0] enc_state;
reg [4:0] vol_level;   // 0-31, inicio en 16 (50%)

always @(posedge clk or posedge reset) begin
    if (reset) begin
        enc_state <= 2'b11;
        vol_level <= 5'd16;
    end else begin
        case (enc_state)
            2'b00: begin
                if ({enc_b_db, enc_a_db} == 2'b01) begin
                    enc_state <= 2'b01;
                    // CW: subir volumen
                    if (vol_level < 31) vol_level <= vol_level + 1;
                end else if ({enc_b_db, enc_a_db} == 2'b10) begin
                    enc_state <= 2'b10;
                    // CCW: bajar volumen
                    if (vol_level > 0) vol_level <= vol_level - 1;
                end
            end
            2'b01: begin
                if ({enc_b_db, enc_a_db} == 2'b00)
                    enc_state <= 2'b00;
                else if ({enc_b_db, enc_a_db} == 2'b11)
                    enc_state <= 2'b11;
            end
            2'b11: begin
                if ({enc_b_db, enc_a_db} == 2'b01)
                    enc_state <= 2'b01;
                else if ({enc_b_db, enc_a_db} == 2'b10)
                    enc_state <= 2'b10;
            end
            2'b10: begin
                if ({enc_b_db, enc_a_db} == 2'b11)
                    enc_state <= 2'b11;
                else if ({enc_b_db, enc_a_db} == 2'b00)
                    enc_state <= 2'b00;
            end
        endcase
    end
end

// ================================================================
// 4. PEAK METER
// El HPS escribe muestras de 16-bit signed en registro AUDIO.
// Calculamos valor absoluto y mapeamos a 8 LEDs logaritmicamente.
// Hold de 200 muestras para que el pico sea visible.
// ================================================================
reg [15:0] peak_val;
reg [7:0]  peak_hold_cnt;
reg [7:0]  peak_leds;
reg [15:0] abs_s;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        peak_val      <= 0;
        peak_hold_cnt <= 0;
        peak_leds     <= 0;
        abs_s         <= 0;
    end else if (write && address == 3'h3) begin
        abs_s = writedata[15] ? (~writedata[15:0] + 1'b1) : writedata[15:0];

        if (abs_s >= peak_val) begin
            peak_val      <= abs_s;
            peak_hold_cnt <= 8'd200;
        end else if (peak_hold_cnt > 0) begin
            peak_hold_cnt <= peak_hold_cnt - 1;
        end else if (peak_val >= 64) begin
            peak_val <= peak_val - 16'd64;
        end else begin
            peak_val <= 0;
        end

        // Escala logaritmica: cada LED = doble de amplitud
        peak_leds <= (peak_val > 16'h7000) ? 8'hFF :
                     (peak_val > 16'h6000) ? 8'h7F :
                     (peak_val > 16'h4000) ? 8'h3F :
                     (peak_val > 16'h2000) ? 8'h1F :
                     (peak_val > 16'h1000) ? 8'h0F :
                     (peak_val > 16'h0800) ? 8'h07 :
                     (peak_val > 16'h0400) ? 8'h03 :
                     (peak_val > 16'h0200) ? 8'h01 : 8'h00;
    end
end

// ================================================================
// 5. REGISTROS AVALON-MM
// Registro 0 (STATUS) : R   - peak_leds [7:0]
// Registro 1 (CTRL)   : R/W - play_state [1:0]
// Registro 2 (VOLUME) : R   - vol_level [4:0]
// Registro 3 (AUDIO)  : W   - muestra audio (manejado arriba)
// ================================================================
reg [1:0] play_state;
reg       filter_active;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        play_state    <= 2'b00;
        filter_active <= 1'b0;
    end else if (write && address == 3'h1) begin
        play_state    <= writedata[1:0];
        filter_active <= writedata[2];   // <-- nuevo
    end
end

always @(*) begin
    case (address)
        3'h0: readdata = {24'b0, peak_leds};
        3'h1: readdata = {30'b0, play_state};
        3'h2: readdata = {27'b0, vol_level};
        3'h3: readdata = 32'b0;
        3'h4: readdata = {31'b0, enc_sw_event};
        default: readdata = 32'b0;
    endcase
end

// ================================================================
// 6. CONTROL DE LEDs
// LEDR[9]: estado reproduccion
//   play  → parpadea 1Hz
//   pause → fijo encendido
//   stop  → apagado
// LEDR[8:1]: peak meter (8 LEDs logaritmicos)
// LEDR[0]:   volumen en extremo (0 o 31)
// ================================================================
reg [25:0] blink_cnt;
reg        blink_out;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        blink_cnt <= 0;
        blink_out <= 0;
    end else if (blink_cnt >= 26'd24_999_999) begin
        blink_cnt <= 0;
        blink_out <= ~blink_out;
    end else begin
        blink_cnt <= blink_cnt + 1;
    end
end

wire led_state   = (play_state == 2'b01) ? blink_out :
                   (play_state == 2'b10) ? 1'b1 : 1'b0;

always @(posedge clk) begin
    leds <= {led_state, peak_leds, filter_active};
end

// ================================================================
// 7. DEBOUNCE Y LECTURA DEL BOTON ENC_SW
// El boton es activo bajo (LOW cuando presionado).
// Debounce de 20ms = 1,000,000 ciclos a 50MHz.
// ================================================================
reg [19:0] sw_debounce_cnt;
reg        enc_sw_db;
reg        enc_sw_last;
reg        enc_sw_prev;
reg        enc_sw_event; // pulso de un ciclo al detectar press

always @(posedge clk or posedge reset) begin
    if (reset) begin
        sw_debounce_cnt <= 0;
        enc_sw_db       <= 1'b1;
        enc_sw_last     <= 1'b1;
        enc_sw_prev     <= 1'b1;
        enc_sw_event    <= 1'b0;
    end else begin
        enc_sw_event <= 1'b0;

        if (enc_sw != enc_sw_last) begin
            sw_debounce_cnt <= 0;
            enc_sw_last     <= enc_sw;
        end else if (sw_debounce_cnt < 20'd1_000_000) begin
            sw_debounce_cnt <= sw_debounce_cnt + 1;
        end else begin
            enc_sw_db <= enc_sw_last;
            // Detectar flanco bajante (press)
            if (enc_sw_prev && !enc_sw_db)
                enc_sw_event <= 1'b1;
            enc_sw_prev <= enc_sw_db;
        end
    end
end

endmodule