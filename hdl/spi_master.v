`timescale 1ns / 1ps

module spi_master(
	/*
	 * SPI bus signals.
	 */
	output reg       sck,
	output reg       mosi,
	input            miso,
	output reg [7:0] ss,
	
	/*
	 * Master interface.
	 */
	input            clk,
	input            reset,
	input            write,
	output           busy,
	input      [7:0] din,
	output reg [7:0] dout,
	
	/*
	 * CPOL - Clock Polarity
	 *   0: SCK is low when bus is idle
	 *   1: SCK is high when bus is idle
	 */
	input cpol,
	
	/*
	 * CPHA - Clock Phase
	 *   0: Sample data on rising edge of SCK
	 *   1: Sample data on falling edge of SCK
	 */
	input cpha
);

	initial begin
		mosi = 0;
	end

	reg [7:0] din_reg;
	reg [3:0] count = 8;
	
	/*
	 * Shift data in and out on SCK edge.
	 */
	always @(negedge sck) begin
		mosi <= din_reg[7];
		din_reg <= {din_reg[6:0], din_reg[7]};
	end
	always @(posedge sck) begin
		dout <= {dout[6:0], miso};
	end
	
	/*
	 * Enable and disable data transfer.
	 */
	localparam
		Reset = 2'h0,
		Idle = 2'h1,
		Busy = 2'h2;
	reg [1:0] state = Reset;
	
	assign busy = (state == Busy);
	
	always @(posedge clk) begin
		case (state)
			Reset : begin
				sck <= cpol;
				count <= 8;
				dout <= 0;
				mosi <= 0;
				state <= Idle;
			end
			
			Idle : begin
				if (write) begin
					din_reg <= {din[6:0], din[7]};
					state <= Busy;
					mosi <= din[7];
				end
				if (reset) begin
					state <= Reset;
				end
			end
			
			Busy : begin
				sck <= ~sck;
				if (sck) begin
					count <= count - 1;
				end
				if (count == 0) begin
					sck <= cpol;
					count <= 8;
					state <= Idle;
				end
			end
		endcase
	end

endmodule

