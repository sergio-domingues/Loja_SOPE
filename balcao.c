#include "loja.h"

void* thr_func(void*arg);
int create_shared_memory(char* shm_name, int shm_size);
void destroy_shared_memory(char *shm, int shm_size);
//=================

pthread_mutex_t balcao_lock;		//mutex thread related
char* shm; 			              //apontador para inicio shm (mem virtual)
int fd_OWR_b; 		           

void ALARMhandler(int sig) {
  signal(SIGALRM, SIG_IGN);          		/* ignore this signal       */
  close(fd_OWR_b);                     		//terminou tempo, fecha fifo (escrita) neste processo
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

  	//variaveis========================
	int shmfd, create_shm = 0;
	Loja* l_ptr;

	time_t tempo_actual_maquina = time(NULL);
	if(tempo_actual_maquina == -1){
		fprintf(stderr, "ERRO NA OBTENCAO DO TEMPO DA MAQUINA\n");
		exit(1);
	}

	shmfd = shm_open(argv[1],O_RDWR,0777);     //verifica se shmem ja exise

	if(shmfd < 0){
		shmfd = create_shared_memory(argv[1],SHM_SIZE); 
		create_shm = 1;
		fprintf(stderr, "cria shmem\n");
	}

  	//mapeamento para virtual memory
	shm = (char *) mmap(0,SHM_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
	if(shm == MAP_FAILED) { 
		fprintf(stderr, "ERRO NO MAPEAMENTO DA MEM PARTILHADA PARA MEM VIRTUAL\n");
		exit(1);
	}  

	fprintf(stderr,"mapeou\n");
  	//inicializacao primitivas shm=====================
	if(create_shm){    
		Loja loja;

	    //log
		log_loja(argv[1],"Balcao",1,"inicia_mempart", "-");

		loja.tempo_abertura_loja = tempo_actual_maquina;
		loja.balcoes_registados = 0;

	    if (pthread_mutex_init(&(loja.access_lock), NULL) != 0){ 	 //INIT MUTEX SHM
	    	printf("\n access_mutex init failed\n");
	    	exit(EXIT_FAILURE);
	    }
	    
	    l_ptr = (Loja*) shm; 	//inicio shm	    
	    *l_ptr = loja;		 	//grava loja na shmem
	    create_shm = 0;    
	    
	    fprintf(stderr,"criou e abriu loja\n");
	}  

  //============================================================   
  l_ptr = (Loja*) shm;   //pointer para a loja (shm)

  if(l_ptr->balcoes_registados == BALCOES_MAX){			//VERIFICA SE NAO ATINGIU LIMITE DE BALCOES
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
  
  //pthread_mutex_lock(&(l_ptr->access_lock)); //tenta fazer lock | espera bloqueante caso nao consiga
  
  l_ptr->balcoes_registados++;        	//incrementa num de balcoes registados  
  b.id = l_ptr->balcoes_registados; 	//id balcao
  
  int n;  
  char buffer[BUFFER_SIZE];
  pid_t pid = getpid();

  sprintf(buffer,"fb_%d",pid);
  strcpy (b.nome , buffer);
  
  n = sprintf(buffer,"tmp/%s",b.nome);    
  n = mkfifo(buffer,0660);   			//criacao do fifo do balcao
  if(n < 0){
  	fprintf(stderr,"ERRO NA CRIACAO DO FIFO\n");
  	exit(1);
  }
  
  strcpy(b.fifo_nome, buffer);  
  l_ptr->balcoes[b.id-1] = b; 		//regista balcao no array
  
  //pthread_mutex_unlock(&(l_ptr->access_lock)); //faz unlock ao mutex de acesso à shm

  //log  
  log_loja(argv[1],"Balcao",b.id,"cria_linh_mempart",b.nome);  
  //==============

  if (pthread_mutex_init(&balcao_lock, NULL) != 0){ 	//inicia mutex do balcao
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

  int fd_2 = open(b.fifo_nome, O_WRONLY);     		//abre fifo balcao escrita
  if(fd_2 < 0){
  	fprintf(stderr, "ERRO NA ABERTURA DO FIFO escrita\n");
  	exit(EXIT_FAILURE);
  }
  
  fd_OWR_b = fd_2;       //descritor (global) writing
    
  //======================
  //print_loja(l_ptr);      //DEBUG PRINTF
  //======================
  
  signal(SIGALRM, ALARMhandler); //alarme handler

  alarm(atoi(argv[2]));   //agenda alarme duracao funcionamento balcao

	//===========THREADING RELATED PART============================
	while(1){ 

	  int v = readline(fd,buffer); 		//espera bloqueante

	  fprintf(stderr, "passa readline\n");

	  if(v == 0)    	//n ha + clientes para atender | todos os que tinham aberto para escrita, fecharam
	  	break;
	  
	  pthread_t tid;    
	  Info_atendimento* info = (Info_atendimento*)malloc (sizeof(Info_atendimento));

	  strcpy(info->fifo_cliente,buffer);
	  info->id_balcao = b.id ;
	  info->shm_name = argv[1];
	  
	  //CRIA THREAD
	  int err = pthread_create(&tid, NULL, thr_func, info);    
	  
	  if(err != 0){
	  	fprintf(stderr, "ERRO NA CRIACAO DA THREAD DE ATENDIMENTO\n");
	  	exit(EXIT_FAILURE);
	  }
	}

	//log
	log_loja(argv[1],"Balcao",b.id,"fecha_balcao",argv[1]);  

	//pthread_mutex_lock(&(l_ptr->access_lock));
	l_ptr->balcoes[b.id-1].duracao_funcionamento = (int)time(NULL) - (int)b.inicio_funcionamento; //actualiza duracao do balcao

	fprintf(stderr, "funcionamento:%d\n",l_ptr->balcoes[b.id-1].duracao_funcionamento);

	//pthread_mutex_unlock(&(l_ptr->access_lock));

	close(fd);				   //fecha fifo balcao (leitura)
	unlink(b.fifo_nome);      //destroi fifo balcao  
	    
	int i,fecha = 1;
	//pthread_mutex_lock(&(l_ptr->access_lock));

	for(i=0; i < l_ptr->balcoes_registados;i++) {    //verifica se ainda existem balcoes activos
		if(l_ptr->balcoes[i].duracao_funcionamento == -1){
			fecha = 0;
			break;
		}
	}    
	//pthread_mutex_unlock(&(l_ptr->access_lock));   

	//=========== //gerar estatisticas   / fechar loja
	if(fecha==1){                    
		gera_stats(shm);

		log_loja(argv[1],"Balcao",b.id,"fecha_loja",NULL); 	//log

		destroy_shared_memory(argv[1],SHM_SIZE);    		//close and remove shared memory region
		fprintf(stderr, "Fecha loja\n");
	}     
	//=======================================================

	pthread_mutex_destroy(&balcao_lock);     	    //destroy mutex      
	return 0;  
}

//========================================================================
void* thr_func(void*arg){

	fprintf(stderr, "entra thread\n" );

  int tempo_simula_atendimento,id;
  Loja* l_ptr = (Loja*)shm;
  id = ((Info_atendimento*)arg)->id_balcao; 	// id balcao 

   //pthread_mutex_lock(&(l_ptr->access_lock)); //lock shm

  if((l_ptr->balcoes[id-1].num_cli_atendimento) > 10) //maximo de 10 segundos
     tempo_simula_atendimento = 10;
  else
    tempo_simula_atendimento = l_ptr->balcoes[id-1].num_cli_atendimento;

  //pthread_mutex_unlock(&(l_ptr->access_lock)); //lock shm

  fprintf(stderr,"simula:%d\n\n",tempo_simula_atendimento);

  //log
  log_loja(((Info_atendimento*)arg)->shm_name,"Balcao",id,"inicia_atend_cli",((Info_atendimento*)arg)->fifo_cliente);  

  sleep(tempo_simula_atendimento); 		  //realiza atendimento
  
  int fdr = open((*(Info_atendimento*)arg).fifo_cliente, O_WRONLY); 	//abre FIFO cliente para escrita  
  write(fdr,msg, sizeof(msg));	  										//escreve no fifo do cliente : fim_atendimento  
  close(fdr);															//fecha fifo cliente (escrita)
  
  //log
  log_loja(((Info_atendimento*)arg)->shm_name,"Balcao",((Info_atendimento*)arg)->id_balcao,"fim_atendimento",((Info_atendimento*)arg)->fifo_cliente);

  //actualiza info balcao
  //pthread_mutex_lock(&(l_ptr->access_lock)); //lock shm
  
  l_ptr->balcoes[id-1].num_cli_atendidos++;  
  l_ptr->balcoes[id-1].num_cli_atendimento--;
  
  l_ptr->balcoes[id-1].tempo_medio_atendimento = (l_ptr->balcoes[id-1].tempo_medio_atendimento + (*(Info_atendimento*)arg).tempo_simula_atendimento) / 2 ; 
  
  //pthread_mutex_unlock(&(l_ptr->access_lock));    //unlock mutex de acesso à shm  

  free((Info_atendimento*)arg); //liberta memoria

  return NULL;
}

//FUNCOES AUXILIARES====================================

//CRIA SHM SE NAO EXISTIR
int create_shared_memory(char* shm_name, int shm_size){
	int shmfd;

  //try to create the shared memory region
	shmfd = shm_open(shm_name,O_CREAT|O_RDWR,0777);    
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
