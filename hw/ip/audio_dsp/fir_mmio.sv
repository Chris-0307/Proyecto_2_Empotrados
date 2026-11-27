module fir_mmio (
    input logic clk,
    input logic reset_n,

    input logic [1:0] address,
    input logic read,
    input logic write,
    input logic [31:0] writedata,
    output logic [31:0] readdata
);

    logic ready_l, ready_r;
    logic valid_l, valid_r;

    logic signed [15:0] salida_l;
    logic signed [15:0] salida_r;

    logic entrada_valida;
    logic leer_salida;
    logic soft_reset;
    logic fir_reset_n;

    assign fir_reset_n = reset_n & ~soft_reset;

    assign entrada_valida = write && address == 2 &&
                            ready_l && ready_r && fir_reset_n;

    assign leer_salida = read && address == 3 &&
                         valid_l && valid_r;

    // Reiniciar el filtro mediante el registro CONTROL
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n)
            soft_reset <= 0;
        else
            soft_reset <= write && address == 0 && writedata[0];
    end

    // Filtro del canal izquierdo
    fir_core filtro_l (
        .clk(clk),
        .rst_n(fir_reset_n),
        .in_valid(entrada_valida),
        .in_ready(ready_l),
        .in_sample(writedata[15:0]),
        .out_valid(valid_l),
        .out_ready(leer_salida),
        .out_sample(salida_l)
    );

    // Filtro del canal derecho
    fir_core filtro_r (
        .clk(clk),
        .rst_n(fir_reset_n),
        .in_valid(entrada_valida),
        .in_ready(ready_r),
        .in_sample(writedata[31:16]),
        .out_valid(valid_r),
        .out_ready(leer_salida),
        .out_sample(salida_r)
    );

    // Lectura de los registros
    always_comb begin
        readdata = 0;

        case (address)
            0: readdata = 0;

            // STATUS
            1: begin
                readdata[0] = ready_l && ready_r && fir_reset_n;
                readdata[1] = valid_l && valid_r;
            end

            // DATA_OUT
            3: readdata = {salida_r, salida_l};

            default: readdata = 0;
        endcase
    end

endmodule