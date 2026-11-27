`timescale 1ns/1ps

module tb_fir_mmio;

    logic clk = 0;
    logic reset_n = 0;

    logic [1:0] address = 0;
    logic read = 0;
    logic write = 0;

    logic [31:0] writedata = 0;
    logic [31:0] readdata;

    always #5 clk = ~clk;

    fir_mmio dut (
        .clk(clk),
        .reset_n(reset_n),
        .address(address),
        .read(read),
        .write(write),
        .writedata(writedata),
        .readdata(readdata)
    );

    task escribir(input [1:0] dir, input [31:0] dato);
        @(negedge clk);
        address = dir;
        writedata = dato;
        write = 1;

        @(negedge clk);
        write = 0;
    endtask

    task leer(input [1:0] dir, output [31:0] dato);
        @(negedge clk);
        address = dir;
        read = 1;

        @(posedge clk);
        dato = readdata;

        @(negedge clk);
        read = 0;
    endtask

    logic [31:0] estado;
    logic [31:0] resultado;

    initial begin
        #20;
        reset_n = 1;

        leer(1, estado);

        if (estado != 1)
            $fatal(1, "ERROR: filtro no disponible");

        $display("PASS: registro STATUS inicial");

        // Izquierdo = 32767, derecho = -32768
        escribir(2, 32'h80007FFF);

        leer(1, estado);

        if (estado != 0)
            $fatal(1, "ERROR: estado ocupado");

        $display("PASS: filtro ocupado");

        while (!estado[1])
            leer(1, estado);

        leer(3, resultado);

        $display("Canal izquierdo: %0d",
                 $signed(resultado[15:0]));

        $display("Canal derecho: %0d",
                 $signed(resultado[31:16]));

        if (resultado != 32'hFFF90006)
            $fatal(1, "ERROR: resultado incorrecto");

        $display("PASS: procesamiento estereo");

        leer(1, estado);

        if (estado != 1)
            $fatal(1, "ERROR: filtro no disponible");

        escribir(0, 1);

        leer(1, estado);

        if (estado != 1)
            $fatal(1, "ERROR: reinicio del filtro");

        escribir(2, 0);

        leer(1, estado);

        while (!estado[1])
            leer(1, estado);

        leer(3, resultado);

        if (resultado != 0)
            $fatal(1, "ERROR: historial no reiniciado");

        $display("PASS: registro CONTROL");
        $display("PASS: tb_fir_mmio");

        $finish;
    end

    initial begin
        #100000;
        $fatal(1, "ERROR: timeout");
    end

endmodule