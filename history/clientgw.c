//#include <stdio.h>
//#include "udp.h"
//#include "mfs.c"

/* gw: maybe need to load all imap piece into mem? */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mfsgw.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "udp.h"

int fd = -1;
MFS_CR_t* p_cr = NULL;

/********************Net lib  ********************/




enum MFS_REQ {
        REQ_INIT,
	REQ_LOOKUP,
	REQ_STAT,
	REQ_WRITE,
	REQ_READ,
	REQ_CREAT,
	REQ_UNLINK,
	REQ_RESPONSE,
	REQ_SHUTDOWN
};



typedef struct __UDP_Packet {
	enum MFS_REQ request;

	int inum;
	int block;
	int type;

	char name[LEN_NAME];
	char buffer[MFS_BLOCK_SIZE];
	MFS_Stat_t stat;
} UDP_Packet;

/******************** UDP lib ********************/
int
UDP_Open(int port)
{
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
	perror("socket");
	return 0;
    }

    // set up the bind
    struct sockaddr_in myaddr;
    bzero(&myaddr, sizeof(myaddr));

    myaddr.sin_family      = AF_INET;
    myaddr.sin_port        = htons(port);
    myaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
	perror("bind");
	close(fd);
	return -1;
    }

    // give back descriptor
    return fd;
}

// fill sockaddr_in struct with proper goodies
int
UDP_FillSockAddr(struct sockaddr_in *addr, char *hostName, int port)
{
    bzero(addr, sizeof(struct sockaddr_in));
    if (hostName == NULL) {
	return 0; // it's OK just to clear the address
    }
    
    addr->sin_family = AF_INET;          // host byte order
    addr->sin_port   = htons(port);      // short, network byte order

    struct in_addr *inAddr;
    struct hostent *hostEntry;
    if ((hostEntry = gethostbyname(hostName)) == NULL) {
	perror("gethostbyname");
	return -1;
    }
    inAddr = (struct in_addr *) hostEntry->h_addr;
    addr->sin_addr = *inAddr;

    // all is good
    return 0;
}

int
UDP_Write(int fd, struct sockaddr_in *addr, char *buffer, int n)
{
    int addrLen = sizeof(struct sockaddr_in);
    int rc      = sendto(fd, buffer, n, 0, (struct sockaddr *) addr, addrLen);
    return rc;
}

int
UDP_Read(int fd, struct sockaddr_in *addr, char *buffer, int n)
{
    int len = sizeof(struct sockaddr_in); 
    int rc = recvfrom(fd, buffer, n, 0, (struct sockaddr *) addr, (socklen_t *) &len);
    // assert(len == sizeof(struct sockaddr_in)); 
    return rc;
}


int
UDP_Close(int fd)
{
    return close(fd);
}




/******************** netlib ********************/
//int sendPacket(char *hostname, int port, UDP_Packet *tx, UDP_Packet *rx, int maxTries)
int UDP_Send( UDP_Packet *tx, UDP_Packet *rx, char *hostname, int port)
{

    int sd = UDP_Open(0);
    if(sd < -1)
    {
        perror("udp_send: failed to open socket.");
        return -1;
    }

    struct sockaddr_in addr, addr2;
    int rc = UDP_FillSockAddr(&addr, hostname, port);
    if(rc < 0)
    {
        perror("upd_send: failed to find host");
        return -1;
    }

    fd_set rfds;
    struct timeval tv;
    tv.tv_sec=3;
    tv.tv_usec=0;

    int trial_limit = 5;	/* trial = 5 */
    do {
        FD_ZERO(&rfds);
        FD_SET(sd,&rfds);
        UDP_Write(sd, &addr, (char*)tx, sizeof(UDP_Packet));
        if(select(sd+1, &rfds, NULL, NULL, &tv))
        {
            rc = UDP_Read(sd, &addr2, (char*)rx, sizeof(UDP_Packet));
            if(rc > 0)
            {
                UDP_Close(sd);
                return 0;
            }
        }else {
            trial_limit --;
        }
    }while(1);
}

/* int sendPacket(char *hostname, int port, UDP_Packet *sentPacket, UDP_Packet *responsePacket, int maxTries) */
/* { */
/*     int sd = UDP_Open(0); */
/*     if(sd < -1) */
/*     { */
/*         perror("Error opening connection.\n"); */
/*         return -1; */
/*     } */

/*     struct sockaddr_in addr, addr2; */
/*     int rc = UDP_FillSockAddr(&addr, hostname, port); */
/*     if(rc < 0) */
/*     { */
/*         perror("Error looking up host.\n"); */
/*         return -1; */
/*     } */

/*     fd_set rfds; */
/*     struct timeval tv; */
/*     tv.tv_sec=3; */
/*     tv.tv_usec=0; */


