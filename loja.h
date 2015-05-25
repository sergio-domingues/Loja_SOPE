#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> // For O_* constants
#include <time.h> 
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <limits.h>

#define SHM_SIZE 10
#define BUFFER_SIZE 50
#define BALCOES_MAX 30
#define msg "fim_atendimento"

typedef struct {  
  time_t inicio_funcionamento;
  pthread_mutex_t access_balcao;
  int duracao_funcionamento;
  int id;
  char nome[BUFFER_SIZE]; 
  char fifo_nome[BUFFER_SIZE];
  int num_cli_atendidos;
  int num_cli_atendimento;
  float tempo_medio_atendimento;
  int estado; 
} Balcao;

typedef struct {  
  char fifo_cliente[BUFFER_SIZE];
  char* shm_name;
  int id_balcao;  
}Info_atendimento;

typedef struct {  
  pthread_mutex_t access_lock;
  pthread_mutex_t log_access;
  time_t tempo_abertura_loja;
  int balcoes_registados; 
  Balcao balcoes[BALCOES_MAX];
}Loja;

int readline(int fd, char *str){
  int n;
  
  do
  {
    n = read(fd,str,1);
  }
  while (n > 0 && *str++ != '\0');
  return n;
}

void formated_time(char * buf){
    time_t     now;
    struct tm  ts;
   
    time(&now);  // Get current time

    // Format time, "yyyy-mm-dd hh:mm:ss"
    ts = *localtime(&now);
    strftime(buf, BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", &ts);
}

void log_loja(char* shm_name, const char* quem, int balcao_id, const char* descricao, char* canal){
  
  FILE* log_stream;
  char buffer[BUFFER_SIZE];

  sprintf(buffer,"%s.log",shm_name);
  if((log_stream = fopen(buffer,"r")) == NULL){
    log_stream =fopen(buffer,"w");   //creates log file
    fprintf(log_stream,"%-20s | %-10s  | %-10s  | %-20s | %20s \n" ,"quando","quem","balcao","o_que","canal_criado/usado");
  }
  else 
    log_stream = fopen(buffer,"a");
 
  setbuf(log_stream,NULL);  //unbuffering

  formated_time(buffer);
  fprintf(log_stream,"%-20s | %-10s  | %-10d  | %-20s | %20s \n", buffer, quem, balcao_id, descricao, canal);    
  
  fclose(log_stream);
}

void print_loja(Loja* loja){
  fprintf(stderr,"\n//===========================//\n");
  fprintf(stderr,"abertura_loja:%d\nbalcoes:%d\n",(int)loja->tempo_abertura_loja,loja->balcoes_registados);
  
  int i;
  for(i=0; i < loja->balcoes_registados; i++){    
    fprintf(stderr,"balcao_id:%d\ninit_func:%d\ndur:%d\ncli_atendim:%d\ncli_atendid:%d\n",i+1,(int)loja->balcoes[i].inicio_funcionamento,loja->balcoes[i].duracao_funcionamento,loja->balcoes[i].num_cli_atendimento,loja->balcoes[i].num_cli_atendidos);
  }  
  fprintf(stderr,"//===========================//\n");
}

void gera_stats(char* shm){

  int i,clis=0;
  float t_medio=0;
  Loja* l_ptr = (Loja*) shm;
  char buffer[BUFFER_SIZE];

  printf("\n//==========STATS==========//\n");
  for(i=0; i < l_ptr->balcoes_registados;i++){

    strftime(buffer, BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", localtime(&l_ptr->balcoes[i].inicio_funcionamento));
    fprintf(stderr,"Balcao %d\n",l_ptr->balcoes[i].id);
    fprintf(stderr,"Tempo abertura:%s\n",buffer);
    fprintf(stderr,"Cli atendidos:%d\n",l_ptr->balcoes[i].num_cli_atendidos);
    fprintf(stderr,"Tempo medio atendimento:%5.2f\n",l_ptr->balcoes[i].tempo_medio_atendimento);
    clis += l_ptr->balcoes[i].num_cli_atendidos;
    t_medio += l_ptr->balcoes[i].tempo_medio_atendimento;
  }

  strftime(buffer, BUFFER_SIZE, "%Y-%m-%d %H:%M:%S",localtime(&l_ptr->tempo_abertura_loja));
  fprintf(stderr,"Abertura loja:%s\n",buffer);
  fprintf(stderr,"Total cli atendidos:%d\n",clis);
  if(clis==0)
    fprintf(stderr,"Tempo medio antedimento:%f\n", t_medio);
  else
    fprintf(stderr,"Tempo medio antedimento:%4.2f\n", (float) t_medio/clis);
  printf("//===========================//\n");
}