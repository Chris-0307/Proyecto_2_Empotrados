// Entrada y salida: PCM de 16 bits con signo.
// Coeficientes: Q1.15, cargados desde un archivo .mem.
// Implementacion: un MAC secuencial, 1 tap por ciclo.
// Este nucleo procesa UN canal. 

module fir_core #(
    parameter integer NUM_TAPS = 31,
    // Ruta relativa al directorio de donde se ejecuta el simulador.
    parameter COEFF_FILE = "hw/ip/audio_dsp/fir_coeffs.mem"
) (
    input  logic               clk,
    input  logic               rst_n,

    input  logic               in_valid,
    output logic               in_ready,
    input  logic signed [15:0] in_sample,

    output logic               out_valid,
    input  logic               out_ready,
    output logic signed [15:0] out_sample
);

    localparam integer TAP_BITS = (NUM_TAPS > 1) ? $clog2(NUM_TAPS) : 1;

    typedef enum logic [1:0] {
        IDLE,
        MAC,
        SCALE,
        OUTPUT
    } state_t;

    state_t state;

    logic signed [15:0] coeffs [0:NUM_TAPS-1];
    logic signed [15:0] history [0:NUM_TAPS-1];
    logic [TAP_BITS-1:0] tap_index;

    // 40 bits evitan overflow al sumar 31 productos de 16x16 bits.
    logic signed [39:0] acc;
    logic signed [31:0] product;
    logic signed [39:0] scaled;

    integer i;

    initial begin
        $readmemh(COEFF_FILE, coeffs);
    end

    assign in_ready  = (state == IDLE);
    assign out_valid = (state == OUTPUT);

    assign product = $signed(history[tap_index]) * $signed(coeffs[tap_index]);
    // Los coeficientes estan en Q1.15, por eso se desplaza 15 bits.
    assign scaled = acc >>> 15;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state      <= IDLE;
            tap_index  <= '0;
            acc        <= '0;
            out_sample <= '0;
            for (i = 0; i < NUM_TAPS; i = i + 1)
                history[i] <= '0;
        end else begin
            case (state)
                IDLE: begin
                    if (in_valid) begin
                        // Agregar la muestra nueva y desplazar las anteriores.
                        for (i = NUM_TAPS-1; i > 0; i = i - 1)
                            history[i] <= history[i-1];
                        history[0] <= in_sample;

                        tap_index <= '0;
                        acc       <= '0;
                        state     <= MAC;
                    end
                end

                MAC: begin
                    // Acumulacion: sum(x[k] * h[k]).
                    acc <= acc + {{8{product[31]}}, product};

                    if (tap_index == NUM_TAPS-1)
                        state <= SCALE;
                    else
                        tap_index <= tap_index + 1'b1;
                end

                SCALE: begin
                    // Saturar a PCM signed de 16 bits, sin wraparound.
                    if (scaled > 40'sd32767)
                        out_sample <= 16'sh7FFF;
                    else if (scaled < -40'sd32768)
                        out_sample <= 16'sh8000;
                    else
                        out_sample <= scaled[15:0];

                    state <= OUTPUT;
                end

                OUTPUT: begin
                    // Mantener la salida valida hasta que el consumidor la lea.
                    if (out_ready)
                        state <= IDLE;
                end

                default: state <= IDLE;
            endcase
        end
    end
endmodule
