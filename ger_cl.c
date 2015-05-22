
#include "loja.h"

void gera_fifo_cli(char* buf);

int main(int argc, char*argv[]){

//CHECK INPUT==================
if(argc!=3){   
fprintf(stderr,"Wrong no of arguments.\nUsage: ger_cl <nome_mempartilhada> <num_clientes> \n");
exit(1);
}

if(atoi(argv[2]) <= 0){
fprintf(stderr,"O NUMERO DE CLIENTES DEVE SER SUPERIOR A ZERO!\n");
exit(1);
}
//=============================

char* shm;
int shmfd;
shmfd = shm_open(argv[1],O_RDWR,0777);

if(shmfd < 0){
fprintf(stderr,"ERRO AO TENTAR ACEDER A MEMORIA PARTILHADA\n");
exit(EXIT_FAILURE);    
}

shm = (char *) mmap(0,SHM_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
if(shm == MAP_FAILED) { 
fprintf(stderr, "ERRO NO MAPEAMENTO DA MEM PARTILHADA PARA MEM VIRTUAL\n");
exit(EXIT_FAILURE);
} 

pid_t pid;
int i;
for(i = 0; i < atoi(argv[2]); i++){

pid = fork();     
if(pid < 0){
fprintf(stderr,"ERRO NA EXECUCAO DO FORK\n");
exit(EXIT_FAILURE);
}

if(pid == 0){ //processo filho

int fd,n,min,min_id=0;
Loja* l_ptr = (Loja*)shm;      	
char buffer[BUFFER_SIZE], buf_aux[BUFFER_SIZE];
min = INT_MAX;

gera_fifo_cli(buffer);                                 //cria FIFO cliente
//pthread_mutex_lock(&(l_ptr->access_lock));             //espera bloqueante acesso à shm     

fprintf(stderr,"debug access_lock\n");

n = l_ptr->balcoes_registados;       	

//ESCOLHA DO BALCAO=========================
int i,existe_balcao=0;
for(i=0; i < n; i++){ 	                               //verificar qual o balcao com menos clientes	
  if(l_ptr->balcoes[i].duracao_funcionamento < 0) {   //verifica se balcao ainda está em funcionamento	  
  existe_balcao = 1;
  if(l_ptr->balcoes[i].num_cli_atendimento < min){	    
      min = l_ptr->balcoes[i].num_cli_atendimento;
      min_id = i;
    }
  }	
} 

if(!existe_balcao) {                                    //nao conseguiu alocar cliente
//pthread_mutex_unlock(&(l_ptr->access_lock)); 
unlink(buffer); 
fprintf(stderr,"n conseguiu alocar\n");
exit(1);
}

fprintf(stderr, "balcao escolhido:%d\n",min_id+1); 
log_loja(argv[1],"Client",l_ptr->balcoes[min_id].id,"pede_atendimento",buffer);

l_ptr->balcoes[min_id].num_cli_atendimento++;  	 	
//pthread_mutex_unlock(&(l_ptr->access_lock));            //liberta acesso a shm

fd = open(l_ptr->balcoes[min_id].fifo_nome, O_WRONLY);  //abre fifo balcao para escrita      
write(fd,buffer,strlen(buffer) + 1);                    //escreve nome fifo_cliente no fifo_balcao
close(fd);                                              //fecha fifo balcao para escrita


int fd_cli = open(buffer,O_RDONLY);         //espera bloqueante
readline(fd_cli,buf_aux);                  //espera bloqueante  ate receber mensagem "fim_atendimento"

//recebeu mensagem 
close(fd_cli);      //fecha fifo cliente (leitura)
unlink(buffer);     //destroi fifo cliente
return 0;
}    
}

return 0;
}

void gera_fifo_cli(char* buf){

pid_t pid = getpid();
int n;

n = sprintf(buf,"tmp/fc_%d",pid); //getpid(void) retorna pid do proprio processo
n = mkfifo(buf,0660); 

if(n < 0){
fprintf(stderr,"ERRO NA CRIACAO DO FIFO CLIENTE\n");
exit(1);
}
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
>>>>>>> f02810f6917e0a2024fd42105de02927168f122c
