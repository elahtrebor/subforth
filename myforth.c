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

char *banner = "MYFORTH 1.0 - COMPILED JAN 2020 - Developed by rhale";

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

// This is the Interpreter VM structure that will keep track of states, stack pointer, stack counter, etc
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

void read(interp *vm, char *s);
void print_stack(interp *vm);
void print_prog();
void destroy_vm(interp *vm);
int drop(interp *vm);
void print_words();
void parsespace (interp *vm);
int pop(interp *vm);
char *strtoupper(char *s);
int push(interp *vm, int n);
void dumpsym(interp *vm);
void readlib(interp *vm, char *filename);

// This is the VM init function in which sets all values initially
interp *init_vm(){
 interp *vm = malloc(sizeof(interp));
 vm->state = RUNNING;
 vm->error = OK;
 vm->ip = -1;
 vm->sp = 0;
 vm->sv = 0;
 vm->sc = 0;
 vm->r0 = 0;
 return vm;
}

// this will reset the VM
interp *reset_vm(interp *vm){
 vm->state = 99;
 vm->error = OK;
 vm->ip = -1;
 vm->sp = 0;
 vm->sv = 0;
 vm->sc = 0;
 vm->r0 = 0;
 return vm;
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
char * findsym(interp *vm, char *name){
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("FINDSYM called on name='%s'\n", name);
int i;
int idx = 0;
int found = 0;
 char *result = NULL;
name[strcspn(name, " ")] = 0;
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("FINDSYM vm->string='%s'\n", vm->string);
for(i = 0; i < symcount; i++){
 if(strcmp(symnames[i], name) == 0) {
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("FINDSYM FOUND: '%s'\n", symvals[i]);
 found = 1; idx = i;
   result = symvals[i];
  }
 }
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("FINDSYM after loop vm->string='%s'\n", vm->string);
    vm->stype = symtype[idx];
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("UPDATING VARSTACK\n");
   strcpy(varstack[0],name);
  if(found == 0) result = "FINDSYM_NIL";
 return result;
}

void variable(interp *vm){ // ADD A VARIABLE NAME TO THE SYMTABLE
 symtype[symcount++] = VAR_T;
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("variable called symtype: %d\n", symtype[symcount]);
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("VARIABLE: vm->string: '%s'\n", vm->string);
 parsespace(vm);
 char varname[500];
 int n = 0;
  char *s;
 //while(*vm->string != '\0'){  // advance past variable command vm->string++;n++;}
 if(DEBUG>=2 || DEBUG_LAYER == 2 ) printf("VARIABLE ADVANCED to: '%s'\n", vm->string);
 parsespace(vm);
 n = 0;
while(*vm->string != '\0'){
     varname[n] = *vm->string;
     vm->string++;
     n++;
 }
 varname[n] ='\0';
 varname[strcspn(varname, " ")] = 0;
 if(DEBUG>=2 || DEBUG_LAYER == 2 )  printf("GOT %s\n", varname);
 strtoupper(varname);  // send the command to the table and the rest of the string as definition
 if(isalpha(*varname))addsymname(varname, "");
}

void readvar(interp *vm, char *value){
    findsym(vm, value);     
}

void question(interp *vm, char *s){
 questionflag = 1;
 printf("%s", findsym(vm, s));
 vm->string++;
}

void store(interp *vm, char *varname){
 if(DEBUG>=3 || DEBUG_LAYER == 3 ) printf("STORE CALLED ON: %s\n", varname);
 vm->string++; // eat the !
 char buffer[MAX_STR_LEN];
 char *value = NULL;
  int a = pop(vm);   // pop the stack
parsespace(vm);
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

void branch_else(interp *vm){
 if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("ELSE CALLED ON : '%s'\n", vm->string);
 vm->string+=4; // get past the ELSE word
  parsespace(vm);
  read(vm, vm->string);
 vm->string = "";  
}

void branch_if(interp *vm){
   ifflag = 0; // ensure if flag is zero 
   int elseflag = 0;
 if(strstr(vm->string, "else") || strstr(vm->string, "ELSE") ) { elseflag = 1; }
   strtoupper(vm->string);
if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("IF BRANCH DETECTED. BRANCHED TO IF: '%s'\n", vm->string);
 int a = 0; // pop the stack to determine output of the last command
 a = pop(vm);
  if(a == 0) { 
   ifflag = 1; 
   if(elseflag) {
     while(!matchExpr(vm->string, "ELSE")){  // skip up to else
        vm->string++;
     }
    branch_else(vm);    
    }
   vm->string = ""; 
    if(DEBUG>=5 || DEBUG_LAYER == 5 )printf ("RETURNING AS DETECTED 0 vmstring: '%s'\n", vm->string); 
   return; 
 }
   parsespace(vm); 
 if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("STACK VALUE IS: %d\n", a);
 char expr[MAX_STR_LEN];
  int i = 0; int m = 0;
   if(DEBUG>=5 || DEBUG_LAYER == 5 )  printf("PARSING UP TO END OF STRING.\n");
  if(elseflag){  // Parse up to else only otherwise parse to end of string
      while(!matchExpr(vm->string, "ELSE")){  // capture up to else
         expr[i] = *vm->string;
          vm->string++;i++;      
     }
  }
   else {
 while(*vm->string != '\0'){
      expr[i] = *vm->string;
      vm->string++;i++;
     } 
     vm->string--; // put back char
    } 
  expr[i] ='\0';
 if(DEBUG>=5 || DEBUG_LAYER == 5 ) printf("STACK VALUE FOR EXPR COMPARE: %d and EXPR: '%s' vm->string: '%s'\n", a, expr, vm->string);
      if(a == 0) { return;}
    else{ 
      read(vm, expr); 
      vm->string = "";
      }
}

void branch_do(interp *vm){
if(DEBUG>=5 || DEBUG_LAYER == 5 )printf("***BRANCHED TO DO: '%s'\n", vm->string);
int a, b;
 a = b = loopflag = 0;
// pop the two values off the stack
 a = pop(vm);
 b = pop(vm);
 if(strstr(vm->string, "LOOP") || strstr(vm->string, "loop") ){
   loopflag = 1;
 }

char expr[MAX_STR_LEN];
if(!loopflag  ){
  printf("ERROR: EXPECTED LOOP ;\n");
 } 
   int i = 0;
 while(!matchExpr(vm->string, "LOOP")){
   expr[i] = *vm->string;
    vm->string++; i++;
  }
  expr[i] = '\0';
int n;
 for(n = a; n <= b; n++){
     vm->r0 = n;
     push(vm, n);
     read(vm, expr);
     pop(vm);
  }
vm->string = "";
}

// will get rid whitespace when parsing the input
void parsespace (interp *vm){
if(DEBUG>=8 || DEBUG_LAYER == 8 ) printf("PARSESPACE CALLED\n");
 char *s = vm->string;
  while(*s == ' ' || *s == '\t' || *s == '\n'){
    s++;
   }
 vm->string = s;  // update the parser
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

int stack_empty(interp *vm){   // returns if the stack is empty or not.
 if(vm->sc == 0){
  return 1;
  }
 else { 
  return 0;
  }
}

void call_system(interp *vm){
 if(*vm->string == '"')vm->string++;
 if(strstr(vm->string, "\"")) vm->string[strlen(vm->string)-1] = 0;
  system(vm->string);
}
/* ******************************  PRIMITIVES ***************************************** */
int push(interp *vm, int n){ // pushes an int on the stack
 if(questionflag == 1) return vm->error = OK;
 vm->stack[vm->sp] = n;
 vm->sp++;
 vm->sc++;
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("pushed %d, sp=%d, sc=%d\n", n, vm->sp, vm->sc);
 return vm->error = OK; 
}

int pop(interp *vm){   // pops an int from the stack
 if(seeflag) return 0;
 if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("POP CALLED\n");
 if(vm->sp== 0){ 
   printf("ERROR: STACK UNDERFLOW\n");
  return 0;
 }
 vm->sp--;
 int c = vm->stack[vm->sp];
 vm->sc--;
 vm->sv = c;
 return c; 
}

void clear_stack(interp *vm){
 int i;
for (i = 0 ; i <= vm->sp; i++){
  drop(vm);
 }
 int sctemp = vm->sc;
 for (i = 0 ; i < sctemp; i++){
  drop(vm);
 }
  vm->sp = 0;
  vm->sc = 0;
}

int add(interp *vm){ // pop 2 items from the stack and add them
 int a, b, c;
 vm->sv = 0;
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("ADD CALLED\n");
  a = pop(vm);
  b = pop(vm);
  c = b + a;
  push(vm, c);
 vm->sv = c;
return vm->sv;
}

int sub(interp *vm){
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  c = b - a;
  push(vm, c);
  vm->sv = c;
return vm->sv;
}

int mul(interp *vm){
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  c = b * a;
  push(vm, c);
  vm->sv = c;
return vm->sv;
}

int drop(interp *vm){   // drops the last int from the top of stack
  int a = 0;
  a = pop(vm);
 vm->sv = vm->stack[vm->sp];
 return vm->sv;
}
int dup(interp *vm){     // duplicates the top of the stack
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("DUP CALLED \n"); 
 int a = 0;
 a = vm->stack[vm->sp - 1];
 push(vm, a);
 vm->sv = a;
 return vm->sv;
}

int swap(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("SWAP CALLED \n"); 
 int a, b;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  push(vm, a);
  push(vm, b);
  vm->sv = a;
return vm->sv;
}

int mod(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("MOD CALLED \n"); 
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  c = b % a;
  push(vm, c);
  vm->sv = c;
return vm->sv;
}

int equal(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("EQUAL CALLED \n"); 
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  c = b == a ? -1 : 0;
  push(vm, c);
  vm->sv = c;
return vm->sv;
}

int not_equal(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("NOT EQUAL CALLED \n"); 
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  c = b != a ? -1 : 0;
  push(vm, c);
  vm->sv = c;
return vm->sv;
}

int greater(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("GREATER THAN CALLED \n"); 
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  c = b > a ? -1 : 0;
  push(vm, c);
  vm->sv = c;
return vm->sv;
}

int less(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("LESS THAN CALLED \n"); 
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  b = pop(vm);
  c = b < a ? -1 : 0;
  push(vm, c);
  vm->sv = c;
return vm->sv;
}

int negate(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("NEGATE CALLED \n"); 
 int a, b, c;
 vm->sv = 0;
  a = pop(vm);
  a = a * -1;
  push(vm, a);
  vm->sv = a;
return vm->sv;
}

void rotate(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("ROTATE CALLED \n"); 
int a, b, c; // pop the last 3 elements from the stack and rotate them
a = pop(vm);
b = pop(vm);
c = pop(vm);
push(vm, b);
push(vm, a);
push(vm, c);
}

void over(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("OVER CALLED \n"); 
int a, b; // take the next to last stack element DUP and push
a = pop(vm);
b = pop(vm);
push(vm, b);
push(vm, a);
push(vm, b);
}

void nip(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("NIP CALLED \n"); 
int a, b; // removes the second from last
a = pop(vm);
b = pop(vm);
push(vm, a);
}

void tuck(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("TUCK CALLED \n"); 
int a, b, c; // take the last and push it 3 behind
a = pop(vm);
b = pop(vm);
c = pop(vm);
push(vm, c);
push(vm, a);
push(vm, b);
push(vm, a);
}

int see(interp *vm){
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("SEE CALLED \n"); 
 char *buff; 
 parsespace(vm);
if(DEBUG>=1 || DEBUG_LAYER == 1 ) printf("SEE CALLED ON '%s'\n", vm->string); 
 buff = findsym(vm, strtoupper(vm->string));   // if a word exists then print the definition
   if(strcmp(buff, "FINDSYM_NIL") != 0){
   printf("FOUND:\n: %s %s\n", vm->string, buff); 
  }
 vm->string = "";
 return 0; 
}
/* *********************************** EVAL *************************************** */
void eval(interp *vm, int op){ // evaluates the current token mapped by word
if(DEBUG>=6 || DEBUG_LAYER == 6 ) printf("EVAL CALLED\n"); 
 if(DEBUG>=6 || DEBUG_LAYER == 6 ) printf("OP DETECTED: '%s'\n", words[op]); 
 switch (op){   // perform the Operation
    case CRT: { 
        printf("\n"); break;
     }
    case ADD: {
      add(vm); 
      break;
     }
    case SUB: {
      sub(vm); 
      break;
     }
   case MUL: {
      mul(vm); 
      break;
     }
    case DUP: {
      dup(vm); 
      break;
     }
   case DRP: {
      drop(vm); 
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
    swap(vm); 
    break;
    }
  case CLR: {
      clear_stack(vm); 
      break;     
    }
  case MOD: {
     mod(vm); 
     break;
    }
  case EQL: {
     equal(vm); 
     break;
    }
  case GTN: {
     greater(vm); 
     break;
    }
  case LTN: {
     less(vm); 
     break;
    }
  case SEE: {
     seeflag = 1; 
     see(vm); 
     break;
    }  
  case VAR: {
     variable(vm); 
     break;
    }
  case STO: {
     store(vm, varstack[0]); 
     break;
    }
  case RVR: {
       readvar(vm, varstack[0]); 
       break;
    }
  case IF: {
      branch_if(vm);
      break;
    }
  case ELS: {
      branch_else(vm); 
      break;
    }
  case NEQ: {
     not_equal(vm); 
     break;
   }
 case DSM: {
     dumpsym(vm); 
     break;
   } 
 case DO: {
     branch_do(vm); 
     break;
   } 
 case NEG: {
     negate(vm);
     break;
   } 
 case LIB: {            
    readlib(vm, vm->string); 
    break;
   } 
 case I: {
     printf("%d", vm->r0); 
     vm->string++;  
     break;
   } 
 case ROT: {
     rotate(vm); 
     break;
   } 
 case OVR: {
     over(vm); 
     break;
   } 
case NIP: {
     nip(vm); 
     break;
   } 
case TCK: {
     tuck(vm); 
     break;
   } 
 case QST: {
     break;
   }
 case SYS: {
    call_system(vm);
    break;   
 } 
  case EXT: { 
     op = BYE; 
   }
  case BYE: {
    destroy_vm(vm); 
    if(interact) printf("Exiting Interpreter..\n"); 
    exit(0); 
   }
  } // end switch
}
/* **************************   PARSEWORD ****************************************************** */
int parseword(interp *vm){              // Attempts to parse a word from the input
 if(! isprint(*vm->string)) {
  if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("ERROR: NON ASCII VALUE DETECTED.\n"); 
  vm->string = "";
  return 0;
  }
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("PARSEWORD CALLED\n");
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("PARSEWORD vm->string: '%s'\n", vm->string);
 int c = 0;
 char word[WORD_MAX]; 
  int i = 0;
  while(isprint(*vm->string) && *vm->string != ' '){
      word[i++] = *vm->string; 
      vm->string++;
  }
 parsespace(vm);
 word[i] = '\0';
 if(DEBUG>=7 || DEBUG_LAYER == 7 )printf("PARSEWORD AFTER '%s'\n", vm->string);
 strtoupper(word);  // convert the string to upper 
 int l = lookup(word);
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("LOOKUP FOUND: '%s:%d'\n", words[l], l );
 if(l > 0) {   // call eval with the command found
   if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("lookup op: %d\n", l);
   eval ( vm, l); 
  }
 else {    // if the word is not found in primatives then search the symtable
 char *hresult;
 hresult = findsym(vm, word);
 if(DEBUG>=7 || DEBUG_LAYER == 7 ) printf("parseword::hresult = '%s'\n", hresult);
 if(strstr(hresult, "FINDSYM_NIL") == 0){
  if(DEBUG>=7 || DEBUG_LAYER == 7 )  printf("CALLING READ with findsym data: '%s' vm->string = '%s'\n", hresult, vm->string);
  if(*vm->string == '?') { question(vm, word); }
   if(*vm->string == '!'){ store(vm, word); } else {
      read(vm, hresult);
      }
  }
 }
 memset(word, 0, sizeof(word));
 if(DEBUG>=7 || DEBUG_LAYER == 7 )printf("PARSEWORD RETURNING AND STRING LEFT '%s'\n", vm->string);
 return vm->state = OK;
}
/* **************************   PARSEDOT  ***************************************************** */
interp * parsedot(interp *vm){        // Parses an expression beginning with a DOT
 if(DEBUG > 8 || DEBUG_LAYER == 8) printf("PARSEDOT CALLED\n");
   vm->string++;
   if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("PARSEDOT S: %s\n", vm->string);
    if(*vm->string == 'S' || *vm->string == 's'){
     if(DEBUG > 8 || DEBUG_LAYER == 8 )  printf("FOUND COMMAND %c\n", *vm->string);
    print_stack(vm);
    } 
    else if(*vm->string == '"'){        // if we find a string
      vm->string++; 
        if(*vm->string != ' '){  printf("SYNTAX ERROR: <EXPECTED SPACE>\n");
              vm->string = ""; 
             return vm;
         }
          vm->string++;// eat the space
         while(*vm->string != '"'){
           printf("%c", *vm->string);
            vm->string++; 
         }        
    }
    else {
      int a = 0;
      a = pop(vm);
      printf("%d", a); 

     }
   vm->string++; 
  return vm;
}

/* **************************   PARSENUM ****************************************************** */
// parses an integer
interp *parsenum (interp *vm){// parse the current number and push on the stack
   if (DEBUG > 8 || DEBUG_LAYER == 8) printf("parsenum called on string: '%s'\n", vm->string);
  char *s = vm->string; 
  int n = 0;
  while(*s >= '0' && *s <= '9' && *s != ' ') {
    n = (n * 10) + (*s - '0');    
     s++;
   } 
  if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("parsenum  vm->string '%s'\n", vm->string);
  if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("PARSENUM:: collected n: '%d'\n", n);
  push(vm, n);
  vm->string = s;
  if(DEBUG > 8 || DEBUG_LAYER == 8 ) printf("parsenum returning vm->string '%s'\n", vm->string);
  if(*vm->string != '\0') read(vm, vm->string);
  return vm;
}

/* **************************   COMPILE MODE ****************************************************** */

interp *compile(interp *vm){ 
 if (DEBUG > 5 ) printf("compile called on string: '%s'\n", vm->string);
 compilemode = 1; 
 vm->string++; // strip the colon
 parsespace(vm);
 char cmdname[500];
 char cmdval[500];
 int n = 0;
  char *s;
 while(*vm->string != ' '){     // parse the name of the command
     cmdname[n] = *vm->string;
     vm->string++;
     n++;
 }
 cmdname[n] ='\0';

 parsespace(vm);
 n = 0;
while(*vm->string != ';'){
     cmdval[n] = *vm->string;
     vm->string++;
     n++;
 }
 cmdval[n] = '\0';
if(*vm->string == ';' ) compilemode = 0;
 // send the command to the table and the rest of the string as definition
 addsymname(cmdname, cmdval);
 vm->string++;
 return vm;
}

/* ************************** READ INPUT **************************************** */

void read(interp *vm, char *s){
 if(seeflag) return;

 if(DEBUG > 9 || DEBUG_LAYER == 9 ) printf("READ CALLED on '%s'\n", s);
 
   strcpy(progmem[linenum], s); // store the input in progmem
   linenum++;   
   vm->string = strdup(s);
   int i;

 while(*vm->string != '\0'){
   parsespace(vm);
   if(isdigit(*vm->string) ) {
     parsenum(vm);
    if(DEBUG > 9 || DEBUG_LAYER == 9) print_stack(vm);
   }
   else if(*vm->string == ':'){
     compile(vm);
   }
   else if(*vm->string == '.'){
     if(DEBUG > 9 || DEBUG_LAYER == 9) printf("DETECTED DOT\n");
     parsedot(vm);
   }
   else if(*vm->string == '?'){
      printf("READ CAUGHT QUESION ?\n");
    }
  else { parseword(vm); }
  parsespace(vm);
 }
 questionflag = 0;
}

/* ****************************** PRINT  ************************************************* */
// this will free the initial memory used by the vm
void destroy_vm(interp *vm){
 free(vm);
}

// This prints what is on the stack
void print_stack(interp *vm){
 int i;
 if(interact) printf("[ ");
 for (i = 0 ; i < vm->sp; i++){
   printf("%d ", vm->stack[i]);
  }
 if(interact) printf("]\n");
}

void dumpsym(interp *vm){
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

void readlib(interp *vm, char *filename){
   if(DEBUG >=8) printf("READLIB CALLED on '%s'\n", filename);
   char buffer[MAX_STR_LEN];
   if(*filename == '"')filename++;
    if(strstr(filename, "\"")) filename[strlen(filename)-1] = 0;
       FILE *lib = fopen(filename, "r");  
    if(lib == NULL) { printf("ERROR: Couldn't open file %s for READ\n", filename);  return; }
       while(fgets(buffer, MAX_STR_LEN, lib) != NULL){
         buffer[strcspn(buffer, "\n")] = 0;
         if(DEBUG >= 1 || DEBUG_LAYER == 1) printf("read: %s\n", buffer);
           read(vm, buffer);
           memset(buffer, 0 , MAX_STR_LEN);         
        }        
}

// Main loop, initialize the VM, get input and call the parser
int main(int argc, char *argv[]){
  interp *vm;
  vm = init_vm();
  char buffer[MAX_STR_LEN];
  //readlib(vm, "functions.txt");

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
           read(vm, buffer);
           memset(buffer, 0 , MAX_STR_LEN);         
        }        
     }
  }
 
 if(interact) printf("%s\n", banner);
  while(1){  // INTERACTIVE MODE REPL
     fgets(buffer, MAX_STR_LEN, stdin); 
       buffer[strcspn(buffer, "\n")] = 0; 
            seeflag = 0;
           read(vm, buffer);
           printf("\nOK\n");
        memset(buffer, 0 , MAX_STR_LEN);            
 }

return 0;
}

