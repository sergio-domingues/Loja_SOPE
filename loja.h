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

#define SHM_SIZE 1024
#define BUFFER_SIZE 50
#define BALCOES_MAX 30
#define msg "fim_atendimento"

typedef struct {  
  int inicio_funcionamento;
  int duracao_funcionamento;
  int id;
  char nome[BUFFER_SIZE]; 
  char fifo_nome[BUFFER_SIZE];
  int num_cli_atendidos;
  int num_cli_atendimento;
  int tempo_medio_atendimento;
  int estado; 
} Balcao;

typedef struct {  
  char fifo_cliente[BUFFER_SIZE];
  char* shm_name;
  int tempo_simula_atendimento; 
  int id_balcao;  
}Info_atendimento;

typedef struct {  
  pthread_mutex_t access_lock;
  pthread_mutex_t log_access;
  int tempo_abertura_loja;
  int balcoes_registados; 
  Balcao balcoes[BALCOES_MAX];
}Loja;

int readline(int fd, char *str)
{
  int n;
  
  do
  {
    n = read(fd,str,1);
  }
  while (n < 0 && *str++ != '\0');
  return n;
}

void formated_time(char * buf){
    
    time_t     now;
    struct tm  ts;
    // Get current time
    time(&now);

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
 
  formated_time(buffer);
  fprintf(log_stream,"%-20s | %-10s  | %-10d  | %-20s | %20s \n", buffer, quem, balcao_id, descricao, canal);    
  
  fclose(log_stream);
}

void print_loja(Loja* loja){
  
  printf("abertura_loja:%d\nbalcoes:%d\n",loja->tempo_abertura_loja,loja->balcoes_registados);
  
  int i;
  for(i=0; i < loja->balcoes_registados; i++){    
    printf("balcao_id:%d\ninit_func:%d\ndur:%d\ncli_atend:%d\ncli_atendid:%d\n",i+1,loja->balcoes[i].inicio_funcionamento,loja->balcoes[i].duracao_funcionamento,loja->balcoes[i].num_cli_atendimento,loja->balcoes[i].num_cli_atendidos);
  }  
}