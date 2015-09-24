
module mem (
	input clk,
	input w,
	input [7:0] addr,
	input [7:0] data,
	output reg [31:0] data_out,
);

	reg [7:0] mem [0:255];

	always @(posedge clk)
	begin
		if (w)
			mem[addr] = data;
	end

endmodule
