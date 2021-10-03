CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic
EXEC = proj2
LDLIBS = -g -pthread

all: $(EXEC)

$(EXEC): proj2.o
	gcc $(CFLAGS) -o $(EXEC) $^ $(LDLIBS)

proj2.o: proj2.c
	gcc $(CFLAGS) -c $^

clean:
	rm $(EXEC) *.o
