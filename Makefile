#这是网上抓的一个通用的makefile，作者不可考，无法感谢了。

EXE	= out
CC 	= gcc
SRC	= $(wildcard *.c)
OBJ	= $(SRC:.c=.o)
CFLAGS = -g

all: depend $(EXE)

depend:
	$(CC) $(SRC) -M > .depend

-include .depend

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE)

clean:
	rm $(EXE) $(OBJ) .depend Areactor* -f

utf8:
	iconv -f gb2312 -t utf-8 *.c *.h

run:
	python ./server/server_A.py &
	./$(EXE)