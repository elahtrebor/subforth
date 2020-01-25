myforth:
	@echo Compiling.. 
	cc -o subforth subforth.c 
clean:
 ifeq ($(UNAME), Linux)
	rm subforth
else
	rm subforth.exe
endif

