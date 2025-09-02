NAME = webserv

CC = c++
CFLAGS = -g -std=c++98 -march=native
# -Wall -Wextra -Werror 

OBJ_DIR = obj
SRC_DIR = src
INC_DIR = inc

SRC =		src/main.cpp \
			src/WebServer.cpp \
			src/CgiHandler.cpp \
			src/ResponseBuilder.cpp

OBJS = ${SRC:${SRC_DIR}/%.cpp=${OBJ_DIR}/%.o}

${NAME}: ${OBJS}
	${CC} -I${INC_DIR} ${CFLAGS} ${OBJS} -o $@

${OBJ_DIR}/%.o:${SRC_DIR}/%.cpp
	mkdir -p obj
	${CC} ${CFLAGS} -I${INC_DIR} -c $< -o $@

all: ${NAME}

v valgrind: ${NAME}
	valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes -s ./${NAME}

clean:
	rm -f ${OBJS}

fclean: clean
	rm -f ${NAME} 

re: fclean all

.PHONY: all clean fclean re
