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
    output reg [7:0]LED,
	 
	 output [2:0] debug_state,
	 output [7:0] debug_opcode,
	 output [7:0] debug_mode,
	 output [7:0] debug_reg0,
	 output [7:0] debug_reg1,
	 output [31:0] debug_raw2,
	 output debug_enable
    );

// Register used internally	
reg [24:0]count = 0;
reg enable = 0;

// Provide the initial value
initial 
  begin
    LED = 8'h01;
  end
  
// Scale down the clock so that output is easily visible.
always @(posedge Clk) begin
  count <= count+1'b1;
  //enable <= count[24];
  enable <= count[1];
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
		/*
		if (!Switch[0])
			addra <= 0;
		else
			addra <= addra + 1;
			*/
	end
	
	/*
	 * CPU State.
	 */
	parameter R_SP = 11;
	parameter R_BA = 12;
	parameter R_FL = 13;
	
	parameter FL_N = 32'b0001; // Negative
	parameter FL_Z = 32'b0010; // Zero
	parameter FL_V = 32'b0100; // Overflow
	parameter FL_C = 32'b1000; // Carry
	
	reg [31:0] r [0:15];
/*	= {
		32'h0,32'h0,32'h0,32'h0,32'h0,32'h0,32'h0,32'h0,
		32'h0,32'h0,32'h0,32'h0,32'h0,32'h0,32'h0,32'h0
	};
	*/
	
	initial begin
		r[0] = 0;
		r[1] = 0;
		r[2] = 0;
		r[3] = 0;
		r[4] = 0;
		r[5] = 0;
		r[6] = 0;
		r[7] = 0;
		r[8] = 0;
		r[9] = 0;
		r[10] = 0;
		r[11] = 0;
		r[12] = 0;
		r[13] = 0;
		r[14] = 0;
		r[15] = 0;
	end
	
	reg [31:0] PC = 0;
	//reg [31:0] nextPC = 0;
	
	/*
	 * Instruction and components.
	 */
	reg [7:0] inst [0:7];
	initial begin
		inst[0] = 0;
		inst[1] = 0;
		inst[2] = 0;
		inst[3] = 0;
		inst[4] = 0;
		inst[5] = 0;
		inst[6] = 0;
		inst[7] = 0;
	end
	wire [7:0] opcode, mode, reg0, reg1;
	wire [31:0] raw2;
	
	assign {opcode, mode, reg0, reg1} = {inst[7], inst[6], inst[5], inst[4]};
	assign raw2 = {inst[0],inst[1],inst[2],inst[3]};
	
	parameter I_ADD = 8'b00000001;
	parameter I_SUB = 8'b00000010;
	
	parameter I_MOV = 8'b00001011;
	
	/*
	 * Decoded operands.
	 */
	 reg [31:0] opr0, opr1, opr2;

	/*
	 * CPU state machine.
	 */
	parameter STATE_FETCH = 3'h0;
	parameter STATE_DECODE = 3'h1;
	parameter STATE_EXECUTE = 3'h2;
	reg [2:0] state = STATE_FETCH;
	
	reg [3:0] bytes_left = 0;
	
	always @(posedge enable) begin
		case (state)
			STATE_FETCH : begin
				inst[bytes_left - 1] <= douta;
				addra <= PC;
				PC <= PC + 1;
				bytes_left <= bytes_left - 1;
				if (bytes_left == 1) begin
					bytes_left <= 8;
					state <= STATE_DECODE;
				end
			end
			
			STATE_DECODE : begin
				opr0 <= r[reg0[3:0]];
				opr1 <= r[reg1[3:0]];
				if (mode[0])
					opr2 <= r[raw2[3:0]];
				else
					opr2 <= raw2;
				state <= STATE_EXECUTE;
			end
			
			STATE_EXECUTE : begin
				case(opcode)
					I_ADD : begin
						{r[R_FL], r[reg0]} = opr1 + opr2;
					end
					I_MOV : begin
						r[reg0] = opr2;
					end
				endcase
				state <= STATE_DECODE;
			end
		endcase
	end

	assign debug_state = state;
	assign debug_opcode = opcode;
	assign debug_mode = mode;
	assign debug_reg0 = reg0;
	assign debug_reg1 = reg1;
	assign debug_raw2 = raw2;
	assign debug_enable = enable;

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
