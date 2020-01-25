#include<stdio.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<search.h>
 
// Debugging variable, max 10 , the higher the number the more verbose the debug messaging is
// This can be dynamically turned on or off from the command line 
int DEBUG = 0; int DEBUG_LAYER = 0; // 1 = PRIM, 2 = SYM , 3 = STORE, 4 = lookup, 5 = branch, 6 = EVAL , 7 = PARSE, 8 = READ

int interact = 1;
int inquote = 0;
int seeflag = 0;
int ifflag = 0;
int loopflag = 0;
int questionflag = 0;

#define STACK_SIZE 1024
#define WORD_MAX 500
#define MAX_PROG_LEN 65535
#define MAX_STR_LEN 1024
// This will be the program memory array 
char progmem[MAX_PROG_LEN][STACK_SIZE];

int linenum = 0;
int compilemode = 0;
typedef enum { WORD_T, SYM_T, VAR_T } types;

// this is our list of primitive function tokens that will be used to dispatch to functions

typedef enum { ADD = 1, SUB, MUL, DIV, PSH, POP, DUP, DRP, BYE, HLT, WRD, DON, DOF, PPG,
               DSM, SWP, CRT, CLR, MOD, EQL, GTN, LTN, SEE, VAR, STO, RVR, IF, NEQ, ELS, 
               DO, NEG, I, QST, LIB, ROT, OVR, NIP, TCK, EXT, SYS } tokens;

char *banner = "SUBFORTH 1.0 - COMPILED JAN 2020 - Developed by rhale";

// Forth employs a Primitive wordlist in which we will pair with our token list.
// These primitives are basically built in functions.

char *words[] = { "ZERO_IDX", "+", "-", "*", "/", "PUSH", "POP", "DUP", "DROP", "BYE",
                  "HALT", "WORDS", "DEBUG", "DEBUGOFF", "PRINTPROG", "DUMPSYM", "SWAP",
                  "CR", "CLEAR", "MOD", "=", ">", "<", "SEE", "VARIABLE", "!", "@", "IF", "<>", "ELSE",
                  "DO", "NEGATE", "I", "?", "INCLUDE", "ROT", "OVER", "NIP", "TUCK", "EXIT", "SYSTEM","", 0 };

#define MAX_NAME_LEN 500

// The symtable will consist of 2 arrays. One will keep name and the other will keep function
// Why not a hash? Because FORTH allows reference to the most recent definition of a word 
// while still allowing the older definition. The newer definition can be erased with FORGET..
#define MAX_SYM 10000
#define MAX_NAME_LEN 500

int symcount = 0;
char symnames[MAX_SYM][MAX_NAME_LEN];
char symvals[MAX_SYM][MAX_NAME_LEN];
int symtype[MAX_SYM];

char varstack[MAX_SYM][MAX_NAME_LEN];

#define BUCKETS 2069
int hashstring(char *s){
int i, n, z; i = n = z = 0;
 while(*s != '\0'){ n = (int)*s++ + i; z = z + n; }
 return z % BUCKETS;
}
 
typedef enum { OK , SYNTAX_ERROR} errors; // these are some state id's
typedef enum { STOPPED, RUNNING, RESET } states;

// This is the Interpreter structure that will keep track of states, stack, stack counter, etc
// The data stack is wrapped in the interp struct.

typedef struct {
   int stack[STACK_SIZE]; 
   char * string; 
   int ip;
   int sp;
   int error;
   int state;
   int sv;
   int sc;
   int stype;
   int r0;
} interp; 

// Will have to declare forward declarations for functions that appear out of order.

void read(interp *sf, char *s);
void print_stack(interp *sf);
void print_prog();
void destroy_sf(interp *sf);
int drop(interp *sf);
void print_words();
void parsespace (interp *sf);
int pop(interp *sf);
char *strtoupper(char *s);
int push(interp *sf, int n);
void dumpsym(interp *sf);
void readlib(interp *sf, char *filename);

// This is the interpreter init function in which sets all values initially
interp *init_sf(){
 interp *sf = malloc(sizeof(interp));
 sf->state = RUNNING;
 sf->error = OK;
 sf->ip = -1;
 sf->sp = 0;
 sf->sv = 0;
 sf->sc = 0;
 sf->r0 = 0;
 return sf;
}

