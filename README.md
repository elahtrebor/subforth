# subforth
A simple interpreter that supports a subset of forth.
<pre>
Work in progress.

Supports words:

+  -  *  /  PUSH  POP  DUP  DROP  BYE  HALT  WORDS  DEBUG
DEBUGOFF  PRINTPROG  DUMPSYM  SWAP  CR  CLEAR  MOD  =  >  <  SEE  VARIABLE
!  @  IF  <>  ELSE  DO  NEGATE  I  ?  INCLUDE  ROT  OVER
NIP  TUCK  EXIT  SYSTEM

Here is a simple test drive output:

MYFORTH 1.0 - COMPILED JAN 2020 - Developed by rhale

variable myvar

OK
99 myvar !

OK
myvar ?
99
OK
.s
[ ]

OK
myvar @

OK
.s
[ 99 ]

OK
1 +

OK
.s
[ 100 ]

OK
: SQUARE DUP * ;

OK
SQUARE

OK
.s
[ 10000 ]

OK
clear

OK
.s
[ ]

OK
." hello world!"
hello world!
OK
: LOOP10 10 0 DO ." Processing " I . CR LOOP ;

OK
LOOP10
Processing 0
Processing 1
Processing 2
Processing 3
Processing 4
Processing 5
Processing 6
Processing 7
Processing 8
Processing 9
Processing 10

OK
system "ls"
a.out  conditions.txt  functions.txt  Makefile  myforth.c  test.f  todo.txt

OK
: IFELSE90 90 > IF ." GREATER THAN 90" ELSE ." LESS THAN 90" THEN ;

OK
88

OK
IFELSE90
LESS THAN 90
OK
99

OK
IFELSE90
GREATER THAN 90
OK
DUMPSYM
LOOP10 => 10 0 DO ." Processing " I . CR LOOP
IFELSE90 => 90 > IF ." GREATER THAN 90" ELSE ." LESS THAN 90" THEN
VARSTACK contains: 'IFELSE90'
1 2 3 4

OK
ROT

OK
.s
[ 1 3 4 2 ]

OK
+

OK
.s
[ 1 3 6 ]

OK
+

OK
.s
[ 1 9 ]

OK
+

OK
.s
[ 10 ]


</pre>
