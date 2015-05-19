#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // For O_* constants
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> 
#include <unistd.h>

#define BUFFER_SIZE 50

int main(int argc,char* argv[]){
  
  if(argc!=3){   
    fprintf(stderr,"Wrong no of arguments.\nUsage: balcao <nome_mempartilhada> <tempo_abertura> \n");
    exit(1);
  }
  
  if(atoi(argv[2]) <= 0){
    fprintf(stderr,"O TEMPO DE ABERTURA DO BALCAO DEVE SER SUPERIOR A ZERO!\n");
    exit(1);
  }

  char* SHM_NAME = argv[1];
  
  int nrClientes=argv[2];
  
  int shmfd;
  
  //try to create the shared memory region
  shmfd = shm_open(SHM_NAME,O_CREAT|O_EXCL|O_RDWR,0660);
  
  if(shmfd != EEXIST){ //SE JA EXISTIR RETORNA-1
		fprintf(stderr, "ERRO MEMORIA PARTILHADA NAO CRIADA\n");
		exit(1);
  }
  
  //mapeamento para virtual memory
  shm = (char *) mmap(0,SHM_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
  
  if(shm == MAP_FAILED) { 
    fprintf(stderr, "ERRO NO MAPEAMENTO DA MEM PARTILHADA PARA MEM VIRTUAL\n");
    exit(2);
  }
  
  pid_t pid;
  int n;
  
  while(nrClientes>0){
	pid=fork();
	
	if(pid == -1){
      fprintf(stderr, "FORK ERROR\n");
      exit(1);
    }
	
	//filho
	if(pid==0){
		n = sprintf(buffer,"./tmp/fc_%d",COCO); //nao sei o que meter Será que já é orfao????
  
	if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO NOME DO FIFO (SPRINTF)\n");
    exit(1);
	}
  
	n = mkfifo(buffer,0660); 
  
	if(n < 0){
    fprintf(stderr,"ERRO NA CRIACAO DO FIFO\n");
    exit(1);
	}
	}
	
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