// this will reset the interp
interp *reset_sf(interp *sf){
 sf->state = 99;
 sf->error = OK;
 sf->ip = -1;
 sf->sp = 0;
 sf->sv = 0;
 sf->sc = 0;
 sf->r0 = 0;
 return sf;
}

/* ******************************* CORE stack functions. ******************************************** */

void addsymname(char *name, char *value){
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("ADDSYMNAME CALLED\n");
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("ADDSYMNAME GOT VALUES 'name=%s' and value='%s'\n", name, value);
 if(isalpha(*name) ){
 strcpy(symnames[symcount], name);
 strcpy(symvals[symcount], value);
 symtype[symcount] = SYM_T;
 symcount++;
 }
}

void updatesym(char *name, char *value){
 int i;
 name = strtoupper(name); 
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("UPDATESYM: GOT '%s', '%s'\n", name, value);
for(i = 0; i <=symcount; i++){
  if(strcmp(symnames[i], name) == 0 && isalpha(*name)) {
     strcpy(symvals[i], value);
   }
 }
}
char * findsym(interp *sf, char *name){
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("FINDSYM called on name='%s'\n", name);
int i;
int idx = 0;
int found = 0;
 char *result = NULL;
name[strcspn(name, " ")] = 0;
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("FINDSYM sf->string='%s'\n", sf->string);
for(i = 0; i < symcount; i++){
 if(strcmp(symnames[i], name) == 0) {
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("FINDSYM FOUND: '%s'\n", symvals[i]);
 found = 1; idx = i;
   result = symvals[i];
  }
 }
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("FINDSYM after loop sf->string='%s'\n", sf->string);
    sf->stype = symtype[idx];
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("UPDATING VARSTACK\n");
   strcpy(varstack[0],name);
  if(found == 0) result = "FINDSYM_NIL";
 return result;
}

void variable(interp *sf){ // ADD A VARIABLE NAME TO THE SYMTABLE
 symtype[symcount++] = VAR_T;
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("variable called symtype: %d\n", symtype[symcount]);
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("VARIABLE: sf->string: '%s'\n", sf->string);
 parsespace(sf);
 char varname[500];
 int n = 0;
  char *s;
 //while(*sf->string != '\0'){  // advance past variable command sf->string++;n++;}
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("VARIABLE ADVANCED to: '%s'\n", sf->string);
 parsespace(sf);
 n = 0;
while(*sf->string != '\0'){
     varname[n] = *sf->string;
     sf->string++;
     n++;
 }
 varname[n] ='\0';
 varname[strcspn(varname, " ")] = 0;
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("GOT %s\n", varname);
 strtoupper(varname);  // send the command to the table and the rest of the string as definition
 if(isalpha(*varname))addsymname(varname, "");
}

void readvar(interp *sf, char *value){
    findsym(sf, value);     
}

void question(interp *sf, char *s){
 questionflag = 1;
 printf("%s", findsym(sf, s));
 sf->string++;
}

void store(interp *sf, char *varname){
 if(DEBUG>=3 || DEBUG_LAYER == 3 ) printf("STORE CALLED ON: %s\n", varname);
 sf->string++; // eat the !
 char buffer[MAX_STR_LEN];
 char *value = NULL;
  int a = pop(sf);   // pop the stack
parsespace(sf);
int n = 0;
 value = buffer;
 sprintf(value, "%d", a);
updatesym(varname, value);
}

// This function looks up a word in the word list and returns the token number if found.
int lookup(char *s){
  int tok = 0;
 if(DEBUG>=4 || DEBUG_LAYER == 4 ) printf("LOOKUP CALLED: looking for '%s'\n", s);
  int i;
  i = 0;
 while(strcmp(words[i], "") != 0){
   if(strcmp(words[i], s) == 0) {  tok = i; }
   i++;
  }
 return tok;
}

int matchExpr(char *s, char *match){
 if(DEBUG>=9 || DEBUG_LAYER == 9 ) printf("LOOKUP CALLED: looking for '%s'\n", s);
   if(*s != *match) return 0;
    while(*s == *match){
      s++; match++;
      }
    if(*match == '\0') return 1;
    return 0;
}

