module test(
	output [15:0] A,
	output [15:0] B,
	output [15:0] C,
	input clk
);

	reg [47:0] counter = 0;
	
	always @(posedge clk) begin
		counter = counter + 1;
	end
	
	assign A[15:0] = counter[35:20];
	assign B[15:0] = counter[31:16];
	assign C[15:0] = counter[15:0];

endmodule
