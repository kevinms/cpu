`include "alu.v"`

module cpu (
	input clk,
	output [7:0] bus_addr,
	inout  [7:0] bus_data,
	output [7:0] bus_sel
);
	reg [7:0] op;
	reg [7:0] opr0, opr1, opr2;
	reg stall;

	// Fetch Stage
	always @(posedge clk) begin
		
	end

	// Decode Stage
	always @(posedge clk) begin
		
	end

	// Execute Stage
	always @(posedge clk) begin
		
	end

	

endmodule
