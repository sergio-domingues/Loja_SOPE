#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // For O_* constants
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> 
#include <unistd.h>

#define SHM_SIZE 1024
#define BUFFER_SIZE 50


void* thr_func(void*arg);

typedef struct {
  
  int inicio_funcionamento;
  int duracao_funcionamento;
  int id;
  char* fifo_nome;
  int num_cli_atendidos;
  int num_cli_atendimento;
  int tempo_medio_atendimento;
  int estado;
  pthread_mutex_t mutex; //Não tenho a certeza
} Balcao;

typedef struct {
  
  char* fifo_cliente;
  int tempo_simula_atendimento; 
  
}info_atendimento;


int main(int argc,char* argv[]){
  
  if(argc!=3){   
    fprintf(stderr,"Wrong no of arguments.\nUsage: balcao <nome_mempartilhada> <tempo_abertura> \n");
    exit(1);
  }
  
  if(atoi(argv[2]) <= 0){
    fprintf(stderr,"O TEMPO DE ABERTURA DO BALCAO DEVE SER SUPERIOR A ZERO!\n");
    exit(1);
  }
  
  //TODO realpaths argv[0] para criacao de fifos...
  
  //char buf[PATH_BUF_SIZE];
  //sprintf()...
  
  //variaveis========================
  int create_shm = 0; //Flag memoria partilhada já criada
  int shmfd;
  char *shm;
  int *s;
  
  char* SHM_NAME = argv[1];
  time_t tempo_actual_maquina = time(NULL);
  
  if(tempo_actual_maquina == -1){
    fprintf(stderr, "ERRO NA OBTENCAO DO TEMPO DA MAQUINA\n");
    exit(1);
  }
  //=================================
  
  //Cria memoria partilhada se nao existir
  shmfd = create_shared_memory(SHM_NAME,SHM_SIZE);
  
  if(shmfd >= 0)
    create_shm = 1;  
  
  //mapeamento para virtual memory
  shm = (char *) mmap(0,SHM_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
  if(shm == MAP_FAILED) { 
    fprintf(stderr, "ERRO NO MAPEAMENTO DA MEM PARTILHADA PARA MEM VIRTUAL\n");
    exit(1);
  }  
  
  //inicializacao primitivas shm=====================
  if(create_shm){
    
    s = (int*) shm; //inicio shm
    
    *s = 1; //flag: 1->mem_partilhada a ser acedida	
    s++;
    
    *s = tempo_actual_maquina; //data de abertura da loja
    s++;
    
    *s = 0; //num de balcoes registados       
  }  
  //============================================================   
  
  //inicializa atributos Balcao
  
  Balcao b;
  
  b.inicio_funcionamento = tempo_actual_maquina;
  b.duracao_funcionamento = -1;
  b.num_cli_atendidos=0;
  b.num_cli_atendimento=0;
  b.tempo_medio_atendimento=0;
  b.estado = 1;
  b.mutex = PTHREAD_MUTEX_INITIALIZER; //Não tenho a certeza
  
  //TODO verificar se pode aceder a mem partilhada:-> PROBLEMATICA DE ACESSO CONCORRENTE ENTRE PROCESSOS
  
  s = (int*)shm; 
  s++;
  s++; //s->nº balcoes
  (*s)+=1; //nº balcoes++
  
  b.id = *s; //id balcao
  s++; //aponta agora para o inicio da tabela de balcoes
  
  //criação do fifo TODO usar realpath para o caminho do fifo  
  char buffer[BUFFER_SIZE];
  int n;
  pid_t pid = getpid();
  
  n = sprintf(buffer,"./tmp/fb_%d",pid); //getpid(void) retorna pid do proprio processo
  
  if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO NOME DO FIFO (SPRINTF)\n");
    exit(1);
  }
  
  n = mkfifo(buffer,0660); 
  
  if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO FIFO\n");
    exit(1);
  }
  
  b.fifo_nome = buffer;  
  
  Balcao* b_ptr = (Balcao*) s;
  int i;
  for(i=0; i < b.id-1 ; i++, b_ptr++); //desloca apontador para a nova linha correspondente a este balcao
  
  *b_ptr = b;  //regista balcao na shm
  
  s = (int*)shm;
  *s = 0; //actualiza flag de acesso
  //==========================================================  
  
  //ZONA TESTES===============================================
  int k = 0;
  for(s=(int*)shm; k < 20; k++, s++)
    printf("value:%d\n",*s);
  
  s = (int*)shm;
  s++;
  s++;
  s++;
  
  printf("->:%d\n",*s);
  
  Balcao x;
  b_ptr = (Balcao*) s;
  x=*b_ptr;
  
  printf("->:%s\n",x.fifo_nome);
  //==========================================================  
  
  //===========THREAD RELATED PART TODO
  
  //criacao mutex  TODO  
  //CADA THREAD VAI TENTAR ADQUIRIR MUTEX
  
  int tempo_limite = (int)tempo_actual_maquina + atoi(argv[2]);
  int clientes_atendidos = 0; // =1 todos os clientes atribuidos foram atendidos
  int fd;  
  
  
  //TODO usar sinais para interromper espera bloquenate no FIFO caso balcao ja tenha atingido limite de funcionamento e todos 
  //os seus clientes tenham sido atendidos
  
  while((tempo_limite - (int)time(NULL)) > 0 && !clientes_atendidos){    
    
    fd = open(b.nome_fifo, O_RDONLY);//espera bloqueante ate que um processo abra o FIFO para escrita. PROBLEMA:pode nao chegar a escrever para la
    
    while(readFifo(fd,buffer)); //espera bloqueante ate conseguir ler do FIFO
    
    close(fd);
    
    info_atendimento info; //struct a ser passada com argumento à thr_func
    
    info.fifo_cliente = buffer;
    
    if((b.tempo_simula_atendimento+1) >10) //maximo de 10 segundos
      info.tempo_simula_atendimento = 10;
    
    info.tempo_simula_atendimento = b.num_cli_atendimento+1;
    
    pthread_t tid;
    
    //CRIA THREAD
    pthread_create(&tid, NULL, thr_func, &buffer);    
  }
  
  
  //===============================
  
  //se for o ultimo balcao activo e tiver terminado o seu tempo_abertura
  //COMO VERIFICAR: percorrer cad abalcao da mem partilhada e verificar se ainda esta em funcionamento (flag -1)
  
  int nrBalcoesActivos=1; //numero de balcoes activos da loja (predefinido como sendo só um)
  
  s = (int*)shm; 
  s++;
  s++;
  int nr_balcoes=s; //s->nº balcoes
  s++;
  
	Balcao* balcao_tabela;
	int i;
	
	for(i=0;i<nr_balcoes;i++){
		balcao_tabela=(Balcao *)s;
		if((*balcao_tabela).estado==1){
			nrBalcoesActivos++;
			break;
		}
	}
  
  //actualiza last info do ultimo balcao TODO
  
  //coloca na posicao inicial da tabela
 
  
  //gerar estatisticas de funcionamento da loja
  
  
  //==============
  //close and remove shared memory region  
  destroy_shared_memory(shm,SHM_SIZE); 
  
  return 0;  
}


