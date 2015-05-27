#include "loja.h"

void* thr_func(void*arg);
int create_shared_memory(char* shm_name, int shm_size);
void destroy_shared_memory(char *shm, int shm_size);
//=================

char* shm; 			              //apontador para inicio shm (mem virtual)
int fd_OWR_b; 		           

void ALARMhandler(int sig) {
  signal(SIGALRM, SIG_IGN);          		/* ignore this signal       */
  close(fd_OWR_b);                     		//terminou tempo, fecha fifo (escrita) deste processo(balcao)
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
	}

  	//mapeamento para virtual memory
	shm = (char *) mmap(0,SHM_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
	if(shm == MAP_FAILED) { 
		fprintf(stderr, "ERRO NO MAPEAMENTO DA MEM PARTILHADA PARA MEM VIRTUAL\n");
		exit(1);
	}  

  	//inicializacao loja=====================
	if(create_shm){    
		Loja loja;

	    //log
		log_loja(argv[1],"Balcao",1,"inicia_mempart", "-");

		loja.tempo_abertura_loja = tempo_actual_maquina;
		loja.balcoes_registados = 0;
		loja.tempo_total = 0;

	    if (pthread_mutex_init(&(loja.access_lock), NULL) != 0){ 	 //INIT MUTEX SHM
	    	printf("\n access_mutex init failed\n");
	    	exit(EXIT_FAILURE);
	    }

	    /*if (pthread_mutex_init(&(loja.access_log), NULL) != 0){ 	 //INIT MUTEX log
	    	printf("\n access_mutex init failed\n");
	    	exit(EXIT_FAILURE);
	    }*/
	    
	    l_ptr = (Loja*) shm; 	//inicio shm	    
	    *l_ptr = loja;		 	//grava loja na shmem
	    create_shm = 0;    
	}  

  //============================================================   
  l_ptr = (Loja*) shm;   //pointer para a loja (shm)

  pthread_mutex_lock(&(l_ptr->access_lock));

  if(l_ptr->balcoes_registados == BALCOES_MAX){			//VERIFICA SE NAO ATINGIU LIMITE DE BALCOES
  	fprintf(stderr,"limite de balcoes atingidos\n");
  	exit(EXIT_FAILURE);
  }  
  
  pthread_mutex_unlock(&(l_ptr->access_lock));
  //inicializa atributos Balcao=================================
  
  int n;  
  char buffer[BUFFER_SIZE];
  pid_t pid = getpid();


  Balcao b;  
  b.inicio_funcionamento = tempo_actual_maquina;
  b.duracao_funcionamento = -1;
  b.num_cli_atendidos = 0;
  b.num_cli_atendimento = 0;
  b.tempo_medio_atendimento = 0;  
  
  sprintf(buffer,"fb_%d",pid);
  strcpy (b.nome , buffer);
  
  sprintf(buffer,"tmp/%s",b.nome);    
  n = mkfifo(buffer,0660);   			//criacao do fifo do balcao
  if(n < 0){
  	fprintf(stderr,"ERRO NA CRIACAO DO FIFO BALCAO\n");
  	exit(EXIT_FAILURE);
  }
  
  strcpy(b.fifo_nome, buffer);
  
  pthread_mutex_lock(&(l_ptr->access_lock)); //lock ao mutex de acesso à shm  
  
  l_ptr->balcoes_registados++;        		 //incrementa num de balcoes registados  
  b.id = l_ptr->balcoes_registados; 	     //id balcao 
  l_ptr->balcoes[b.id-1] = b; 				 //regista balcao no array
  
  log_loja(argv[1],"Balcao",b.id,"cria_linh_mempart",b.nome); 
  
  pthread_mutex_unlock(&(l_ptr->access_lock)); 			//liberta mutex shm_access
  //==============  

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
  
  //===========THREADING RELATED PART==================================    
 
    signal(SIGALRM, ALARMhandler); //alarme handler
    alarm(atoi(argv[2]));   //agenda alarme duracao funcionamento balcao

	while(1){ 

	  int v = readline(fd,buffer); 		//espera bloqueante

	  if(v == 0)    	//n ha + clientes para atender | todos os que tinham aberto para escrita, fecharam
	  	break;
	  
	  pthread_t tid;    
	  Info_atendimento* info = (Info_atendimento*) malloc(sizeof(Info_atendimento));

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
	//FECHO BALCAO=========================================

	pthread_mutex_lock(&(l_ptr->access_lock)); //actualiza duracao do balcao
	
	l_ptr->balcoes[b.id-1].duracao_funcionamento = (int)time(NULL) - (int)b.inicio_funcionamento;

	if(l_ptr->balcoes[b.id-1].num_cli_atendidos != 0)	
		l_ptr->balcoes[b.id-1].tempo_medio_atendimento = l_ptr->balcoes[b.id-1].tempo_medio_atendimento / l_ptr->balcoes[b.id-1].num_cli_atendidos;
	
	pthread_mutex_unlock(&(l_ptr->access_lock));

	close(fd);				  //fecha fifo balcao (leitura)
	unlink(b.fifo_nome);      //destroi fifo balcao  
	
	log_loja(argv[1],"Balcao",b.id,"fecha_balcao","-"); 	//log

	int i,fecha = 1;

	pthread_mutex_lock(&(l_ptr->access_lock));
	for(i=0; i < l_ptr->balcoes_registados;i++) {    //verifica se ainda existem balcoes activos
		if(l_ptr->balcoes[i].duracao_funcionamento == -1){
			fecha = 0;
			break;
		}
	}    
	pthread_mutex_unlock(&(l_ptr->access_lock));   

	//gerar estatisticas / fechar loja
	if(fecha==1){		
		gera_stats(shm);
		destroy_shared_memory(argv[1],SHM_SIZE);    		//close and remove shmem region

		log_loja(argv[1],"Balcao",b.id,"fecha_loja","-"); 	//log
		fprintf(stderr, "Fecha loja\n");
	}       
	return 0;  
}

//========================================================================
void* thr_func(void*arg){

  int tempo_simula_atendimento,id;
  Loja* l_ptr = (Loja*)shm;
  id = ((Info_atendimento*)arg)->id_balcao; 				 // id balcao 

  pthread_mutex_lock(&(l_ptr->access_lock)); 				 //lock shmem

  if((l_ptr->balcoes[id-1].num_cli_atendimento) > 10) 		 //maximo de 10 segundos
     tempo_simula_atendimento = 10;
  else
    tempo_simula_atendimento = l_ptr->balcoes[id-1].num_cli_atendimento;

  pthread_mutex_unlock(&(l_ptr->access_lock)); 				 //unlock shmem

  log_loja(((Info_atendimento*)arg)->shm_name,"Balcao",id,"inicia_atend_cli",((Info_atendimento*)arg)->fifo_cliente);  

  sleep(tempo_simula_atendimento); 		  			//realiza atendimento
  
  log_loja(((Info_atendimento*)arg)->shm_name,"Balcao",((Info_atendimento*)arg)->id_balcao,"fim_atendimento",((Info_atendimento*)arg)->fifo_cliente);

  int fdr = open((*(Info_atendimento*)arg).fifo_cliente, O_WRONLY); 	//abre FIFO cliente para escrita  
  write(fdr,msg, sizeof(msg));	  										//escreve no fifo do cliente : fim_atendimento  
  close(fdr);															//fecha fifo cliente (escrita)
  
  //actualiza info balcao
  pthread_mutex_lock(&(l_ptr->access_lock)); 		//lock shm

  l_ptr->balcoes[id-1].num_cli_atendidos++;  
  l_ptr->balcoes[id-1].num_cli_atendimento--;  
  l_ptr->balcoes[id-1].tempo_medio_atendimento += tempo_simula_atendimento;   //variavel acumula tempo, no fim divide-se pelos clientes
  l_ptr->tempo_total+=tempo_simula_atendimento;
  
  pthread_mutex_unlock(&(l_ptr->access_lock));    //unlock shm  

  free((Info_atendimento*)arg); 				  //liberta memoria

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