void branch_else(interp *sf){
 if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("ELSE CALLED ON : '%s'\n", sf->string);
 sf->string+=4; // get past the ELSE word
  parsespace(sf);
  read(sf, sf->string);
 sf->string = "";  
}

void branch_if(interp *sf){
   ifflag = 0; // ensure if flag is zero 
   int elseflag = 0;
 if(strstr(sf->string, "else") || strstr(sf->string, "ELSE") ) { elseflag = 1; }
   strtoupper(sf->string);
if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("IF BRANCH DETECTED. BRANCHED TO IF: '%s'\n", sf->string);
 int a = 0; // pop the stack to determine output of the last command
 a = pop(sf);
  if(a == 0) { 
   ifflag = 1; 
   if(elseflag) {
     while(!matchExpr(sf->string, "ELSE")){  // skip up to else
        sf->string++;
     }
    branch_else(sf);    
    }
   sf->string = ""; 
    if(DEBUG>=5 || DEBUG_LAYER == 5 )printf ("RETURNING AS DETECTED 0 sfstring: '%s'\n", sf->string); 
   return; 
 }
   parsespace(sf); 
 if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("STACK VALUE IS: %d\n", a);
 char expr[MAX_STR_LEN];
  int i = 0; int m = 0;
   if(DEBUG>=5 || DEBUG_LAYER == 5 )  printf("PARSING UP TO END OF STRING.\n");
  if(elseflag){  // Parse up to else only otherwise parse to end of string
      while(!matchExpr(sf->string, "ELSE")){  // capture up to else
         expr[i] = *sf->string;
          sf->string++;i++;      
     }
  }
   else {
 while(*sf->string != '\0'){
      expr[i] = *sf->string;
      sf->string++;i++;
     } 
     sf->string--; // put back char
    } 
  expr[i] ='\0';
 if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("STACK VALUE FOR EXPR COMPARE: %d and EXPR: '%s' sf->string: '%s'\n", a, expr, sf->string);
      if(a == 0) { return;}
    else{ 
      read(sf, expr); 
      sf->string = "";
      }
}

void branch_do(interp *sf){
if(DEBUG>=5 || DEBUG_LAYER == 5 )printf("***BRANCHED TO DO: '%s'\n", sf->string);
int a, b;
 a = b = loopflag = 0;
// pop the two values off the stack
 a = pop(sf);
 b = pop(sf);
 if(strstr(sf->string, "LOOP") || strstr(sf->string, "loop") ){
   loopflag = 1;
 }

char expr[MAX_STR_LEN];
if(!loopflag  ){
  printf("ERROR: EXPECTED LOOP ;\n");
 } 
   int i = 0;
 while(!matchExpr(sf->string, "LOOP")){
   expr[i] = *sf->string;
    sf->string++; i++;
  }
  expr[i] = '\0';
int n;
 for(n = a; n <= b; n++){
     sf->r0 = n;
     push(sf, n);
     read(sf, expr);
     pop(sf);
  }
sf->string = "";
}

// will get rid whitespace when parsing the input
void parsespace (interp *sf){
if(DEBUG>=8 || DEBUG_LAYER == 8 ) printf("PARSESPACE CALLED\n");
 char *s = sf->string;
  while(*s == ' ' || *s == '\t' || *s == '\n'){
    s++;
   }
 sf->string = s;  // update the parser
}

char *strtoupper(char *s){   // converts string to upper case
if(DEBUG>=8 || DEBUG_LAYER == 8 ) printf("STRTOUPPER CALLED on string '%s'\n", s);
 int i;
 int quote = 0;
 for (i = 0; s[i]!='\0'; i++) {
      if( s[i] == '"'){ i++; while(s[i] != '"'){ i++; } } 
      if(s[i] >= 'a' && s[i] <= 'z') {
         s[i] = s[i] -32;
      }
   }
 return s;
}

int stack_empty(interp *sf){   // returns if the stack is empty or not.
 if(sf->sc == 0){
  return 1;
  }
 else { 
  return 0;
  }
}

