`timescale 1ns / 1ps

////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer:
//
// Create Date:   20:29:30 03/21/2017
// Design Name:   bram
// Module Name:   /home/kevin/waxwing_test1/test_fixture.v
// Project Name:  waxwing_test1
// Target Device:  
// Tool versions:  
// Description: 
//
// Verilog Test Fixture created by ISE for module: bram
//
// Dependencies:
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
////////////////////////////////////////////////////////////////////////////////

module test_fixture;

	// Inputs
	reg clka;
	reg [0:0] wea;
	reg [15:0] addra;
	reg [7:0] dina;

	// Outputs
	wire [7:0] douta;

	// Instantiate the Unit Under Test (UUT)
	bram uut (
		.clka(clka), 
		.wea(wea), 
		.addra(addra), 
		.dina(dina), 
		.douta(douta)
	);

	initial begin
		// Initialize Inputs
		clka = 0;
		wea = 0;
		addra = 0;
		dina = 0;

		// Wait 100 ns for global reset to finish
		#100;
        
		// Add stimulus here

	end
	
	always
		#5 clka = !clka;
	
	parameter S_SET_ADDR = 2'b00;
	parameter S_READ_DATA = 2'b01;
	
	reg [7:0] tmp = 0;
	reg [1:0] state = S_SET_ADDR;
	reg [15:0] next_addra = 0;
	
	reg [3:0] idle = 0;
	
	always @(posedge clka) begin
		if (idle != 0)
			idle = idle - 1;
	end
	
	always @(posedge clka) begin
		if (idle == 0) begin
			case (state)
				S_SET_ADDR : begin
					addra <= next_addra;
					state <= S_READ_DATA;
				end
				
				S_READ_DATA: begin
					tmp <= douta;
					state <= S_SET_ADDR;
					next_addra = addra + 1;
				end
			endcase
		end
	end
      
endmodule


