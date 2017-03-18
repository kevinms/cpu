module test(
	output reg A,
	input clk
);

	reg slow_clk = 1'b0;
	reg [3:0] clk_count = 4'b0000;

	always @(posedge clk) begin
		if (clk_count == 4'b0001)
			clk_count <= 4'b0000;
		else
			clk_count <= clk_count+1;
		slow_clk <= (clk_count == 4'b0000);
	end

	reg [2:0] counter = 3'b000;
	reg [7:0] number = 8'b11010010;
	
	always @(posedge clk) begin
		counter <= counter + 1;
		A <= number[counter];
	end

endmodule