void call_system(interp *sf){
 if(*sf->string == '"')sf->string++;
 if(strstr(sf->string, "\"")) sf->string[strlen(sf->string)-1] = 0;
  system(sf->string);
}
/* ******************************  PRIMITIVES ***************************************** */
int push(interp *sf, int n){ // pushes an int on the stack
 if(questionflag == 1) return sf->error = OK;
 sf->stack[sf->sp] = n;
 sf->sp++;
 sf->sc++;
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("pushed %d, sp=%d, sc=%d\n", n, sf->sp, sf->sc);
 return sf->error = OK; 
}

int pop(interp *sf){   // pops an int from the stack
 if(seeflag) return 0;
 if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("POP CALLED\n");
 if(sf->sp== 0){ 
   printf("ERROR: STACK UNDERFLOW\n");
  return 0;
 }
 sf->sp--;
 int c = sf->stack[sf->sp];
 sf->sc--;
 sf->sv = c;
 return c; 
}

void clear_stack(interp *sf){
 int i;
for (i = 0 ; i <= sf->sp; i++){
  drop(sf);
 }
 int sctemp = sf->sc;
 for (i = 0 ; i < sctemp; i++){
  drop(sf);
 }
  sf->sp = 0;
  sf->sc = 0;
}

int add(interp *sf){ // pop 2 items from the stack and add them
 int a, b, c;
 sf->sv = 0;
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("ADD CALLED\n");
  a = pop(sf);
  b = pop(sf);
  c = b + a;
  push(sf, c);
 sf->sv = c;
return sf->sv;
}

int sub(interp *sf){
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  c = b - a;
  push(sf, c);
  sf->sv = c;
return sf->sv;
}

int mul(interp *sf){
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  c = b * a;
  push(sf, c);
  sf->sv = c;
return sf->sv;
}

int drop(interp *sf){   // drops the last int from the top of stack
  int a = 0;
  a = pop(sf);
 sf->sv = sf->stack[sf->sp];
 return sf->sv;
}
int dup(interp *sf){     // duplicates the top of the stack
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("DUP CALLED \n"); 
 int a = 0;
 a = sf->stack[sf->sp - 1];
 push(sf, a);
 sf->sv = a;
 return sf->sv;
}

int swap(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("SWAP CALLED \n"); 
 int a, b;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  push(sf, a);
  push(sf, b);
  sf->sv = a;
return sf->sv;
}

int mod(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("MOD CALLED \n"); 
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  c = b % a;
  push(sf, c);
  sf->sv = c;
return sf->sv;
}

int equal(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("EQUAL CALLED \n"); 
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  c = b == a ? -1 : 0;
  push(sf, c);
  sf->sv = c;
return sf->sv;
}

int not_equal(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("NOT EQUAL CALLED \n"); 
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  c = b != a ? -1 : 0;
  push(sf, c);
  sf->sv = c;
return sf->sv;
}

int greater(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("GREATER THAN CALLED \n"); 
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  c = b > a ? -1 : 0;
  push(sf, c);
  sf->sv = c;
return sf->sv;
}

int less(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("LESS THAN CALLED \n"); 
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  b = pop(sf);
  c = b < a ? -1 : 0;
  push(sf, c);
  sf->sv = c;
return sf->sv;
}

int negate(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("NEGATE CALLED \n"); 
 int a, b, c;
 sf->sv = 0;
  a = pop(sf);
  a = a * -1;
  push(sf, a);
  sf->sv = a;
return sf->sv;
}

void rotate(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("ROTATE CALLED \n"); 
int a, b, c; // pop the last 3 elements from the stack and rotate them
a = pop(sf);
b = pop(sf);
c = pop(sf);
push(sf, b);
push(sf, a);
push(sf, c);
}

void over(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("OVER CALLED \n"); 
int a, b; // take the next to last stack element DUP and push
a = pop(sf);
b = pop(sf);
push(sf, b);
push(sf, a);
push(sf, b);
}

void nip(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("NIP CALLED \n"); 
int a, b; // removes the second from last
a = pop(sf);
b = pop(sf);
push(sf, a);
}

void tuck(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("TUCK CALLED \n"); 
int a, b, c; // take the last and push it 3 behind
a = pop(sf);
b = pop(sf);
c = pop(sf);
push(sf, c);
push(sf, a);
push(sf, b);
push(sf, a);
}

