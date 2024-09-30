
SRC := Tcomp4sync_funs.c
OBJ := Tcomp4sync_funs.o
LIB := libTcomp4sync_funs.so

all:
	gcc -O2 -fPIC -c $(SRC)
	gcc -O2 -fPIC -shared -o $(LIB) $(OBJ)

clean:
	rm $(OBJ) $(LIB)
