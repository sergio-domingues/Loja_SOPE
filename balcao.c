#include "loja.h"

#define end_msg "terminou tempo"

void* thr_func(void*arg);
void* loop_thr(void*arg);
int create_shared_memory(char* shm_name, int shm_size);
void destroy_shared_memory(char *shm, int shm_size);
//=================

pthread_mutex_t balcao_lock;	//mutex thread related
char* shm; 			              //apontador para inicio shm (mem virtual)
int fd_OWR_b; 		           

void  ALARMhandler(int sig) {
  signal(SIGALRM, SIG_IGN);          		/* ignore this signal       */
  close(fd_OWR_b);                     //terminou tempo, fecha fifo (escrita) neste processo
  signal(SIGALRM, ALARMhandler);     		/* reinstall the handler    */
}

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
  
  signal(SIGALRM, ALARMhandler); 
  
  //variaveis========================
  int shmfd, create_shm = 0;
  Loja* l_ptr;
  
  time_t tempo_actual_maquina = time(NULL);
  
  if(tempo_actual_maquina == -1){
    fprintf(stderr, "ERRO NA OBTENCAO DO TEMPO DA MAQUINA\n");
    exit(1);
  }
  //=================================
  
  //Cria memoria partilhada se nao existir
  shmfd = create_shared_memory(argv[1],SHM_SIZE);
  
  if(shmfd >= 0)
    create_shm = 1;  
  
  //mapeamento para virtual memory
  shm = (char *) mmap(0,SHM_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
  if(shm == MAP_FAILED) { 
    fprintf(stderr, "ERRO NO MAPEAMENTO DA MEM PARTILHADA PARA MEM VIRTUAL\n");
    exit(1);
  }  
  
  printf("mapeou\n");
  //inicializacao primitivas shm=====================
  if(create_shm){    
    Loja loja;
    
    //log
    log_loja(argv[1],"Balcao",1,"inicia_mempart", "-");
    
    loja.tempo_abertura_loja = (int) tempo_actual_maquina;
    loja.balcoes_registados = 0;
    
    if (pthread_mutex_init(&(loja.access_lock), NULL) != 0){ 	 //INIT MUTEX SHM
      printf("\n access_mutex init failed\n");
      exit(EXIT_FAILURE);
    }
    
    l_ptr = (Loja*) shm; //inicio shm
    
    *l_ptr = loja;
    create_shm = 0;
  }  
  //============================================================   
  
  printf("criou e abriu loja\n");

  l_ptr = (Loja*) shm; 
  
  if(l_ptr->balcoes_registados == BALCOES_MAX){		//VERIFICA SE NAO ATINGIU LIMITE DE BALCOES
    fprintf(stderr,"limite de balcoes atingidos\n");
    exit(EXIT_FAILURE);
  }  
  
  //inicializa atributos Balcao=================================
  Balcao b;
  
  b.inicio_funcionamento = tempo_actual_maquina;
  b.duracao_funcionamento = -1;
  b.num_cli_atendidos = 0;
  b.num_cli_atendimento = 0;
  b.tempo_medio_atendimento = 0;
  b.estado = 1;    
  
  pthread_mutex_lock(&(l_ptr->access_lock)); //tenta fazer lock | espera bloqueante caso nao consiga
  
  l_ptr->balcoes_registados++;        //incrementa num de balcoes registados  
  b.id = l_ptr->balcoes_registados; 	//id balcao

  //criação do fifo TODO usar realpath para o caminho do fifo  
  int n;
  pid_t pid = getpid();
  char buffer[BUFFER_SIZE];
  
  sprintf(buffer,"fb_%d",pid);
  strcpy (b.nome , buffer);

  n = sprintf(buffer,"./tmp/%s",b.nome); //getpid(void) retorna pid do proprio processo
  
  if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO NOME DO FIFO (SPRINTF)\n");
    exit(1);
  }
    
  n = mkfifo(buffer,0660);   //criacao do fifo do balcao

  if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO FIFO\n");
    exit(1);
  }
  
  strcpy(b.fifo_nome, buffer);    
  
  l_ptr->balcoes[b.id-1] = b; //regista balcao no array
  
  //log  
  log_loja(argv[1],"Balcao",b.id,"cria_linh_mempart",b.nome);
    
  l_ptr = (Loja*) shm; 
  pthread_mutex_unlock(&(l_ptr->access_lock)); //faz unlock ao mutex de acesso à shm
  
  //==============  
  if (pthread_mutex_init(&balcao_lock, NULL) != 0){ 	//inicia mutex deste processo
    printf("\n mutex init failed\n");
    return 1;
  }  
  
  int fd = open(b.fifo_nome, O_RDONLY|O_NONBLOCK);    	//abre fifo balcao leitura
  if(fd < 0){
    fprintf(stderr, "ERRO NA ABERTURA DO FIFO LEITURA\n");
    exit(EXIT_FAILURE);
  }
  
  int old_flags = fcntl(fd, F_GETFL,0);
  fcntl (fd, F_SETFL, old_flags & (~O_NONBLOCK));   //clear O_NONBLOCK flag

  int fd_2 = open(b.fifo_nome, O_WRONLY);     //abre fifo balcao escrita
  if(fd_2 < 0){
    fprintf(stderr, "ERRO NA ABERTURA DO FIFO escrita\n");
    exit(EXIT_FAILURE);
  }

  fd_OWR_b = fd_2;       //descritor writing

  alarm(atoi(argv[2]));   //agenda alarme duracao funcionamento balcao
  
  //======================
  print_loja(l_ptr);      //DEBUG PRINTF
  //======================

  pid = fork();

  if(pid < 0){
    fprintf(stderr,"ERRO FORK BALCAO.C\n");
    exit(1);
  }

  if(pid==0){  //filho faz o trabalho -> assim pode criar balcoes no mesmo terminal

    //===========THREADING RELATED PART============================
    while(1){    

      int v = readline(fd,buffer); //espera bloqueante
      
      if(v==0)    //nao ha mais clientes para atender //todos os que tinham aberto para escrita, fecharam
        break;

      pthread_t tid;    
      Info_atendimento info; 
      Loja* l_ptr = (Loja*) shm;    
      
      strcpy(info.fifo_cliente,buffer);
      info.id_balcao = b.id ;
      info.shm_name = argv[1];
      
      pthread_mutex_lock(&(l_ptr->access_lock));              //lock

      if((l_ptr->balcoes[b.id-1].num_cli_atendimento+1) > 10) //maximo de 10 segundos
        info.tempo_simula_atendimento = 10;
      else
        info.tempo_simula_atendimento = l_ptr->balcoes[b.id-1].num_cli_atendimento+1;
      
      pthread_mutex_unlock(&(l_ptr->access_lock));            //unlock

      //CRIA THREAD
      int err = pthread_create(&tid, NULL, thr_func, &info);    
      
      if(err != 0){
        fprintf(stderr, "ERRO NA CRIACAO DA THREAD DE ATENDIMENTO\n");
        exit(EXIT_FAILURE);
      }
    }
    //===============
    
    //log
    log_loja(argv[1],"Balcao",b.id,"fecha_balcao",argv[1]);  

    l_ptr->balcoes[b.id].duracao_funcionamento = (int)time(NULL) - b.inicio_funcionamento; //actualiza duracao do balcao
    
    close(fd);
    close(fd_2);
    unlink(b.fifo_nome);      //destroi fifo  
    
    //======================
    print_loja(l_ptr);    //DEBUG PRINTF
    //======================

    int i,fecha = 0;
    pthread_mutex_lock(&(l_ptr->access_lock));

    for(i=0; i < l_ptr->balcoes_registados;i++) {

      if(l_ptr->balcoes[i].duracao_funcionamento == -1){
        fecha = 1;
        break;
      }
    }

    pthread_mutex_unlock(&(l_ptr->access_lock));


    if(fecha){                    //gerar estatisticas
      int clis=0,t_medio;
      struct tm  ts;
      time_t x;

      for(i=0; i < l_ptr->balcoes_registados;i++){
        ts = *localtime((time_t *)&l_ptr->balcoes[i].inicio_funcionamento);

        strftime(buffer, BUFFER_SIZE, "%Y-%m-%d %H:%M:%S", &ts);
        printf("Balcao %d\n",l_ptr->balcoes[i].id);
        printf("Tempo abertura:%s\n",buffer);
        printf("Cli atendidos:%d\n",l_ptr->balcoes[i].num_cli_atendidos);
        printf("Tempo medio atendimento:%d\n",l_ptr->balcoes[i].tempo_medio_atendimento);
        clis+=l_ptr->balcoes[i].num_cli_atendidos;
        t_medio+=l_ptr->balcoes[i].tempo_medio_atendimento;
      }

      ts = *localtime(&((time_t)l_ptr->tempo_abertura_loja));
      strftime(buffer, BUFFER_SIZE, "%Y-%m-%d %H:%M:%S",&ls);
      printf("Abertura loja:%s\n",buffer);
      printf("Total cli atendidos:%d\n",clis);
      printf("Tempo medio antedimento:%d\n",t_medio/clis);
    } 

    //==============
    //log
    log_loja(argv[1],"Balcao",b.id,"fecha_loja",NULL);    
    
    pthread_mutex_destroy(&balcao_lock);     	    //destroy mutex
    destroy_shared_memory(argv[1],SHM_SIZE);    //close and remove shared memory region 
  }

  return 0;  
}


