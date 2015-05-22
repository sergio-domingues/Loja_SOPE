FLAGS = -Wall
MKDIR_P = mkdir -p
BIN_DIR = bin

make: out_dir balcao ger_cl

out_dir:
	${MKDIR_P} ${BIN_DIR}
	
balcao:
	gcc $(FLAGS) balcao.c loja.h -o ${BIN_DIR}/balcao -lrt -pthread
	
ger_cl:
	gcc $(FLAGS) ger_cl.c loja.h -o ${BIN_DIR}/ger_cl -lrt
