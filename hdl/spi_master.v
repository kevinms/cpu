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
	output reg       busy,
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

	reg [7:0] din_reg;
	reg [2:0] count;
	
	/*
	 * Shift data in and out on SCK edge.
	 */
	always @(negedge sck) begin
		mosi <= dout[7];
		dout <= {dout[6:0], dout[7]};
	end
	always @(posedge sck) begin
		din_reg <= {din_reg[6:0], miso};
	end
	
	/*
	 * Enable and disable data transfer.
	 */
	localparam
		Reset = 2'h0,
		Idle = 2'h1,
		Busy = 2'h2;
	reg [1:0] state;
	
	always @(posedge clk) begin
		case (state)
			Reset : begin
				sck <= cpol;
				busy <= 1'b0;
				count <= 8;
			end
			
			Idle : begin
				if (write) begin
					busy <= 1'b1;
					state <= Busy;
					din_reg <= din;
				end
			end
			
			Busy : begin
				if (count) begin
					sck <= ~sck;
				end else begin
					busy <= 1'b0;
					state <= Idle;
				end
			end
		endcase
	end

endmodule
