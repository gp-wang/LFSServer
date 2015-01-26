/* this is working fine I think it passed the result  */
/* packets are encapsulated correctly */
/* UD send and receive Ok */
/* gw: maybe need to load all imap piece into mem? */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "udp.h"

#include "mfs.h"








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

/******************** MFS start ********************/

char* server_host = NULL;
int server_port = 3000;
int online = 0;



int MFS_Init(char *hostname, int port) {
	server_host = strdup(hostname); /* gw: tbc dubious  */
	server_port = port;
	online = 1;
	return 0;
}


int MFS_Lookup(int pinum, char *name){
	if(!online)
		return -1;
	
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
