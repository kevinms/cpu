`timescale 1ns / 1ps

// LED Module demo code
// Numato Lab
// http://www.numato.com
// http://www.numato.cc
// License : CC BY-SA (http://creativecommons.org/licenses/by-sa/2.0/)

module waxwing_test1(

  // Assuming 100MHz input clock. My need to adjust the counter below
  // if any other input frequency is used
    input Clk,
	 
  // Inputs from the Push Buttons.
    input [6:0]Switch,
  
  // Output is shown on LED with different functionality.
    output reg [7:0]LED  
    );

// Register used internally	
reg [24:0]count;
reg enable;

// Provide the initial value
initial 
  begin
    LED = 8'h01;
  end
  
// Scale down the clock so that output is easily visible.
always @(posedge Clk) begin
  count <= count+1'b1;
  enable <= count[24];
end  

// Inputs
	reg [0:0] wea;
	reg [15:0] addra = 0;
	reg [7:0] dina;

	// Outputs
	wire [7:0] douta;

	// Instantiate the Unit Under Test (UUT)
	bram uut (
		.clka(enable), 
		.wea(wea), 
		.addra(addra), 
		.dina(dina), 
		.douta(douta)
	);

	always @(posedge enable) begin
		//LED <= douta;
		LED <= {douta[0],douta[1],douta[2],douta[3],douta[4],douta[5],douta[6],douta[7]};
		if (!Switch[0])
			addra <= 0;
		else
			addra <= addra + 1;
	end

// On every rising edge of enable check for the Push Button input.
/*
always @(posedge enable)
   begin
     LED <= !Switch[0] ? 8'd1   :  
	         !Switch[1] ? 8'd2   :
				!Switch[2] ? 8'd4   :
				!Switch[3] ? 8'd8   :
				!Switch[4] ? 8'd16  :
				!Switch[5] ? 8'd32  :
				!Switch[6] ? 8'd64  :
				{LED[0],LED[7:1]};
   end
	*/
endmodule
