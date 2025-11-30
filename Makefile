NAME = webserv

CC = clang++
CFLAGS = -g -std=c++98 -march=native -fno-limit-debug-info 
# -Wall -Wextra -Werror

OBJ_DIR = obj
SRC_DIR = src

SRC := $(shell find $(SRC_DIR) -type f -name '*.cpp')
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

TMP_SRC_DIRS = ${shell find ${SRC_DIR} -type d}
OBJ_DIRS = ${subst ${SRC_DIR},${OBJ_DIR},${TMP_SRC_DIRS}}

INC_DIRS := $(TMP_SRC_DIRS)
INC_FLAGS := $(patsubst %,-I%,$(INC_DIRS))

${NAME}: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} -o $@

${OBJ_DIR}/%.o:${SRC_DIR}/%.cpp | ${OBJ_DIRS}
	${CC} ${CFLAGS} $(INC_FLAGS) -c $< -o $@

${OBJ_DIRS}:
	mkdir -p $(OBJ_DIRS)

all: ${NAME}

v valgrind: ${NAME}
	valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes -s ./${NAME}

debug: CFLAGS += -D LOG_LEVEL=0
debug: clean ${OBJS}
	rm -f ${OBJS}
	${NAME} -D LOG_LEVEL=0

clean:
	rm -f ${OBJS}

fclean: clean
	rm -f ${NAME}
	rm -f *.log

re: fclean all

.PHONY: all clean fclean re
