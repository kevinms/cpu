`timescale 1ns / 1ps

module waxwing_test1(
	input Clk,
	input [6:0]Switch,
	output reg [7:0]LED
);

	reg [24:0]count = 0;
	reg enable = 0;

	initial begin
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
	reg [7:0] dina;

	// Outputs
	wire [7:0] douta;

	// Instantiate the Unit Under Test (UUT)
	bram uut (
		.clka(Clk), 
		.wea(wea), 
		.addra(PC[15:0]), 
		.dina(dina), 
		.douta(douta)
	);

	parameter d = 2;
	always @(posedge enable) begin
		LED <= {r[d][0],r[d][1],r[d][2],r[d][3],r[d][4],r[d][5],r[d][6],r[d][7]};
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
	wire [31:0] flag;
	
	assign flag = r[R_FL];
	
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
	
	parameter I_NOP = 8'b00000000;
	
	parameter I_ADD = 8'b00000001;
	parameter I_SUB = 8'b00000010;
	parameter I_ADC = 8'b00000011;
	parameter I_SBC = 8'b00000100;
	parameter I_MUL = 8'b00000101;
	parameter I_DIV = 8'b00000110;
	
	parameter I_LDB = 8'b00000111;
	parameter I_LDW = 8'b00001000;
	parameter I_STB = 8'b00001001;
	parameter I_STW = 8'b00001010;
	
	parameter I_MOV = 8'b00001011;
	
	parameter I_AND = 8'b00001100;
	parameter I_OR  = 8'b00001101;
	parameter I_XOR = 8'b00001110;
	parameter I_NOR = 8'b00001111;
	parameter I_LSL = 8'b00010000;
	parameter I_LSR = 8'b00010001;
	
	parameter I_CMP = 8'b00010010;
	parameter I_JMP = 8'b00010011;
	parameter I_JZ  = 8'b00010100;
	parameter I_JNZ = 8'b00010101;
	parameter I_JL  = 8'b00010110;
	parameter I_JGE = 8'b00010111;
	
	parameter I_DIE = 8'b11111111;
	
	/*
	 * Decoded operands.
	 */
	 reg [31:0] opr0, opr1, opr2, jmp_addr;

	/*
	 * CPU state machine.
	 */
	parameter STATE_RESET = 3'h0;
	parameter STATE_FETCH = 3'h1;
	parameter STATE_DECODE = 3'h2;
	parameter STATE_EXECUTE = 3'h3;
	parameter STATE_HALT = 3'h4;
	
	reg [2:0] state = STATE_RESET;
	
	reg [3:0] bytes_left = 8;
	
	always @(posedge enable) begin
		case (state)
			STATE_RESET : begin
				PC <= 0;
				bytes_left <= 8;
				state <= STATE_FETCH;
			end
			
			STATE_FETCH : begin
				inst[bytes_left - 1] <= douta;
				PC <= PC + 1;
				bytes_left <= bytes_left - 4'b1;
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
				jmp_addr <= (mode[1] == 0) ? r[R_BA] + raw2 : raw2;
				state <= STATE_EXECUTE;
			end
			
			STATE_EXECUTE : begin
				case(opcode)
					I_NOP : begin
					
					end
				
					/*
					 * Arithmetic operations.
					 */
					I_ADD : {r[R_FL][FL_C], r[reg0]} <= opr1 + opr2;
					I_SUB : r[reg0] <= opr1 - opr2;
					I_ADC : {r[R_FL], r[reg0]} <= opr1 + opr2 + flag[FL_C];
					I_SBC : r[reg0] <= opr1 - opr2;
					I_MUL : r[reg0] <= opr1 * opr2;
					I_DIV : r[reg0] <= opr1 / opr2;
					
					/*
					 * Loads and stores.
					 */
					I_MOV : r[reg0] <= opr2;
					
					/*
					 * Bitwise operations.
					 */
					I_AND : r[reg0] <= opr1 & opr2;
					I_OR : r[reg0] <= opr1 | opr2;
					I_XOR : r[reg0] <= opr1 ^ opr2;
					I_NOR : r[reg0] <= ~opr1 & ~opr2;
					I_LSL : r[reg0] <= {opr1[30:0], 1'b0};
					I_LSR : r[reg0] <= {1'b0, opr1[31:1]};
					
					/*
					 * Branches and jumps.
					 */
					I_CMP : begin
						r[R_FL][FL_Z] <= (r[reg0] - opr2) == 0;
						r[R_FL][FL_C] <= r[reg0] < opr2;
					end
					
					I_JMP : PC <= jmp_addr;
					I_JZ : begin
						if (r[R_FL][FL_Z] != 0)
							PC <= jmp_addr;
					end
					I_JNZ : begin
						if (r[R_FL][FL_Z] == 0)
								PC <= jmp_addr;
					end
					I_JL : begin
						if (r[R_FL][FL_C] != 0)
								PC <= jmp_addr;
					end
					I_JGE : begin
						if ((r[R_FL][FL_C] == 0) || (r[R_FL][FL_Z] != 0))
								PC <= jmp_addr;
					end
					
					/*
					 * Halt, but don't catch fire.
					 */
					I_DIE : state <= STATE_HALT;
				endcase
				
				if (opcode != I_DIE)
					state <= STATE_FETCH;
			end
		endcase
		
		/*
		 * Press and hold a switch to reset.
		 */
		if (!Switch[0])
			state <= STATE_RESET;
	end

endmodule
