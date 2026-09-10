module decodeUnit (
    input wire [15:0] instruction;
);

wire opcode[4:0];

assign opcode = instruction[15:10];

wire normal_field_1[4:0];
wire normal_field_2[4:0];
wire normal_field_3[4:0];
wire two_field_a[4:0];
wire two_field_b[4:0];

assign normal_field_1 = {1'b0, instruction[9:6]};
assign normal_field_2 = {2'b00, instruction[5:3]};
assign normal_field_3 = {2'b00, instruction[2,0]};
assign two_field_a = instruction[9:5];
assign two_field_b = instruction[4:0];



endmodule