void* thr_func(void*arg){
  
  //TENTA ADQUIRIR MUTEX TODO
  //se adquirir tranca mutex
  
  sleep((info_atendimento*)arg->tempo_simula_atendimento);
  
  int fd_fifo = open((info_atendimento*)arg->fifo_cliente, O_WRONLY);
  
  write(fd_fifo,"FIM_ATENDIMENTO",sizeof("FIM ATENDIMENTO"));
  
  //destranca mutex TODO    
}

//=====================================================
//FUNCOES AUXILIARES====================================

int readFifo(int fd, char *str){
  int n;
  do{
    n = read(fd,str,1);
  }
  while (n>0 && *str++ != '\0');
  return (n>0);
}

//CRIA SHM SE NAO EXISTIR
int create_shared_memory(char* shm_name, int shm_size){
  int shmfd;
  
  //try to create the shared memory region
  shmfd = shm_open(SHM_NAME,O_CREAT|O_EXCL|O_RDWR,0660);
  
  if(shmfd == EEXIST) //SE JA EXISTIR RETORNA-1
    return -1;
  
  if(shmfd < 0 ){
    fprintf(stderr, "ERRO NA CONECCAO A MEM PARTILHADA\n");
    exit(EXIT_FAILURE);
  }
  
  //specify the size of the shared memory region
  if (ftruncate(shmfd,shm_size) < 0){
    fprintf(stderr, "ERRO NA ATRIBUICAO DO TAMAMNHO DA MEM PARTILHADA\n");
    exit(EXIT_FAILURE);
  } 
  
  return shmfd;
} 


void destroy_shared_memory(Shared_memory *shm, int shm_size)
{
  if (munmap(shm,shm_size) < 0){
    fprintf(stderr,"ERRO NA LIBERTACAO DA MEMORIA\n");
    exit(EXIT_FAILURE);
  }
  if (shm_unlink(SHM_NAME) < 0){
    fprintf(stderr,"ERRO NA REMOCAO DA MEMORIA PARTILHADA\n");
    exit(EXIT_FAILURE);
  }
} 