int see(interp *sf){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("SEE CALLED \n"); 
 char *buff; 
 parsespace(sf);
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("SEE CALLED ON '%s'\n", sf->string); 
 buff = findsym(sf, strtoupper(sf->string));   // if a word exists then print the definition
   if(strcmp(buff, "FINDSYM_NIL") != 0){
   printf("FOUND:\n: %s %s\n", sf->string, buff); 
  }
 sf->string = "";
 return 0; 
}
/* *********************************** EVAL *************************************** */
void eval(interp *sf, int op){ // evaluates the current token mapped by word
if(DEBUG>=6 || DEBUG_LAYER == 6 ) printf("EVAL CALLED\n"); 
 if(DEBUG>=6 || DEBUG_LAYER == 6 ) printf("OP DETECTED: '%s'\n", words[op]); 
 switch (op){   // perform the Operation
    case CRT: { 
        printf("\n"); break;
     }
    case ADD: {
      add(sf); 
      break;
     }
    case SUB: {
      sub(sf); 
      break;
     }
   case MUL: {
      mul(sf); 
      break;
     }
    case DUP: {
      dup(sf); 
      break;
     }
   case DRP: {
      drop(sf); 
      break;
     }
  case WRD: {
      print_words(); 
      break;     
    }
  case DON: {
      DEBUG = 9; 
      break;     
    }
  case DOF: {
      DEBUG = 0; 
      break;     
    }
  case PPG: {
      print_prog(); 
      break;     
    }
 case SWP: {
    swap(sf); 
    break;
    }
  case CLR: {
      clear_stack(sf); 
      break;     
    }
  case MOD: {
     mod(sf); 
     break;
    }
  case EQL: {
     equal(sf); 
     break;
    }
  case GTN: {
     greater(sf); 
     break;
    }
  case LTN: {
     less(sf); 
     break;
    }
  case SEE: {
     seeflag = 1; 
     see(sf); 
     break;
    }  
  case VAR: {
     variable(sf); 
     break;
    }
  case STO: {
     store(sf, varstack[0]); 
     break;
    }
  case RVR: {
       readvar(sf, varstack[0]); 
       break;
    }
  case IF: {
      branch_if(sf);
      break;
    }
  case ELS: {
      branch_else(sf); 
      break;
    }
  case NEQ: {
     not_equal(sf); 
     break;
   }
 case DSM: {
     dumpsym(sf); 
     break;
   } 
 case DO: {
     branch_do(sf); 
     break;
   } 
 case NEG: {
     negate(sf);
     break;
   } 
 case LIB: {            
    readlib(sf, sf->string); 
    break;
   } 
 case I: {
     printf("%d", sf->r0); 
     sf->string++;  
     break;
   } 
 case ROT: {
     rotate(sf); 
     break;
   } 
 case OVR: {
     over(sf); 
     break;
   } 
case NIP: {
     nip(sf); 
     break;
   } 
case TCK: {
     tuck(sf); 
     break;
   } 
 case QST: {
     break;
   }
 case SYS: {
    call_system(sf);
    break;   
 } 
  case EXT: { 
     op = BYE; 
   }
  case BYE: {
    destroy_sf(sf); 
    if(interact) printf("Exiting Interpreter..\n"); 
    exit(0); 
   }
  } // end switch
}
/* **************************   PARSEWORD ****************************************************** */
int parseword(interp *sf){              // Attempts to parse a word from the input
 if(! isprint(*sf->string)) {
  if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("ERROR: NON ASCII VALUE DETECTED.\n"); 
  sf->string = "";
  return 0;
  }
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("PARSEWORD CALLED\n");
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("PARSEWORD sf->string: '%s'\n", sf->string);
 int c = 0;
 char word[WORD_MAX]; 
  int i = 0;
  while(isprint(*sf->string) && *sf->string != ' '){
      word[i++] = *sf->string; 
      sf->string++;
  }
 parsespace(sf);
 word[i] = '\0';
 if(DEBUG>=7 || DEBUG_LAYER == 7 )printf("PARSEWORD AFTER '%s'\n", sf->string);
 strtoupper(word);  // convert the string to upper 
 int l = lookup(word);
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("LOOKUP FOUND: '%s:%d'\n", words[l], l );
 if(l > 0) {   // call eval with the command found
   if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("lookup op: %d\n", l);
   eval ( sf, l); 
  }
 else {    // if the word is not found in primatives then search the symtable
 char *hresult;
 hresult = findsym(sf, word);
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("parseword::hresult = '%s'\n", hresult);
 if(strstr(hresult, "FINDSYM_NIL") == 0){
  if(DEBUG>=7 || DEBUG_LAYER == 7 )  printf("CALLING READ with findsym data: '%s' sf->string = '%s'\n", hresult, sf->string);
  if(*sf->string == '?') { question(sf, word); }
   if(*sf->string == '!'){ store(sf, word); } else {
      read(sf, hresult);
      }
  }
 }
 memset(word, 0, sizeof(word));
 if(DEBUG>=7 || DEBUG_LAYER == 7 )printf("PARSEWORD RETURNING AND STRING LEFT '%s'\n", sf->string);
 return sf->state = OK;
}
/* **************************   PARSEDOT  ***************************************************** */
interp * parsedot(interp *sf){        // Parses an expression beginning with a DOT
 if(DEBUG > 8 || DEBUG_LAYER == 8) printf("PARSEDOT CALLED\n");
   sf->string++;
   if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("PARSEDOT S: %s\n", sf->string);
    if(*sf->string == 'S' || *sf->string == 's'){
     if(DEBUG > 8 || DEBUG_LAYER == 8 )  printf("FOUND COMMAND %c\n", *sf->string);
    print_stack(sf);
    } 
    else if(*sf->string == '"'){        // if we find a string
      sf->string++; 
        if(*sf->string != ' '){  printf("SYNTAX ERROR: <EXPECTED SPACE>\n");
              sf->string = ""; 
             return sf;
         }
          sf->string++;// eat the space
         while(*sf->string != '"'){
           printf("%c", *sf->string);
            sf->string++; 
         }        
    }
    else {
      int a = 0;
      a = pop(sf);
      printf("%d", a); 

     }
   sf->string++; 
  return sf;
}

