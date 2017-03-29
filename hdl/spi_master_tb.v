`timescale 1ns / 1ps

////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer:
//
// Create Date:   01:11:12 03/29/2017
// Design Name:   spi_master
// Module Name:   /home/kevin/waxwing_test1/spi_master_tb.v
// Project Name:  waxwing_test1
// Target Device:  
// Tool versions:  
// Description: 
//
// Verilog Test Fixture created by ISE for module: spi_master
//
// Dependencies:
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
////////////////////////////////////////////////////////////////////////////////

module spi_master_tb;

	// Inputs
	reg miso;
	reg clk;
	reg reset;
	reg write;
	reg [7:0] din;
	reg cpol;
	reg cpha;

	// Outputs
	wire sck;
	wire mosi;
	wire [7:0] ss;
	wire busy;
	wire [7:0] dout;

	// Instantiate the Unit Under Test (UUT)
	spi_master uut (
		.sck(sck), 
		.mosi(mosi), 
		.miso(miso), 
		.ss(ss), 
		.clk(clk), 
		.reset(reset), 
		.write(write), 
		.busy(busy), 
		.din(din), 
		.dout(dout), 
		.cpol(cpol), 
		.cpha(cpha)
	);

	initial begin
		// Initialize Inputs
		miso = 0;
		clk = 0;
		reset = 0;
		write = 0;
		din = 0;
		cpol = 0;
		cpha = 0;

		// Wait 100 ns for global reset to finish
		#100;
        
		// Add stimulus here
		din = 42;
		write = 1;
		
		#5 miso = 1;
		#20 miso = 0;
		#20 miso = 0;
		#20 miso = 1;
		#20 miso = 0;
		#20 miso = 1;
		#20 miso = 0;
		#20 miso = 1;
		
	end
	
	always
		#5 clk = !clk;
      
endmodule
