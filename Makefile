NAME:=openglfun
CC:=clang
CFLAGS:= -pedantic -Wall -Wextra -Werror -g
LDFLAGS= -pedantic -g -lglfw -lglew

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
CFLAGS += -I$(shell brew --prefix glfw)/include -I$(shell brew --prefix glew)/include

LDFLAGS += -L$(shell brew --prefix glfw)/lib -L$(shell brew --prefix glew)/lib  -framework Cocoa -framework OpenGL -framework IOKit
endif

ifdef	FSANITIZE
	CFLAGS += -g -fsnaitize=address
	LDFLAGS += -g -fsanitize=address
endif

SRCS:= src/main.c
OBJS:= $(SRCS:.c=.o)

all: $(OBJS)
	$(CC) $(OBJS) -o $(NAME) $(LDFLAGS) && echo "Compilation successful"

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: clean fclean re
