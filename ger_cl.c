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
  char buffer[BUFFER_SIZE], buf_aux[BUFFER_SIZE];
  int shmfd;
  shmfd = shm_open(argv[1],O_RDWR,0660);

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
    
    pthread_mutex_lock(&(l_ptr->access_lock)); //espera bloqueante acesso à shm     
    
    n=l_ptr->balcoes_registados; 
    
    min = l_ptr->balcoes[0].num_cli_atendimento; //inicializacao valor minimo de clientes atendimento
    
    //ESCOLHA DO BALCAO=========================
    int i;
    for(i=0; i < n; i++){ 	                               //verificar qual o balcao com menos clientes	
      if(l_ptr->balcoes[i].duracao_funcionamento < 0) {   //verifica se balcao ainda está em funcionamento	  
        if(l_ptr->balcoes[i].num_cli_atendimento < min){	    
          min = l_ptr->balcoes[i].num_cli_atendimento;
          min_id = i;
        }
      }	
    }
    //==========================================           
    gera_fifo_cli(buffer); //cria FIFO cliente
    pthread_mutex_unlock(&(l_ptr->access_lock)); //liberta acesso a shm

    int fd_cli = open(buffer,O_RDONLY|O_NONBLOCK);             //abre FIFO cliente para leitura
    
    //log 
    log_loja(argv[1],"Balcao",l_ptr->balcoes[min_id].id,"pede_atendimento",buffer);
    
    fd = open(l_ptr->balcoes[min_id].fifo_nome, O_WRONLY);  //abre fifo balcao para escrita
    
    write(fd,buffer,strlen(buffer)); //escreve nome fifo_cliente no fifo_balcao
    
    readline(fd_cli,buf_aux);     //espera bloqueante  ate receber mensagem "fim_atendimento"

    //recebeu mensagem
    close(fd);          //fecha fifo balcao para escrita
    close(fd_cli);       //fecha fifo cliente
    unlink(buffer);     //destroi fifo cliente
    }    
  }

  return 0;
}

void gera_fifo_cli(char* buf){

  pid_t pid = getpid();
  int n;

  n = sprintf(buf,"./tmp/fc_%d",pid); //getpid(void) retorna pid do proprio processo

  if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO NOME DO FIFO (SPRINTF)\n");
    exit(1);
  }

  n = mkfifo(buf,0660); 

  if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO FIFO CLIENTE\n");
    exit(1);
  }
}