/*     do { */
/*         FD_ZERO(&rfds); */
/*         FD_SET(sd,&rfds); */
/*         UDP_Write(sd, &addr, (char*)sentPacket, sizeof(UDP_Packet)); */
/*         if(select(sd+1, &rfds, NULL, NULL, &tv)) */
/*         { */
/*             rc = UDP_Read(sd, &addr2, (char*)responsePacket, sizeof(UDP_Packet)); */
/*             if(rc > 0) */
/*             { */
/*                 UDP_Close(sd); */
/*                 return 0; */
/*             } */
/*         }else { */
/*             maxTries -= 1; */
/*         } */
/*     }while(1); */
/* } */


/******************** MFS start ********************/

char* server_host = NULL;
int server_port = 3000;
int online = 0;



int MFS_Init(char *hostname, int port) {
	/* if(port < 0 || strlen(hostname) < 1) */
	/* 	return -1; */
  //	server_host = malloc(strlen(hostname) + 1);
	//	strcpy(server_host, hostname);
	server_host = strdup(hostname); /* gw: tbc dubious  */
	server_port = port;
	online = 1;
	return 0;
}

/* int MFS_Init(char *hostname, int port) { */
/* 	if(port < 0 || strlen(hostname) < 1) */
/* 		return -1; */
/* 	server_host = malloc(strlen(hostname) + 1); */
/* 	strcpy(server_host, hostname); */
/* 	server_port = port; */
/* 	online = 1; */
/* 	return 0; */
/* } */


/* int MFS_Lookup(int pinum, char *name){ */
/* 	if(!online) */
/* 		return -1; */
	
/* 	if(checkName(name) < 0) */
/* 		return -1; */

/* 	UDP_Packet sentPacket; */
/* 	UDP_Packet responsePacket; */

/* 	sentPacket.inum = pinum; */
/* 	sentPacket.request = REQ_LOOKUP; */
/* 	printf("\nMFS_Lookup: name is %s ",name); */
/* 	strcpy((char*)&(sentPacket.name), name); */
/* 	printf("\nMFS_Lookup: Packet contained name is %s ",sentPacket.name); */
/* 	int rc = sendPacket(server_host, server_port, &sentPacket, &responsePacket, 3); */
/* 	if(rc < 0) */
/* 		return -1; */
	
/* 	rc = responsePacket.inum; */
/* 	return rc; */
/* } */


int MFS_Lookup(int pinum, char *name){
	if(!online)
		return -1;
	
	//	if(checkName(name) < 0)
	//	if(strlen(name)>60)	/* gw: tbc */
	if(strlen(name) > 60 || name == NULL)
		return -1;

	UDP_Packet tx;
	UDP_Packet rx;

	tx.inum = pinum;
	tx.request = REQ_LOOKUP;

	strcpy((char*)&(tx.name), name);

	if(UDP_Send( &tx, &rx, server_host, server_port) < 0)
	  return -1;
	else
	  return rx.inum;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	if(!online)
		return -1;

	UDP_Packet tx;
	tx.inum = inum;
	tx.request = REQ_STAT;


	UDP_Packet rx;
	if(UDP_Send( &tx, &rx, server_host, server_port) < 0)
		return -1;
	m->type = rx.stat.type;
	m->size = rx.stat.size;

	return 0;
}

int MFS_Write(int inum, char *buffer, int block){
	int i = 0;
	if(!online)
		return -1;
	
	UDP_Packet tx;
	UDP_Packet rx;

	tx.inum = inum;

	for(i=0; i<MFS_BLOCK_SIZE; i++)
	  tx.buffer[i]=buffer[i];

	tx.block = block;
	tx.request = REQ_WRITE;
	
	if(UDP_Send( &tx, &rx, server_host, server_port) < 0)
		return -1;
	
	return rx.inum;
}

int MFS_Read(int inum, char *buffer, int block){
  int i = 0;
  if(!online)
    return -1;
	
  UDP_Packet tx;


  tx.inum = inum;
  tx.block = block;
  tx.request = REQ_READ;

  UDP_Packet rx;	
  if(UDP_Send( &tx, &rx, server_host, server_port) < 0)
    return -1;

  if(rx.inum > -1) {
    for(i=0; i<MFS_BLOCK_SIZE; i++)
      buffer[i]=rx.buffer[i];
  }

	
  return rx.inum;
}

int MFS_Creat(int pinum, int type, char *name){
	if(!online)
		return -1;
	
	//	if(checkName(name) < 0)
	if(strlen(name) > 60 || name == NULL)
		return -1;

	UDP_Packet tx;

	strcpy(tx.name, name);
	tx.inum = pinum;
	tx.type = type;
	tx.request = REQ_CREAT;

	UDP_Packet rx;	
	if(UDP_Send( &tx, &rx, server_host, server_port) < 0)
		return -1;

	return rx.inum;
}

