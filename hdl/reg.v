module reg_file (
	input clk,
	input write_enable,
	input [4:0] address,
	input [15:0] data_in,
	output [15:0] data_out
);

	// 32 registers each 16 bits
	reg [15:0] registers [0:31];

	always @(posedge clk)
	begin
		assign data_out = registers
	end

endmodule