/* **************************   PARSENUM ****************************************************** */
// parses an integer
interp *parsenum (interp *sf){// parse the current number and push on the stack
   if (DEBUG > 8 || DEBUG_LAYER == 8) printf("parsenum called on string: '%s'\n", sf->string);
  char *s = sf->string; 
  int n = 0;
  while(*s >= '0' && *s <= '9' && *s != ' ') {
    n = (n * 10) + (*s - '0');    
     s++;
   } 
  if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("parsenum  sf->string '%s'\n", sf->string);
  if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("PARSENUM:: collected n: '%d'\n", n);
  push(sf, n);
  sf->string = s;
  if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("parsenum returning sf->string '%s'\n", sf->string);
  if(*sf->string != '\0') read(sf, sf->string);
  return sf;
}

/* **************************   COMPILE MODE ****************************************************** */

interp *compile(interp *sf){ 
 if (DEBUG > 5 ) printf("compile called on string: '%s'\n", sf->string);
 compilemode = 1; 
 sf->string++; // strip the colon
 parsespace(sf);
 char cmdname[500];
 char cmdval[500];
 int n = 0;
  char *s;
 while(*sf->string != ' '){     // parse the name of the command
     cmdname[n] = *sf->string;
     sf->string++;
     n++;
 }
 cmdname[n] ='\0';

 parsespace(sf);
 n = 0;
while(*sf->string != ';'){
     cmdval[n] = *sf->string;
     sf->string++;
     n++;
 }
 cmdval[n] = '\0';
if(*sf->string == ';' ) compilemode = 0;
 // send the command to the table and the rest of the string as definition
 addsymname(cmdname, cmdval);
 sf->string++;
 return sf;
}

/* ************************** READ INPUT **************************************** */

