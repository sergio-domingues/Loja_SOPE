FLAGS = -Wall

make: out_dir balcao ger_cl

out_dir:
	${MKDIR_P} ${BIN_DIR}
	
balcao:
	gcc $(FLAGS) balcao.c loja.h -o bin/balcao -lrt -pthread
	
ger_cl:
	gcc $(FLAGS) ger_cl.c loja.h -o bin/ger_cl -lrt