int MFS_Unlink(int pinum, char *name){
	if(!online)
		return -1;
	
	//	if(checkName(name) < 0)
	if(strlen(name) > 60 || name == NULL)
		return -1;
	
	UDP_Packet tx;

	tx.inum = pinum;
	tx.request = REQ_UNLINK;
	strcpy(tx.name, name);

	UDP_Packet rx;	
	if(UDP_Send( &tx, &rx,server_host, server_port ) < 0)
		return -1;

	return rx.inum;
}

int MFS_Shutdown(){
  UDP_Packet tx;
	tx.request = REQ_SHUTDOWN;

	UDP_Packet rx;
	if(UDP_Send( &tx, &rx,server_host, server_port) < 0)
		return -1;
	
	return 0;
}

/* int checkName(char* name) { */
/* 	if(strlen(name) > 60) */
/* 		return -1; */
/* 	return 0; */
/* } */


int
main(int argc, char *argv[])
{
	printf("Starting Client\n");

	printf("Sizes %u %u %u\n", (unsigned int)sizeof(MFS_CR_t), (unsigned int)sizeof(MFS_Imap_t), (unsigned int)sizeof(MFS_Inode_t));

	MFS_Init("localhost", 3003);

	int rc = MFS_Creat(0, MFS_REGULAR_FILE, "test");
	if(rc < 0){
		printf("Failed at Creat 1\n");
		exit(0);
	}
	printf("Passed Creat 1\n");

	rc = MFS_Creat(0, MFS_DIRECTORY, "test_dir");
	if(rc < 0){
		printf("Failed at Creat 2\n");
	}
	printf("Passed Creat 2\n");

	int inum = MFS_Lookup(0, "test");
	if(inum < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup 1\n");

	int inum2 = MFS_Lookup(0, "test_dir");
	if(inum2 < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup 1\n");


	rc = MFS_Creat(inum2, MFS_REGULAR_FILE, "test");
	if(rc < 0){
		printf("Failed at Creat 3\n");
		exit(0);
	}
	printf("Passed Creat 3\n");

 	inum2 = MFS_Lookup(inum2, "test");
	if(inum2 < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup\n");


	char* tx_buffer = malloc(MFS_BLOCK_SIZE);
	char* tx_buffer2 = malloc(MFS_BLOCK_SIZE);
	char* rx_buffer = malloc(MFS_BLOCK_SIZE);
	strcpy(tx_buffer, "This is just a test!");
	strcpy(tx_buffer2, "Is this a test?");
	
	//	rc = MFS_Write(inum, tx_buffer, 2);
	rc = MFS_Write(inum, tx_buffer, 1);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	MFS_Read(inum, rx_buffer, 1);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	printf("Passed Write\n");

	if(strcmp(rx_buffer, tx_buffer) != 0){
		printf("%s - %s\n", rx_buffer, tx_buffer);
		printf("Failed at Write - Strings does not match\n");
		exit(0);
	}
	printf("Passed Write\n");
	rc = MFS_Write(inum, tx_buffer2, 2);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	printf("Passed Write\n");
	MFS_Read(inum, rx_buffer, 2);
	if(rc == -1){
		printf("Failed at Write\n");
		exit(0);
	}
	printf("Passed Write\n");
	if(strcmp(rx_buffer, tx_buffer2) != 0){
		printf("%s - %s\n", rx_buffer, tx_buffer2);
		printf("Failed at Write - Strings does not match\n");
		exit(0);
	}
	printf("Passed Write\n");
	//	exit(0);

	rc = MFS_Unlink(0, "test");
	if(rc == -1){
		printf("Failed at Unlink 1\n");
		exit(0);
	}
	printf("Passed Unlink\n");

	inum = MFS_Lookup(0, "test");
	if(inum >= 0){
		printf("Failed at Lookup Ghost\n");
		exit(0);
	}
	printf("Passed Lookup Ghost\n");
	
	rc = MFS_Unlink(0, "test");
	if(rc == -1){
		printf("failed at unlink 2\n");
		exit(0);
	}
	printf("Passed unlink 2\n");

	rc = MFS_Unlink(0, "test_dir");
	if(rc != -1){
		printf("failed at unlink 3\n");
		exit(0);
	}
	printf("Passed unlink 3\n");

	inum2 = MFS_Lookup(0, "test_dir");
	if(inum2 < 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup\n");

	rc = MFS_Unlink(inum2, "test");
	if(rc == -1){
		printf("failed at unlink 4\n");
		exit(0);
	}
	printf("Passed unlink 4\n");
	
	inum2 = MFS_Lookup(inum2, "test");
	if(inum2 >= 0){
		printf("Failed at Lookup\n");
		exit(0);
	}
	printf("Passed Lookup\n");

	rc = MFS_Unlink(0, "test_dir");
	if(rc == -1){
		printf("failed at unlink 5\n");
		exit(0);
	}
	printf("Passed unlink 5\n");

	printf("\nall test passed!\n");
	return 0;
}