void* thr_func(void*arg){

  pthread_mutex_lock(&balcao_lock); //espera bloqueante ate conseguir fazer lock
  
  Loja* l_ptr = (Loja*) shm;
  int n;
  
  //log
  log_loja(((Info_atendimento*)arg)->shm_name,"Balcao",((Info_atendimento*)arg)->id_balcao,"inicia_atend_cli",((Info_atendimento*)arg)->fifo_cliente);  
    
  pthread_mutex_lock(&(l_ptr->access_lock)); //lock shm 
  
  n = ((Info_atendimento*)arg)->id_balcao - 1; // indice array balcoes
  l_ptr->balcoes[n].num_cli_atendimento++; 	//incremento nº clientes em atendimento
  
  pthread_mutex_unlock(&(l_ptr->access_lock));
  
  sleep((*(Info_atendimento*)arg).tempo_simula_atendimento); 		  //realiza atendimento
  
  n = open((*(Info_atendimento*)arg).fifo_cliente, O_WRONLY ); 		//abre FIFO cliente para escrita  
  write(n,msg, sizeof(msg));	  					//escreve no fifo do cliente : fim_atendimento  
  close(n);
  
  //actualiza info balcao
  pthread_mutex_lock(&(l_ptr->access_lock)); //lock shm
  
  l_ptr->balcoes[n].num_cli_atendidos++;  
  l_ptr->balcoes[n].num_cli_atendimento--;
  
  if(l_ptr->balcoes[n].tempo_medio_atendimento == 0)
    l_ptr->balcoes[n].tempo_medio_atendimento = (*(Info_atendimento*)arg).tempo_simula_atendimento;
  else  
    l_ptr->balcoes[n].tempo_medio_atendimento = (l_ptr->balcoes[n].tempo_medio_atendimento + (*(Info_atendimento*)arg).tempo_simula_atendimento) / 2 ; 
  
  
  //log
  log_loja(((Info_atendimento*)arg)->shm_name,"Balcao",((Info_atendimento*)arg)->id_balcao,"fim_atendimento",((Info_atendimento*)arg)->fifo_cliente);
  
  pthread_mutex_unlock(&(l_ptr->access_lock));    //unlock mutex de acesso à shm  
  pthread_mutex_unlock(&balcao_lock);		 //unlock mutex balcao
  
  return NULL;
}


//FUNCOES AUXILIARES====================================

//CRIA SHM SE NAO EXISTIR
int create_shared_memory(char* shm_name, int shm_size){
  int shmfd;
  
  //try to create the shared memory region
  shmfd = shm_open(shm_name,O_CREAT|O_EXCL|O_RDWR,0660);
  
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

void destroy_shared_memory(char *shm_name, int shm_size){
  if (munmap(shm,shm_size) < 0){
    fprintf(stderr,"ERRO NA LIBERTACAO DA MEMORIA\n");
    exit(EXIT_FAILURE);
  }
  if (shm_unlink(shm_name) < 0){
    fprintf(stderr,"ERRO NA REMOCAO DA MEMORIA PARTILHADA\n");
    exit(EXIT_FAILURE);
  }
}