void read(interp *sf, char *s){
 if(seeflag) return;

 if(DEBUG > 9 || DEBUG_LAYER == 9 ) printf("READ CALLED on '%s'\n", s);
 
   strcpy(progmem[linenum], s); // store the input in progmem
   linenum++;   
   sf->string = strdup(s);
   int i;

 while(*sf->string != '\0'){
   parsespace(sf);
   if(isdigit(*sf->string) ) {
     parsenum(sf);
    if(DEBUG > 9 || DEBUG_LAYER == 9) print_stack(sf);
   }
   else if(*sf->string == ':'){
     compile(sf);
   }
   else if(*sf->string == '.'){
     if(DEBUG > 9 || DEBUG_LAYER == 9) printf("DETECTED DOT\n");
     parsedot(sf);
   }
   else if(*sf->string == '?'){
      printf("READ CAUGHT QUESION ?\n");
    }
  else { parseword(sf); }
  parsespace(sf);
 }
 questionflag = 0;
}

/* ****************************** PRINT  ************************************************* */
// this will free the initial memory used by the sf
void destroy_sf(interp *sf){
 free(sf);
}

// This prints what is on the stack
void print_stack(interp *sf){
 int i;
 if(interact) printf("[ ");
 for (i = 0 ; i < sf->sp; i++){
   printf("%d ", sf->stack[i]);
  }
 if(interact) printf("]\n");
}

void dumpsym(interp *sf){
   int i;
    for(i = 0; i < symcount;i++){
       printf("%s => %s\n", symnames[i], symvals[i]);
    }
  printf("VARSTACK contains: '%s'\n", varstack[0]);
}

void print_prog(){
  int i;
 printf("-------------------------------------------------\n");
 for(i=0; i<linenum - 1;i++){
  printf("%s\n", progmem[i]);
  } 
 printf("-------------------------------------------------\n");
}

void print_words(){ // This function prints our Primitive built in functions
 if(DEBUG>=8) printf("PRINT_WORDS CALLED\n");
  int i, x;
  i = x = 0;
printf("\n");
  int found = 0;
 while(strcmp(words[i], "") != 0){
  if(i != 0)printf("%s  ",words[i]);
  if(x == 12){ printf("\n"); x = 0;}
   i++;x++;
  }
   x = 0;
  for( i = 0; i < symcount;i++){  // print user defined words
  printf("(U):%s  ", symnames[i]);
  if(x == 12){ printf("\n"); x = 0;}
   x++;
  }  
 printf("\n");
}

void readlib(interp *sf, char *filename){
   if(DEBUG >=8) printf("READLIB CALLED on '%s'\n", filename);
   char buffer[MAX_STR_LEN];
   if(*filename == '"')filename++;
    if(strstr(filename, "\"")) filename[strlen(filename)-1] = 0;
       FILE *lib = fopen(filename, "r");  
    if(lib == NULL) { printf("ERROR: Couldn't open file %s for READ\n", filename);  return; }
       while(fgets(buffer, MAX_STR_LEN, lib) != NULL){
         buffer[strcspn(buffer, "\n")] = 0;
         if(DEBUG >= 1 || DEBUG_LAYER == 1) printf("read: %s\n", buffer);
           read(sf, buffer);
           memset(buffer, 0 , MAX_STR_LEN);         
        }        
}

// Main loop, initialize the sf, get input and call the parser
int main(int argc, char *argv[]){
  interp *sf;
  sf = init_sf();
  char buffer[MAX_STR_LEN];
  //readlib(sf, "functions.txt");

 // check for interactive versus non interactive mode
 if(argc > 1) {
  if(strstr(argv[1], "-h")){ printf("Use: %s <FORTH_FILENAME.f>\n", argv[0]); exit(1);}  
   if(strstr(argv[1], ".f")){  
   interact = 0; 
  // If desired read an external list of user defined functions into the interpreter
      FILE *fp;
      fp = fopen(argv[1], "r");
        while(fgets(buffer, MAX_STR_LEN, fp) != NULL){
         buffer[strcspn(buffer, "\n")] = 0;
           //printf("read: %s\n", buffer);
           read(sf, buffer);
           memset(buffer, 0 , MAX_STR_LEN);         
        }        
     }
  }
 
 if(interact) printf("%s\n", banner);
  while(1){  // INTERACTIVE MODE REPL
     fgets(buffer, MAX_STR_LEN, stdin); 
       buffer[strcspn(buffer, "\n")] = 0; 
            seeflag = 0;
           read(sf, buffer);
           printf("\nOK\n");
        memset(buffer, 0 , MAX_STR_LEN);            
 }

return 0;
}

