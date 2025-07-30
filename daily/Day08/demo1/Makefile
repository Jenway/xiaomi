CC = g++
CFLAGS = -Wall -O2

SRCS = calc.c add.c sub.c
OBJS = $(SRCS:.c=.o)

all: calc

calc: $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean:
	rm -f $(OBJS) calc

re: fclean all