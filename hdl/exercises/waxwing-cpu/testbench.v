`timescale 1ns / 1ps

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
	wire [2:0] debug_state;
	wire [7:0] debug_opcode;
	wire [7:0] debug_mode;
	wire [7:0] debug_reg0;
	wire [7:0] debug_reg1;
	wire [31:0] debug_raw2;
	wire debug_enable;

	// Instantiate the Unit Under Test (UUT)
	waxwing_test1 uut (
		.Clk(Clk), 
		.Switch(Switch), 
		.LED(LED), 
		.debug_state(debug_state), 
		.debug_opcode(debug_opcode), 
		.debug_mode(debug_mode), 
		.debug_reg0(debug_reg0), 
		.debug_reg1(debug_reg1), 
		.debug_raw2(debug_raw2),
		.debug_enable(debug_enable)
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


