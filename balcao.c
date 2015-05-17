#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h> // For O_* constants
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> 

#define SHM_SIZE 1024
#define PATH_BUF_SIZE 50

typedef struct {
  
  int inicio_funcionamento;
  int duracao_abertura;
  int id;
  char nome_fifo;
  int num_cli_atendidos;
  int num_cli_atendimento;
  int tempo_medio_atendimento;
  int estado;  
} Balcao;


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
  int create_shm = 0;
  int shmfd;
  char *shm,*s;
  
  char* SHM_NAME = argv[1];
  time_t tempo_actual_maquina = time(NULL);
  
  if(tempo_actual_maquina == -1){
    fprintf(stderr, "ERRO NA OBTENCAO DO TEMPO DA MAQUINA\n");
    exit(1);
  }
  //=================================
  
  //verifica se a memoria partilhada existe  
  if((shmfd = shm_open(SHM_NAME,O_RDWR,0600)) < 0){
    create_shm = 1; // criar memoria partilhada
  }
  
  //Cria memoria partilhada (caso:1º balcao)================================
  if(create_shm){  
    
    //create the shared memory region IF IT NOT EXISTS
    shmfd = shm_open(SHM_NAME,O_CREAT|O_RDWR,0600);
    if(shmfd<0){ 
      fprintf(stderr, "ERRO NA CONECCAO A MEM PARTILHADA\n");
      exit(1); 
    }
    
    //atribuicao tamanho da mem partilhada
    if (ftruncate(shmfd,SHM_SIZE) < 0){
      fprintf(stderr, "ERRO NA ATRIBUICAO DO TAMAMNHO DA MEM PARTILHADA\n");
      exit(1);
    }
    printf("SHARED MEMORY CREATED SUCCESSFULLY\n"); // TODO : log
  }  
  //========================================================================
  
  //mapeamento para virtual memory
  shm = (char *) mmap(0,SHM_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
  if(shm == MAP_FAILED) { 
    fprintf(stderr, "ERRO NO MAPEAMENTO DA MEM PARTILHADA PARA MEM VIRTUAL\n");
    exit(1);
  }  
  
  //inicializacao primitivas shm=====================
  if(create_shm){
    
    s = shm;
    *(int*)s = 0; //flag: 1->mem_partilhada a ser acedida 
    s++;
    
    *s = tempo_actual_da_maquina; //data de abertura da loja
    s++;
    
    *s = 0; //num de balcoes registados     
  }  
  //============================================================   
  
  //inicializa atributos Balcao
  
  Balcao b;
  
  b.inicio_funcionamento = tempo_actual_maquina;
  b.duracao_abertura = atoi(argv[2]);
  b.num_cli_atendidos=0;
  b.num_cli_atendimento=0;
  b.tempo_medio_atendimento=0;
  b.estado = 1;    
  
  //TODO verificar se pode aceder a mem partilhada PROBLEMATICA DE ACESSO CONCORRENTE ENTRE PROCESSOS
  s = shm;
  (int*)s;
  
  s = s+2; //s->nº balcoes
  *s = *s +1; //nº balcoes++
  b.id = *s; //id balcao
  s++; //aponta agora para o inicio da tabela de balcoes
  
  //criação do fifo TODO usar realpath para o caminho do fifo
  
  char buffer[50];
  int n;
  
  n = sprintf(buffer,"./tmp/fb_%d",getpid(void)); //getpid(void) retorna pid do proprio processo
  
  if(n < 0){
   fprintf(stderr,"ERRO NA CRIACAO DO NOME DO FIFO (SPRINTF)\n");
   exit(1);
  }
  
  mkfifo(buffer,0660);
  
  
  b.nome_fifo = ;
  
  (Balcao*) s;
  
  int i;
  for(i=0; i < b.id-1 ; i++; s++); //desloca apontador para a nova linha correspondente a este balcao

  *s = b;  //regista balcao na shm
  
  (char*) s = shm;
  *(int*)s = 0; //actualiza flag de acesso
  
  
  //===========THREAD RELATED PART TODO
  
  //criacao mutex  
  
  
  open(FIFO1,O_RDONLY); //fica em espera bloqueante ate que um processo abra o FIFO para escrita, PROBLEMA: pode nao chegar a escrever para la
  
  //TODO lanca thread qd receber cliente no fifo e volta a bloquear (fazer isto num ciclo while)
  
  //===============================
  
  //se for o ultimo balcao activo e tiver terminado o seu tempo_abertura
  
  //actualiza last info do ultimo balcao TODO
  
  //gerar estatisticas de funcionamento da loja
  
  
  //==============
  //close and remove shared memory region
  
  if (munmap(shm,SHM_SIZE) < 0){ 
    fprintf(stderr,"ERRO NA LIBERTACAO DA MEMORIA\n");
    exit(1);
  }
  if (shm_unlink(SHM_NAME) < 0) {  
    fprintf(stderr,"ERRO NA REMOCAO DA MEMORIA PARTILHADA\n");
    exit(1);
  }  
  
  return 0;  
}