mescale 1ns / 1ps

////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer:
//
// Create Date:   00:31:15 03/22/2017
// Design Name:   waxwing_test1
// Module Name:   /home/kevin/waxwing_test1/cpu_test2.v
// Project Name:  waxwing_test1
// Target Device:  
// Tool versions:  
// Description: 
//
// Verilog Test Fixture created by ISE for module: waxwing_test1
//
// Dependencies:
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
////////////////////////////////////////////////////////////////////////////////

module cpu_test2;

	// Inputs
	reg Clk;
	reg [6:0] Switch;

	// Outputs
	wire [7:0] LED;

	// Instantiate the Unit Under Test (UUT)
	waxwing_test1 uut (
		.Clk(Clk), 
		.Switch(Switch), 
		.LED(LED)
	);

	initial begin
		// Initialize Inputs
		Clk = 0;
		Switch = 0;

		// Wait 100 ns for global reset to finish
		#100;
        
		// Add stimulus here

	end
      
	always
		#5 Clk = !Clk;
		
endmodule


