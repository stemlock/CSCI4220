CC = gcc
CFLAGS = -Wall -Werror -pthread -ldns_sd
OBJ = zeroconf_server.c

#This is the rule to make object files by compile them with their respective *.c file
#Only useful if you have multiple object files to compile.

## $@ refers to the file to the left of the colon :
## $? refers to the files of the changed dependents, fiels newer than the make target
## $< refers to the first item in the after the : (prerequistes), usually source file
## $^ is all the files after the :

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

##This makes the zero_conf.out file by compiling the object files
zero_conf.out: $(OBJ)
	$(CC) $(OBJ) -o $@ $(CFLAGS)