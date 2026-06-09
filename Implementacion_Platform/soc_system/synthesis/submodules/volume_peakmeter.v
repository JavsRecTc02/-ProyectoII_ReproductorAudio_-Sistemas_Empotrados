module volume_peakmeter (
    // Avalon-MM Slave
    input        clk,
    input        reset,
    input  [1:0] address,
    input        read,
    output reg [31:0] readdata,
    input        write,
    input  [31:0] writedata,

    // Encoder KY-040
    input        enc_a,
    input        enc_b,

    // LEDs
    output reg [9:0] leds
);

// ================================================================
// 1. DEBOUNCE DEL ENCODER
// ================================================================
reg [2:0] enc_a_sr, enc_b_sr;
always @(posedge clk) begin
    enc_a_sr <= {enc_a_sr[1:0], enc_a};
    enc_b_sr <= {enc_b_sr[1:0], enc_b};
end
wire enc_a_s = enc_a_sr[2];
wire enc_b_s = enc_b_sr[2];

// ================================================================
// 2. DECODIFICACION DEL ENCODER
// ================================================================
reg enc_a_prev;
reg [4:0] vol_level;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        vol_level  <= 5'd16;
        enc_a_prev <= 1'b1;
    end else begin
        enc_a_prev <= enc_a_s;
        if (enc_a_prev && !enc_a_s) begin
            if (enc_b_s) begin
                if (vol_level < 5'd31) vol_level <= vol_level + 1;
            end else begin
                if (vol_level > 5'd0)  vol_level <= vol_level - 1;
            end
        end
    end
end

// ================================================================
// 3. PEAK METER
// ================================================================
reg [15:0] peak_val;
reg [7:0]  peak_hold_cnt;
reg [7:0]  peak_leds;
reg [15:0] abs_s; // declarado aqui para evitar error Verilog-2001

always @(posedge clk or posedge reset) begin
    if (reset) begin
        peak_val      <= 0;
        peak_hold_cnt <= 0;
        peak_leds     <= 0;
        abs_s         <= 0;
    end else begin
        if (write && address == 2'h3) begin
            abs_s = writedata[15] ? (~writedata[15:0] + 1) : writedata[15:0];

            if (abs_s >= peak_val) begin
                peak_val      <= abs_s;
                peak_hold_cnt <= 8'd200;
            end else if (peak_hold_cnt > 0) begin
                peak_hold_cnt <= peak_hold_cnt - 1;
            end else if (peak_val > 0) begin
                peak_val <= peak_val - 16'd64;
            end

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
end

// ================================================================
// 4. REGISTROS AVALON-MM
// ================================================================
reg [1:0] play_state;

always @(posedge clk or posedge reset) begin
    if (reset) begin
        play_state <= 2'b00;
    end else if (write && address == 2'h1) begin
        play_state <= writedata[1:0];
    end
end

always @(*) begin
    case (address)
        2'h0: readdata = {24'b0, peak_leds};
        2'h1: readdata = {30'b0, play_state};
        2'h2: readdata = {27'b0, vol_level};
        default: readdata = 32'b0;
    endcase
end

// ================================================================
// 5. CONTROL DE LEDs
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

wire led_extreme = (vol_level == 5'd31 || vol_level == 5'd0);

always @(posedge clk) begin
    leds <= {led_state, peak_leds, led_extreme};
end

endmodule