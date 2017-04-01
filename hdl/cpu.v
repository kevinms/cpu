`timescale 1ns / 1ps

module waxwing_test1(
	input Clk,
	input [6:0]Switch,
	output reg [7:0]LED
);

	initial begin
		LED = 8'h01;
	end
	
	localparam d = 0;
	always @(posedge enable) begin
		//LED <= {r[d][0],r[d][1],r[d][2],r[d][3],r[d][4],r[d][5],r[d][6],r[d][7]};
		LED <= {
			mmap_reg[d][0],mmap_reg[d][1],mmap_reg[d][2],mmap_reg[d][3],
			mmap_reg[d][4],mmap_reg[d][5],mmap_reg[d][6],mmap_reg[d][7]};
	end
  
	/*
	 * Create a signal called enable with double the period of Clk.
	 * Clk is used for bram. 'enable' is used for the instruction
	 * FSM. This will allow the bram unit to get values set between
	 * instruction states. E.g. we set an address in STATE_DECODE
	 * and by the time STATE_EXECUTE starts the bram module has
	 * already used the address for either a read or write.
	 */
	reg enable = 0;
	always @(posedge Clk) begin
		enable <= ~enable;
	end

	// Inputs
	reg [0:0] wea = 0;
	reg [7:0] dina = 0;
	reg [15:0] addr = 0;

	// Outputs
	wire [7:0] douta;

	// Instantiate the Unit Under Test (UUT)
	bram uut (
		.clka(Clk), 
		.wea(wea), 
		.addra(addr), 
		.dina(dina), 
		.douta(douta)
	);
	/*
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
	spi_master spi (
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
	*/
	
	/*
	 * CPU State.
	 */
	localparam R_SP = 11;
	localparam R_BA = 12;
	localparam R_FL = 13;
	
	localparam FL_N = 32'b0001; // Negative
	localparam FL_Z = 32'b0010; // Zero
	localparam FL_V = 32'b0100; // Overflow
	localparam FL_C = 32'b1000; // Carry
	
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
	reg [7:0] opcode = 0, mode = 0, reg0 = 0, reg1 = 0;
	reg [31:0] raw2 = 0;
	
	localparam I_NOP = 8'b00000000;
	
	localparam I_ADD = 8'b00000001;
	localparam I_SUB = 8'b00000010;
	localparam I_ADC = 8'b00000011;
	localparam I_SBC = 8'b00000100;
	localparam I_MUL = 8'b00000101;
	localparam I_DIV = 8'b00000110;
	
	localparam I_LDW = 8'b00000111;
	localparam I_LDB = 8'b00001000;
	localparam I_STW = 8'b00001001;
	localparam I_STB = 8'b00001010;
	
	localparam I_MOV = 8'b00001011;
	
	localparam I_AND = 8'b00001100;
	localparam I_OR  = 8'b00001101;
	localparam I_XOR = 8'b00001110;
	localparam I_NOR = 8'b00001111;
	localparam I_LSL = 8'b00010000;
	localparam I_LSR = 8'b00010001;
	
	localparam I_CMP = 8'b00010010;
	localparam I_JMP = 8'b00010011;
	localparam I_JZ  = 8'b00010100;
	localparam I_JNZ = 8'b00010101;
	localparam I_JL  = 8'b00010110;
	localparam I_JGE = 8'b00010111;
	
	localparam I_DIE = 8'b11111111;
	
	/*
	 * Decoded operands.
	 */
	reg [31:0] opr0, opr1, opr2, jmp_addr, pc_addr, st_addr, ld_addr;
	
	/*
	 * Memory mapped IO state.
	 */
	localparam MMAP_START = 32'h2000;
	localparam MMAP_END = 32'h2004;
	localparam I_MMAP = 8'b11111110;
	
	reg [31:0] mmap_reg [0:3];
	reg [7:0] mmap_opcode = 0;
	
	initial begin
		mmap_reg[0] = 0;
		mmap_reg[1] = 0;
		mmap_reg[2] = 0;
		mmap_reg[3] = 0;
	end

	/*
	 * CPU state machine.
	 */
	localparam STATE_RESET = 3'h0;
	localparam STATE_FETCH = 3'h1;
	localparam STATE_DECODE = 3'h2;
	localparam STATE_EXECUTE = 3'h3;
	localparam STATE_HALT = 3'h4;
	
	reg [2:0] state = STATE_RESET;
	reg [3:0] bytes_left = 8;
	
	always @(posedge enable) begin
		case (state)
			STATE_RESET : begin
				PC <= 0;
				addr <= 0;
				bytes_left <= 8;
				state <= STATE_FETCH;
			end
			
			STATE_FETCH : begin
				//inst[bytes_left - 4'b1] <= douta;
				//PC <= PC + 1;
				case (bytes_left)
					4'h8 : opcode <= douta;
					4'h7 : mode <= douta;
					4'h6 : begin
						reg0 <= douta;
						opr0 <= r[douta[3:0]];
					end
					4'h5 : begin
						reg1 <= douta;
						opr1 <= r[douta[3:0]];
					end
					4'h4 : raw2[7:0] <= douta;
					4'h3 : raw2[15:8] <= douta;
					4'h2 : raw2[23:16] <= douta;
					4'h1 : begin
						raw2[31:24] <= douta;
						opr2 <= mode[0] ? r[raw2[3:0]] : {douta, raw2[23:0]};
						st_addr <= (mode[1] == 0) ? opr0 + r[R_BA] : opr0;
						/*
						 * This looks pretty nasty, but it's really just:
						 *
						 *     ld_addr <= (mode[1] == 0) ? r[R_BA] + opr2 : opr2;
						 */
						ld_addr <= (mode[1] == 0)
							? r[R_BA] + (mode[0] ? r[raw2[3:0]] : {douta, raw2[23:0]})
							: (mode[0] ? r[raw2[3:0]] : {douta, raw2[23:0]});
					end
				endcase
				addr <= addr[15:0] + 16'b1;
				bytes_left <= bytes_left - 4'b1;
				if (bytes_left == 1) begin
					state <= STATE_DECODE;
				end
			end
			
			STATE_DECODE : begin
				/*
				 * Set jmp_addr early in case this is a jump opcode.
				 */
				jmp_addr <= (mode[1] == 0) ? r[R_BA] + raw2 : raw2;
				
				/*
				 * Setup memory access early for load / store.
				 */
				pc_addr <= addr;
				if (opcode == I_LDB || opcode == I_LDW) begin
					if ((ld_addr >= MMAP_START) && (ld_addr < MMAP_END)) begin
						mmap_opcode <= opcode;
						opcode <= I_MMAP;
						addr <= ld_addr - MMAP_START;
					end else begin
						addr <= ld_addr;
						if (opcode == I_LDW)
							bytes_left <= 4'h4;
					end
				end
				if (opcode == I_STB || opcode == I_STW) begin
					if ((st_addr >= MMAP_START) && (st_addr < MMAP_END)) begin
						mmap_opcode <= opcode;
						opcode <= I_MMAP;
						addr <= st_addr - MMAP_START;
					end else begin
						addr <= st_addr;
						dina <= opr2;
						wea <= 1'b1;
						if (opcode == I_STW)
							bytes_left <= 4'h4;
					end
				end
				
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
					 * Moving data around.
					 */
					I_MOV : r[reg0] <= opr2;
					I_LDB : r[reg0][7:0] <= douta;
					I_LDW : begin
						case (bytes_left)
							4'h4 : r[reg0][31:24] <= douta;
							4'h3 : r[reg0][23:16] <= douta;
							4'h2 : r[reg0][15:8] <= douta;
							4'h1 : r[reg0][7:0] <= douta;
						endcase
						addr <= addr[15:0] + 16'b1;
						bytes_left <= bytes_left - 4'b1;
					end
					
					I_STB : begin
						/*
						 * I_STB takes place in the decode state.
						 * The execute state is just to clock out the data.
						 */
					end
					I_STW : begin
						case (bytes_left)
							4'h4 : dina <= opr2[15:8];
							4'h3 : dina <= opr2[23:16];
							4'h2 : dina <= opr2[31:24];
						endcase
						addr <= addr[15:0] + 16'b1;
						bytes_left <= bytes_left - 4'b1;
					end
					
					/*
					 * Memory mapped IO.
					 */
					I_MMAP : begin
						case (mmap_opcode)
							I_STB : mmap_reg[addr] <= opr2[7:0];
							I_STW : mmap_reg[addr] <= opr2;
							I_LDB : r[reg0][7:0] <= mmap_reg[addr];
							I_LDW : r[reg0] <= mmap_reg[addr];
						endcase
					end

					/*
					 * Bitwise operations.
					 */
					I_AND : r[reg0] <= opr1 & opr2;
					I_OR : r[reg0] <= opr1 | opr2;
					I_XOR : r[reg0] <= opr1 ^ opr2;
					I_NOR : r[reg0] <= ~opr1 & ~opr2;
					I_LSL : r[reg0] <= opr1 << opr2;
					I_LSR : r[reg0] <= opr1 >> opr2;
					
					/*
					 * Branches and jumps.
					 */
					I_CMP : begin
						r[R_FL][FL_Z] <= (opr0 - opr2) == 0;
						r[R_FL][FL_C] <= opr0 < opr2;
					end
					
					I_JMP : addr <= jmp_addr;
					I_JZ : begin
						if (r[R_FL][FL_Z] != 0)
							addr <= jmp_addr;
					end
					I_JNZ : begin
						if (r[R_FL][FL_Z] == 0)
								addr <= jmp_addr;
					end
					I_JL : begin
						if (r[R_FL][FL_C] != 0)
								addr <= jmp_addr;
					end
					I_JGE : begin
						if ((r[R_FL][FL_C] == 0) || (r[R_FL][FL_Z] != 0))
								addr <= jmp_addr;
					end
					
					/*
					 * Halt, but don't catch fire.
					 */
					I_DIE : state <= STATE_HALT;
					
					/*
					 * Invalid instruction.
					 */
					default : begin
						//TODO: Trigger interrupt.
						state <= STATE_HALT;
					end
				endcase
				
				/*
				 * Transition to STATE_FETCH except when:
				 *   1. Opcode is I_DIE
				 *   2. Multibyte load or store
				 */
				if (opcode != I_DIE && bytes_left < 2) begin
					if (opcode == I_STB || opcode == I_STW ||
						 opcode == I_LDB || opcode == I_LDW ||
						 opcode == I_MMAP) begin
						/*
						 * Load and store instructions overwrite the addr,
						 * set it back to the PC.
						 */
						addr <= pc_addr;
						wea <= 1'b0;
					end
					state <= STATE_FETCH;
					bytes_left <= 8;
				end
			end
		endcase
		
		/*
		 * Press and hold a switch to reset.
		 */
		//if (!Switch[0])
		//	state <= STATE_RESET;
	end

endmodule
