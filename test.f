#!./myforth
INCLUDE "functions.txt"
INLCUDE "conditions.txt"
." Reading in Library FILE functions.txt " CR
CR ." ********************* TESING THE STACK ******************************************" CR
." Pushing numbers on the stack.." CR
2 3 4 5
." STACK = " .s CR
+
." Adding first set." CR
." STACK = " .s CR
+
." Adding second set." CR
." STACK = " .s CR
+
." Adding last stack instance."
." STACK = " .s CR
." Calling function SQUARE." CR
SQUARE
." Process Stack Complete. OUTPUT IS: "
.s CR CR
clear
." *************  TESTING CONDITIONS IF ELSE THEN **********************." CR
." Testing function IFELSE90 " CR
." Pushing 88 onto the stack." CR
88
." Calling IFELSE90 where the stack contains 88" CR
IFELSE90
CR
." PUSHING 101 onto the stack." CR
101
." CALLING FUNCTION" CR
IFELSE90
CR
." ************************* TESTING VARIABLE *****************************" CR
." Creating variable NN" CR
variable nn
." STORING 12 IN VARIABLE NN" CR
12 nn !
." VARIABLE NN CONTAINS " 
NN @
.s
." Collected from the STACK. "
CR
." ************************ TESTING LOOP ***********************************" CR
CLEAR
: TESTDO 10 0 DO CR ." Hello " LOOP ;
TESTDO
CR
." END OF TEST" CR
BYE
