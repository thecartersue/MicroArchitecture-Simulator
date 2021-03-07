   .ORIG x3000
   ADD R0, R0, #-1
   ADD R0, R0, #2
   BRp BLANK
   AND R0, R0, #1
   AND R1, R1, #1
   AND R2, R2, #-1
   BLANK ADD R5, R5, #1
   LSHF R0, R0, #3
   RSHFL R0, R0, #1
   RSHFA R0, R0, #1
   HALT
   .END