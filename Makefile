myforth:
	@echo Compiling.. 
	cc -o myforth myforth.c 
	cc myforth.c
clean:
 ifeq ($(UNAME), Linux)
	rm myforth a.out
else
	rm myforth.exe a.exe
endif

