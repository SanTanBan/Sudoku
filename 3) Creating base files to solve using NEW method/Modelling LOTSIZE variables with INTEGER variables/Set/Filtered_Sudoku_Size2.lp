\ Model Sudoku Problem
\ LP format - for model browsing. Use MPS format to capture full model detail.
Minimize
 
Subject To
 R0: x_1_1 + x_1_2 + x_1_3 + x_1_4 = 11
 R1: x_2_1 + x_2_2 + x_2_3 + x_2_4 = 11
 R2: x_3_1 + x_3_2 + x_3_3 + x_3_4 = 11
 R3: x_4_1 + x_4_2 + x_4_3 + x_4_4 = 11
 R4: x_1_1 + x_2_1 + x_3_1 + x_4_1 = 11
 R5: x_1_2 + x_2_2 + x_3_2 + x_4_2 = 11
 R6: x_1_3 + x_2_3 + x_3_3 + x_4_3 = 11
 R7: x_1_4 + x_2_4 + x_3_4 + x_4_4 = 11
 R8: x_1_1 + x_1_2 + x_2_1 + x_2_2 = 11
 R9: x_1_3 + x_1_4 + x_2_3 + x_2_4 = 11
 R10: x_3_1 + x_3_2 + x_4_1 + x_4_2 = 11
 R11: x_3_3 + x_3_4 + x_4_3 + x_4_4 = 11
 R12: x_3_1 = 1
 R13: x_3_3 = 0
 R14: x_4_4 = 7
Bounds
Generals
 x_1_1 x_1_2 x_1_3 x_1_4 x_2_1 x_2_2 x_2_3 x_2_4 x_3_1 x_3_2 x_3_3 x_3_4
 x_4_1 x_4_2 x_4_3 x_4_4
End
