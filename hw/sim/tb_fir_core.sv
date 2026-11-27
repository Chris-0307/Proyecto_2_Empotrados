`timescale 1ns/1ps

// Testbench autocontenido del FIR de 31 taps (un canal).
// Ejecutar desde la raiz del repo para que $readmemh encuentre el archivo .mem.
module tb_fir_core;
    localparam integer NUM_TAPS = 31;
    localparam real PI = 3.14159265358979323846;
    localparam real FS = 44100.0;

    logic clk = 1'b0;
    always #5 clk = ~clk;  // 100 MHz

    logic rst_n = 1'b0;
    logic in_valid = 1'b0;
    logic in_ready;
    logic signed [15:0] in_sample = '0;
    logic out_valid;
    logic out_ready = 1'b0;
    logic signed [15:0] out_sample;

    logic signed [15:0] coeffs [0:NUM_TAPS-1];
    logic signed [15:0] ref_history [0:NUM_TAPS-1];
    integer tests_passed = 0;

    fir_core #(.NUM_TAPS(NUM_TAPS)) dut (
        .clk(clk),
        .rst_n(rst_n),
        .in_valid(in_valid),
        .in_ready(in_ready),
        .in_sample(in_sample),
        .out_valid(out_valid),
        .out_ready(out_ready),
        .out_sample(out_sample)
    );

    initial $readmemh("hw/ip/audio_dsp/fir_coeffs.mem", coeffs);

    // Evita que la simulacion quede bloqueada si el DUT no responde.
    initial begin
        #10000000;
        $fatal(1, "TIMEOUT: el testbench no termino");
    end

    task automatic reset_test;
        integer j;
        begin
            @(negedge clk);
            rst_n = 1'b0;
            in_valid = 1'b0;
            in_sample = '0;
            out_ready = 1'b0;
            for (j = 0; j < NUM_TAPS; j = j + 1)
                ref_history[j] = '0;
            repeat (3) @(negedge clk);
            rst_n = 1'b1;
            @(negedge clk);
            if (in_ready !== 1'b1 || out_valid !== 1'b0)
                $fatal(1, "FALLO reset: in_ready=%b out_valid=%b",
                       in_ready, out_valid);
        end
    endtask

    // Modelo de referencia independiente del estado interno del DUT:
    // convolucion de 31 muestras + desplazamiento Q1.15 + saturacion.
    task automatic calculate_expected(
        input integer sample,
        output integer expected
    );
        integer k;
        reg signed [63:0] sum;
        reg signed [63:0] scaled_ref;
        begin
            for (k = NUM_TAPS-1; k > 0; k = k - 1)
                ref_history[k] = ref_history[k-1];
            ref_history[0] = sample[15:0];

            sum = 64'sd0;
            for (k = 0; k < NUM_TAPS; k = k + 1)
                sum = sum + ($signed(ref_history[k]) * $signed(coeffs[k]));

            scaled_ref = sum >>> 15;
            if (scaled_ref > 64'sd32767)
                expected = 32767;
            else if (scaled_ref < -64'sd32768)
                expected = -32768;
            else
                expected = scaled_ref;
        end
    endtask

    task automatic send_and_check(
        input integer sample,
        input integer stall_cycles
    );
        integer expected;
        integer c;
        begin
            calculate_expected(sample, expected);

            // Presentar muestra antes del flanco positivo: handshake valido.
            while (in_ready !== 1'b1) @(negedge clk);
            @(negedge clk);
            in_sample = sample[15:0];
            in_valid = 1'b1;
            out_ready = 1'b0;
            @(posedge clk);
            #1;
            if (in_ready !== 1'b0)
                $fatal(1, "FALLO: el DUT no paso a ocupado");
            @(negedge clk);
            in_valid = 1'b0;

            while (out_valid !== 1'b1) @(negedge clk);
            if (out_sample !== expected[15:0])
                $fatal(1, "FALLO muestra %0d: esperado %0d, recibido %0d",
                       tests_passed, expected, $signed(out_sample));

            // Sin out_ready, debe sostener out_valid y el dato.
            for (c = 0; c < stall_cycles; c = c + 1) begin
                @(posedge clk);
                #1;
                if (out_valid !== 1'b1 || out_sample !== expected[15:0])
                    $fatal(1, "FALLO backpressure: salida no retenida");
            end

            @(negedge clk);
            out_ready = 1'b1;
            @(posedge clk);
            #1;
            if (out_valid !== 1'b0 || in_ready !== 1'b1)
                $fatal(1, "FALLO handshake: salida no consumida");
            @(negedge clk);
            out_ready = 1'b0;
            tests_passed = tests_passed + 1;
        end
    endtask

    // Comprueba atenuacion real para una frecuencia, descartando transitorio.
    task automatic measure_gain(
        input integer frequency_hz,
        output real gain
    );
        integer n;
        integer sample;
        integer total_in;
        integer total_out;
        begin
            reset_test();
            total_in = 0;
            total_out = 0;
            for (n = 0; n < 160; n = n + 1) begin
                sample = $rtoi(12000.0 * $sin(2.0 * PI * frequency_hz * n / FS));
                send_and_check(sample, 0);
                if (n >= NUM_TAPS) begin
                    total_in = total_in + ((sample < 0) ? -sample : sample);
                    total_out = total_out + (($signed(out_sample) < 0) ?
                                              -$signed(out_sample) : $signed(out_sample));
                end
            end
            gain = (total_in == 0) ? 0.0 : (1.0 * total_out / total_in);
        end
    endtask

    integer i;
    integer seed;
    integer coeff_sum;
    real gain_low;
    real gain_high;

    initial begin
        // Comprobar integridad de la tabla antes de estimular el DUT.
        #1;
        coeff_sum = 0;
        for (i = 0; i < NUM_TAPS; i = i + 1) begin
            if ((^coeffs[i]) === 1'bx)
                $fatal(1, "FALLO: coeficiente %0d no inicializado", i);
            coeff_sum = coeff_sum + $signed(coeffs[i]);
        end
        if (coeff_sum != 32768)
            $fatal(1, "FALLO: suma de coeficientes %0d (esperado 32768)", coeff_sum);
        $display("PASS: tabla Q1.15 (31 coeficientes, suma = 32768)");

        $display("TEST 1: impulso y respuesta completa");
        reset_test();
        send_and_check(16384, 4);
        for (i = 0; i < NUM_TAPS + 3; i = i + 1)
            send_and_check(0, 0);

        $display("TEST 2: senal continua positiva y negativa");
        reset_test();
        for (i = 0; i < 40; i = i + 1) send_and_check(10000, 0);
        reset_test();
        for (i = 0; i < 40; i = i + 1) send_and_check(-10000, 0);

        $display("TEST 3: extremos, alternancia y datos aleatorios");
        reset_test();
        for (i = 0; i < 30; i = i + 1)
            send_and_check((i % 2 == 0) ? 32767 : -32768, 1);
        seed = 12345;
        for (i = 0; i < 60; i = i + 1)
            send_and_check($random(seed) >>> 16, 0);

        $display("TEST 4: pasa bajas deja pasar 1 kHz y atenúa 12 kHz");
        measure_gain(1000, gain_low);
        measure_gain(12000, gain_high);
        $display("Ganancia aproximada: 1 kHz = %0.4f, 12 kHz = %0.4f",
                 gain_low, gain_high);
        if (gain_low < 0.70 || gain_high > 0.20)
            $fatal(1, "FALLO: respuesta pasa-bajos inesperada");

        $display("PASS: %0d muestras verificadas contra referencia Q1.15", tests_passed);
        $display("PASS: tb_fir_core");
        $finish;
    end
endmodule
