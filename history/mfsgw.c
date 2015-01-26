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

int server_init(char* , int );
int server_lookup(int, char* );
int server_stat(int , MFS_Stat_t* );
int server_write(int , char* , int );
int server_read(int , char* , int );
int server_creat(int , int , char* );
int server_unlink(int , char* );
int server_shutdown();
int print_block(int, int);


/* TODO: not wrapped up */
/* lacking protocols */
//int main(int argc, char** argv){
/* => */
int server_init(char* hostname, int port){

	//Get arguments
	int port = atoi(argv[1]);
	char* image_path = argv[2];
	//int sd = UDP_Open(port);
	//	assert(sd > -1);

	//Open up the image file
	fd = open(image_path, O_RDWR|O_CREAT, S_IRWXU);
	if (fd < 0) {
		perror("server_init: Cannot open file");
	}
	// Make a copy in memory
	struct stat fs;
	if(fstat(fd,&fs) < 0) {
		perror("server_init: Cannot open file");
	}

	//Put image in memory
	int rc, i, j;

	/* gw: */
	//        MFS_CR_t* p_cr = (MFS_CR_t *)malloc(sizeof(MFS_CR_t));
	p_cr = (MFS_CR_t *)malloc(sizeof(MFS_CR_t));
	int sz = 0;
	int offset = 0, step = 0;
	
	if(fs.st_size < sizeof(MFS_CR_t)){

	  /* new file */
	  fd = open(image_path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
	  if(fd <0)
	    return -1;

	  // Make a copy in memory
	  if(fstat(fd,&fs) < 0) {
	    perror("server_init: fstat error");
	    return -1;
	  }
       
	  p_cr->inode_count = 0;
	  p_cr->end_log = sizeof(MFS_CR_t);
	  for(i=0; i<NUM_IMAP; i++)
	    p_cr->imap[i] = -1;

	  /* write content on the file using lseek and write */
	  lseek(fd, 0, SEEK_SET);
	  write(fd, p_cr, sizeof(MFS_CR_t));

	  /* create and write to block 0, directory */
	  MFS_DirDataBlock_t db;
	  for(i=0; i< NUM_DIR_ENTRIES; i++){
	    strcpy(db.entries[i].name, "\0");
	    db.entries[i].inum = -1;
	  }
	  strcpy(db.entries[0].name, ".\0");
	  db.entries[0].inum = 0; /* gw: correct */
	  //	  db.entries[0].inum = 1; /* misc test change back later */
	  strcpy(db.entries[1].name, "..\0");
      	  db.entries[1].inum = 0;
	  //	  db.entries[1].inum = 2; /* misc test change back later */

	  /* GW: how??? */
	  offset = p_cr->end_log;
	  step = MFS_BLOCK_SIZE; /* DirDataBlock should be rounded to 4096 */
	  p_cr->end_log += step;
	  lseek(fd, offset, SEEK_SET);
	  write(fd, &db, sizeof(MFS_DirDataBlock_t));

	  MFS_Inode_t nd;
	  nd.size = 0; /* gw: tbc, use 0 for dir (prf) */
	  nd.type = MFS_DIRECTORY;
	  for (i = 0; i < 14; i++) nd.data[i] = -1; /* 14 pointers per inode */
	  nd.data[0] = offset;			    /* gw: right, inode contains the actual address of datablock */
	  //	  nd.data[1] = offset;

	  offset = p_cr->end_log;
	  step = sizeof(MFS_Inode_t); /* inode size */
	  p_cr->end_log += step;
	  lseek(fd, offset, SEEK_SET);
	  write(fd, &nd, step);


	  MFS_Imap_t mp;
	  for(i = 0; i< IMAP_PIECE_SIZE; i++) mp.inodes[i] = -1;
	  mp.inodes[0] = offset; /* gw: right, imap translate the inode number "0" to inode's address */

	  offset = p_cr->end_log;
	  step = sizeof(MFS_Imap_t); /* inode size */
	  p_cr->end_log += step;
	  lseek(fd, offset, SEEK_SET);
	  write(fd, &mp, step);



	  /* now the first imap and inode is written, we update imap table */
	  /* this update is done after the (imap, inode, datablock creation is donea altogether ) -> atomic */
	  /* lseek(fd, 0 + 2*sizeof(int), SEEK_SET); /\* imap[0] in cr *\/ */
	  /* write(fd, &offset , sizeof(int));	  /\* write offset (the newest imap's absolute offset) into p_cr *\/ */
	  //	  p_cr->imap[0] = offset; /* offset of starting point of mp */
	  //	  p_cr->imap[0] = (int)offset;
	  p_cr->imap[0] = offset; /* gw: right, contains the address of imap piece "0" */
	  lseek(fd, 0, SEEK_SET);
	  write(fd, p_cr, sizeof(MFS_CR_t));


	  fsync(fd);


	  printf("\n server_init: new file created\n");

	  printf("Initializing new file\n");

	}else{
	  //		image_size = fileStat.st_size + MFS_BYTE_STEP_SIZE;
	  	printf("Using old file of size %d\n", (int)fs.st_size);
		//		header = (MFS_CR_t *)malloc(image_size);
		// Put text in memory



		/* rc = read(fd, header, fs.st_size); */
		/* if(rc < 0){ */
		/* 	error("Cannot open file"); */
		/* } */

		/* gw: read in cr region, including imap */
		lseek(fd,0, SEEK_SET);
		read(fd, p_cr, sizeof(MFS_CR_t));



		/********** test output **********/
		int root_inum = 0;
		int root_imap_offset = p_cr->imap[0];
		MFS_Imap_t root_imap;
		lseek(fd, root_imap_offset, SEEK_SET);
		read(fd, &root_imap, sizeof(MFS_Imap_t));

		root_inum = root_imap.inodes[0];
		MFS_Inode_t root_inode;
		lseek(fd, root_inum, SEEK_SET);
		read(fd, &root_inode, sizeof(MFS_Inode_t));

		int root_data = root_inode.data[0];
		MFS_DirDataBlock_t root;
		//		void* db_buf = malloc(4096);
		char db_buf[MFS_BLOCK_SIZE];
		lseek(fd, root_data, SEEK_SET);
		read(fd, db_buf, 4096); /* recall we store in 4096 byte */
		root = *(MFS_DirDataBlock_t*)db_buf;


		printf("\n The root directory contains: \n");

		for(i = 0; i< NUM_DIR_ENTRIES; i++) {
		  if(root.entries[i].inum == -1) continue;
		  printf("\n%s %d", &root.entries[i].name[0], root.entries[i].inum );
		}

		/* int result = server_lookup(0, "."); */
		/* printf("\n server_lookup(0, \".\"): %d \n", result); */

		/* result = server_lookup(0, ".."); */
		/* printf("\n server_lookup(0, \"..\"): %d \n", result); */


		//		strcpy(root.entries[0].name, "test_write\0");
		//		strcpy(root.entries[1].name, "test_write_2\0");

		int pinum0 = 0;
		char* name0 = "name0";
		server_creat(pinum0, MFS_DIRECTORY, name0);
		int inum0 = server_lookup(pinum0, name0);

		int pinum1 = inum0;
		char* name1 = "name1";
		server_creat(pinum1, MFS_DIRECTORY, name1);
		int inum1 = server_lookup(pinum1, name1);

		int pinum2 = inum1;
		char* name2 = "name2";
		server_creat(pinum2, MFS_REGULAR_FILE, name2);
		int inum2 = server_lookup(pinum2, name2);

		printf("\n\n Under directory root\t (inum = 0) : ");
		print_block(0,0);

		printf("\n\n Under directory %s\t (inum = %d): ", name0, inum0);
		print_block(inum0, 0);

		printf("\n\n Under directory %s\t (inum = %d): ", name1, inum1);
		print_block(inum1, 0);

		/* not a directory */
		print_block(inum2, 0);

		/* => Write Unlink Test and debug */
		server_unlink(pinum2,name2);

		printf("\n\n After unlinking inode with name: %s from parent inode:  %d", name2, pinum2);
		printf("\n\n Under directory %s\t (inum = %d): ", name1, inum1);
		print_block(pinum2, 0);

		server_unlink(pinum1,name1);

		printf("\n\n After unlinking inode with name: %s from parent inode:  %d", name1, pinum1);
		printf("\n\n Under directory %s\t (inum = %d): ", name0, inum0);
		print_block(pinum1, 0);

		server_unlink(pinum0,name0);

		printf("\n\n After unlinking inode with name: %s from parent inode:  %d", name0, pinum0);
		printf("\n\n Under directory %s\t (inum = %d): ", "root", pinum0);
		print_block(pinum0, 0);



		server_unlink(pinum0,name0);

		printf("\n\n After unlinking inode with name: %s from parent inode:  %d", name0, pinum0);
		printf("\n\n Under directory %s\t (inum = %d): ", "root", pinum0);
		print_block(pinum0, 0);



	}
	return 0;




}


int server_lookup(int pinum, char* name){

  int i=0, j=0, k=0, l=0;

  if(pinum < 0 || pinum >= TOTAL_NUM_INODES)
    {
      perror("server_lookup: invalid pinum_1");
      return -1;
    }

  k = pinum / IMAP_PIECE_SIZE; /* imap piece num */
  if(p_cr->imap[k] == -1){
    perror("server_lookup: invalid pinum_2");
    return -1;
  }
  int fp_mp =  p_cr->imap[k];


  /* nav to mp */
  l = pinum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
  MFS_Imap_t mp;
  lseek(fd, fp_mp, SEEK_SET);
  read(fd, &mp, sizeof(MFS_Imap_t));
  int fp_nd = mp.inodes[l]; /* gw: fp denotes file pointer (location within a file) */
  if(fp_nd == -1) {
    perror("server_lookup: invalid pinum_3");
    return -1;
  }


  /* nav to inode */
  MFS_Inode_t nd;
  lseek(fd, fp_nd, SEEK_SET);
  read(fd, &nd, sizeof(MFS_Inode_t));
  /* assert dir */
  if(nd.type != MFS_DIRECTORY) {
    perror("server_lookup: invalid pinum_4");
    return -1;
  }
  int fp_data = nd.data[0];  /* get fp_data */
  int sz_data = nd.size;
  int num_blocks = sz_data / MFS_BLOCK_SIZE + 1;



  /* read the datablock pointed by this nd */
  //  void* data_buf = malloc(MFS_BLOCK_SIZE);
  char data_buf[MFS_BLOCK_SIZE]; /* gw: use char buffer, (no need free) */
  //  for(i=0; i< num_blocks; i++) {
  for(i=0; i< NUM_INODE_POINTERS; i++) {
//    int fp_block = fp_data + i * MFS_BLOCK_SIZE;
    int fp_block = nd.data[i];	/* gw: tbc */
    if(fp_block == -1) continue;


    lseek(fd, fp_block, SEEK_SET);
    read(fd, data_buf, MFS_BLOCK_SIZE);
	  
    MFS_DirDataBlock_t* dir_buf = (MFS_DirDataBlock_t*)data_buf;
    for(j=0; j<NUM_DIR_ENTRIES; j++) {
      MFS_DirEnt_t* p_de = &dir_buf->entries[j];
      if(strcmp(p_de->name,name) == 0)
	return p_de->inum;
    }
  }
  perror("server_lookup: invalid name_5");
  return -1;
}


int server_stat(int inum, MFS_Stat_t* m){

  int i=0, j=0, k=0, l=0;

  if(inum < 0 || inum >= TOTAL_NUM_INODES) {
    perror("server_stat: invalid inum_1");
    return -1;
  }

  k = inum / IMAP_PIECE_SIZE; /* imap piece num */
  if(p_cr->imap[k] == -1){
    perror("server_stat: invalid inum_2");
    return -1;
  }
  int fp_mp =  p_cr->imap[k];


  /* nav to mp */
  l = inum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
  MFS_Imap_t mp;
  lseek(fd, fp_mp, SEEK_SET);
  read(fd, &mp, sizeof(MFS_Imap_t));
  int fp_nd = mp.inodes[l]; /* gw: fp denotes file pointer (location within a file) */
  if(fp_nd == -1) {
    perror("server_stat: invalid inum_3");
    return -1;
  }


  /* nav to inode */
  MFS_Inode_t nd;
  lseek(fd, fp_nd, SEEK_SET);
  read(fd, &nd, sizeof(MFS_Inode_t));


  m->size = nd.size;
  m->type = nd.type;

  return 0;
}


int server_write(int inum, char* buffer, int block){

  int i=0, j=0, k=0, l=0;
  int offset = 0, step =0;
  int is_old_mp = 0, is_old_nd = 0, is_old_block = 0;
  int fp_mp = -1, fp_nd = -1, fp_block = -1;

  MFS_Inode_t nd;
  MFS_Imap_t mp;

  if(inum < 0 || inum >= TOTAL_NUM_INODES) {
    perror("server_write: invalid inum_1");
    return -1;
  }


  if( block < 0 || block > NUM_INODE_POINTERS) {
    perror("server_write: invalid block_5");
    return -1;
  }


  /* generate clean buffer */
  char* ip = NULL;
  char wr_buffer[ MFS_BLOCK_SIZE ];
  /* gw: wipe out the remaining bytes, if any */
  for(i=0, ip=buffer; i<MFS_BLOCK_SIZE; i++) {
    if( ip != NULL ) {
      wr_buffer[i] = *ip;
      ip++;
    } 
    else {
      wr_buffer[i] = '\0';
    }
  }

  /* set default offset if not old mp */
  offset = p_cr->end_log;

  /* fs operations starts */
  k = inum / IMAP_PIECE_SIZE; /* imap piece num */
  fp_mp =  p_cr->imap[k];
  if(fp_mp != -1){
    /* old mp exist */
    is_old_mp = 1;

    /* nav to that mp */
    l = inum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
    /* move def out for later mp_new creation */
    //    MFS_Imap_t mp;
    lseek(fd, fp_mp, SEEK_SET);
    read(fd, &mp, sizeof(MFS_Imap_t));
    fp_nd = mp.inodes[l]; /* gw: fp denotes file pointer (location within a file) */

    /* offset will remain default until confirm old block */
  }


  if(fp_nd != -1 && is_old_mp) { /* also skip if not old mp  */
    /* old nd exist */
    is_old_nd = 1;
    /* nav to inode */
    /* moved def out for later nd_new creation */
    //    mfs_Inode_t nd;
    lseek(fd, fp_nd, SEEK_SET);
    read(fd, &nd, sizeof(MFS_Inode_t));
    /* assert dir */
    /* gw: temp disable */
    if(nd.type != MFS_REGULAR_FILE) {
      perror("server_write: not a regular file_4");
      return -1;
    }
    int fp_data = nd.data[0];  /* get fp_data */
    int sz_data = nd.size;
    int num_blocks = sz_data / MFS_BLOCK_SIZE + 1;
    fp_block = nd.data[block]; 

    /* offset will remain default until confirm old block */

  }


  /* write the datablock pointed by this nd and block # */
  if(fp_block != -1 && is_old_nd && is_old_mp) {
    /* existing block */
    is_old_block = 1;
    /* only in this case will offset not be p_cr->end_log */
    offset = fp_block;
  }


  /* writing action */
  /* offset is defined either as default (p_cr->end_log) or a pre-existing block */
  step = MFS_BLOCK_SIZE; /* DirDataBlock should be rounded to 4096 */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, wr_buffer, MFS_BLOCK_SIZE);


  /* new nd */
  MFS_Inode_t nd_new;
  if(is_old_nd) {
    nd_new.size = nd.size + MFS_BLOCK_SIZE;
    nd_new.type = nd.type;
    for (i = 0; i < 14; i++) nd_new.data[i] = nd.data[i]; /* copy data from old nd */
    nd_new.data[block] = offset;			  /* fp_block_new */
  }
  else {
    nd_new.size = 0;
    //    nd_new.type = MFS_DIRECTORY;			  /* gw: tbc */
    nd_new.type = MFS_REGULAR_FILE;		  /* gw: tbc, likely this because write dont apply to dir */
    for (i = 0; i < 14; i++) nd_new.data[i] = -1; /* copy data from old nd */
    nd_new.data[block] = offset;			  /* fp_block_new */
  }



  offset = p_cr->end_log;	/* after the latestly created block */
  step = sizeof(MFS_Inode_t); /* inode size */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, &nd_new, step);


  /* update imap */
  MFS_Imap_t mp_new;
  if(is_old_mp) {
    for(i = 0; i< IMAP_PIECE_SIZE; i++) mp_new.inodes[i] = mp.inodes[i] ; /* copy old mp's data, mp is still in memory */
    mp_new.inodes[l] = offset; 	/* fp_nd_new */
  }
  else {
    for(i = 0; i< IMAP_PIECE_SIZE; i++) mp_new.inodes[i] = -1 ; /* copy old mp's data, mp is still in memory */
    mp_new.inodes[l] = offset; 	/* fp_nd_new */
  }

  offset = p_cr->end_log;
  step = sizeof(MFS_Imap_t); /* inode size */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, &mp_new, step);

  /* update cr */
  /* now the new imap and inode is written, we update imap table */
  p_cr->imap[k] = offset; 	/* gw: fp_mp_new */
  lseek(fd, 0, SEEK_SET);
  write(fd, p_cr, sizeof(MFS_CR_t));

  fsync(fd);
  return 0;

}


int server_read(int inum, char* buffer, int block){

  int i=0, j=0, k=0, l=0;

  if(inum < 0 || inum >= TOTAL_NUM_INODES)
    {
      perror("server_read: invalid inum_1");
      return -1;
    }

  k = inum / IMAP_PIECE_SIZE; /* imap piece num */
  if(p_cr->imap[k] == -1){
    perror("server_read: invalid inum_2");
    return -1;
  }
  int fp_mp =  p_cr->imap[k];


  /* nav to mp */
  l = inum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
  MFS_Imap_t mp;
  lseek(fd, fp_mp, SEEK_SET);
  read(fd, &mp, sizeof(MFS_Imap_t));
  int fp_nd = mp.inodes[l]; /* gw: fp denotes file pointer (location within a file) */
  if(fp_nd == -1) {
    perror("server_read: invalid inum_3");
    return -1;
  }


  /* nav to inode */
  MFS_Inode_t nd;
  lseek(fd, fp_nd, SEEK_SET);
  read(fd, &nd, sizeof(MFS_Inode_t));
  /* assert dir */
  if(!(nd.type == MFS_REGULAR_FILE || nd.type == MFS_DIRECTORY)) {
    perror("server_read: not a valid file_4");
    return -1;
  }
  int fp_data = nd.data[0];  /* get fp_data */
  int sz_data = nd.size;
  int num_blocks = sz_data / MFS_BLOCK_SIZE + 1;

  if( block < 0 || block > NUM_INODE_POINTERS) {
    perror("server_read: invalid block_5");
    return -1;
  }

  /* get size of buffer */
  char* ip = NULL;
  //  char wr_buffer[ MFS_BLOCK_SIZE ];


  /* int sz_buffer = 0; */
  /* for(ip = buffer, sz_buffer = 0; ip != NULL; ip++, sz_buffer++) */
  /*   ; */
  /* sz_buffer = sz_buffer > MFS_BLOCK_SIZE ? MFS_BLOCK_SIZE: sz_buffer; */

  /* gw: wipe out the remaining bytes, if any */
  /* for(i=0, ip=buffer; i<MFS_BLOCK_SIZE; i++) { */
  /*   if( ip != NULL ) { */
  /*     wr_buffer[i] = *ip; */
  /*     ip++; */
  /*   }  */
  /*   else { */
  /*     wr_buffer[i] = '\0'; */
  /*   } */
  /* } */

  /* write the datablock pointed by this nd and block # */
  //  int fp_block = fp_data + block * MFS_BLOCK_SIZE;
  int fp_block = nd.data[block]; /* gw: tbc */
  lseek(fd, fp_block, SEEK_SET);
  read(fd, buffer, MFS_BLOCK_SIZE); /* gw: tbc */


  return 0;
}


int server_creat(int pinum, int type, char* name){

  int i=0, j=0, k=0, l=0;
  int offset = 0, step =0;
  int is_old_mp = 0, is_old_nd = 0, is_old_block = 0;
  //  int fp_mp_par = -1, fp_nd_par = -1, fp_block_par = -1;


  int fp_mp = -1, fp_nd = -1, fp_block = -1;

  MFS_Inode_t nd;
  MFS_Imap_t mp;


  if(pinum < 0 || pinum >= TOTAL_NUM_INODES) {
    perror("server_creat: invalid pinum_1");
    return -1;
  }

  int len_name = 0;
  for(i=0; name[i] != '\0'; i++, len_name ++)
    ;
  if(len_name > LEN_NAME) {
    perror("server_creat: name too long_1");
    return -1;
  }

  /* if exists, creat is success */
  if(server_lookup(pinum, name) != -1) {
    return 0;
  }


  /* here we know it does not exist */
  /********** pinum **********/

  k = pinum / IMAP_PIECE_SIZE; /* imap piece num */
  fp_mp =  p_cr->imap[k];
  if(fp_mp == -1){
    perror("server_creat: invalid pinum_2");
    return -1;
  }

  /* nav to mp */
  l = pinum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
  MFS_Imap_t mp_par;	       /* imap piece of parent */
  lseek(fd, fp_mp, SEEK_SET);
  read(fd, &mp_par, sizeof(MFS_Imap_t));
  fp_nd = mp_par.inodes[l]; /* gw: fp denotes file pointer (location within a file) */
  if(fp_nd == -1) {
    perror("server_creat: invalid pinum_3");
    return -1;
  }


  /* nav to inode */
  MFS_Inode_t nd_par;
  lseek(fd, fp_nd, SEEK_SET);
  read(fd, &nd_par, sizeof(MFS_Inode_t));
  /* assert dir */
  if(nd_par.type != MFS_DIRECTORY) {
    perror("server_creat: invalid pinum_4");
    return -1;
  }


  /* get the next free inode */
  int free_inum = -1;
  for(i=0; i< TOTAL_NUM_INODES; i++) {
    int ik = i / IMAP_PIECE_SIZE;
    int il = i % IMAP_PIECE_SIZE;

    int i_fp_mp = p_cr->imap[k];
    if(i_fp_mp == -1) { 
      free_inum = ik * IMAP_PIECE_SIZE;
      break;
    }

    MFS_Imap_t i_mp;
    lseek(fd, i_fp_mp, SEEK_SET);
    read(fd, &i_mp, sizeof(MFS_Imap_t));

    for(j=0; j< IMAP_PIECE_SIZE; j++) {
      int i_fp_nd = i_mp.inodes[j];
      if(i_fp_nd == -1) {
	free_inum = ik * IMAP_PIECE_SIZE + j;
	break;
      }
    }
    if(free_inum != -1) break;
  }
  if(free_inum == -1) {
    perror("server_creat: cannot find free inode_5 ");
    return -1;
  }


  /* read the datablock pointed by this nd */
  //  void* data_buf = malloc(MFS_BLOCK_SIZE);
  char data_buf[MFS_BLOCK_SIZE]; /* gw: use char buffer, (no need free) */
  MFS_DirDataBlock_t* dir_buf = NULL;
  int flag_found_entry = 0;
  int block_par = 0;
  for(i=0; i< NUM_INODE_POINTERS; i++) {

    fp_block = nd_par.data[i];	/* gw: tbc */
    if(fp_block == -1) continue; /* gw: tbc, lazy scheme now assume no need to make new block for server_creat */
    block_par = i;
    lseek(fd, fp_block, SEEK_SET);
    read(fd, data_buf, MFS_BLOCK_SIZE);
	  
    dir_buf = (MFS_DirDataBlock_t*)data_buf;
    for(j=0; j<NUM_DIR_ENTRIES; j++) {
      MFS_DirEnt_t* p_de = &dir_buf->entries[j];
      if(p_de->inum == -1) {
	/* found an empty entry slot */
	p_de->inum = free_inum;
	strcpy(p_de->name, name);
	flag_found_entry = 1;
	break;
      }
    }

    if(flag_found_entry)
      break;
  }


  /* no empty entry found */
  if(!flag_found_entry) {
    perror("server_creat: failed to find entry_6");
    return -1;
  }

  /* datablock dir_buf is ready now*/

	  /* GW: how??? */
	  offset = p_cr->end_log;
	  step = MFS_BLOCK_SIZE; /* DirDataBlock should be rounded to 4096 */
	  p_cr->end_log += step;
	  lseek(fd, offset, SEEK_SET);
	  //	  write(fd, &db, step);	/* write data block sized 4096 */
	  //	  write(fd, &dir_buf, sizeof(MFS_DirDataBlock_t)); /* TODO verify this */
	  write(fd, dir_buf, sizeof(MFS_DirDataBlock_t)); /* TODO verify this */


	  /***** chk *****/
	  /* char data_buf_chk[MFS_BLOCK_SIZE]; */
	  /* int fp_block_chk = offset; */
	  /* lseek(fd, fp_block_chk, SEEK_SET); */
	  /* read(fd, data_buf_chk, MFS_BLOCK_SIZE); */
	  /***** chk *****/

	  MFS_Inode_t nd_par_new;
	  nd_par_new.size = nd_par.size; /* gw: tbc, assume no new block add to par dir */
	  nd_par_new.type = MFS_DIRECTORY;
	  for (i = 0; i < 14; i++) nd_par_new.data[i] = nd_par.data[i]; /* 14 pointers per inode */
	  nd_par_new.data[block_par] = offset; 	/* absolute offset */


	  offset = p_cr->end_log;
	  step = sizeof(MFS_Inode_t); /* inode size */
	  p_cr->end_log += step;
	  lseek(fd, offset, SEEK_SET);
	  write(fd, &nd_par_new, step);	


	  MFS_Imap_t mp_par_new;
	  for(i = 0; i< IMAP_PIECE_SIZE; i++) mp_par_new.inodes[i] = mp_par.inodes[i]; /* gw: dubious about num inodes per imap */
	  mp_par_new.inodes[l] = offset;

	  offset = p_cr->end_log;
	  step = sizeof(MFS_Imap_t); /* inode size */
	  p_cr->end_log += step;
	  lseek(fd, offset, SEEK_SET);
	  write(fd, &mp_par_new, step);	



	  /* now the first imap and inode is written, we update imap table */
	  /* this update is done after the (imap, inode, datablock creation is donea altogether ) -> atomic */
	  /* lseek(fd, 0 + 2*sizeof(int), SEEK_SET); /\* imap[0] in cr *\/ */
	  /* write(fd, &offset , sizeof(int));	  /\* write offset (the newest imap's absolute offset) into p_cr *\/ */
	  //	  p_cr->imap[0] = offset; /* offset of starting point of mp */
	  //	  p_cr->imap[0] = (int)offset;
	  p_cr->imap[k] = offset;
	  lseek(fd, 0, SEEK_SET);
	  write(fd, p_cr, sizeof(MFS_CR_t));


	  fsync(fd);




	  /********** now the parent file operation completes **********/


	  /* using child inum = free_inum */

  /********** file **********/
  /* make an empty block */
  /* generate clean buffer */
  char* ip = NULL;
  char wr_buffer[ MFS_BLOCK_SIZE ];
  /* init */
  for(i=0, ip=wr_buffer; i<MFS_BLOCK_SIZE; i++) {
    wr_buffer[i] = '\0';
  }





  /* pick an inode. use the first -1 nd */
  /* use free_inum found earlier */
  int inum = free_inum;
  is_old_mp = 0, is_old_nd = 0, is_old_block = 0;
  fp_mp = -1, fp_nd = -1, fp_block = -1;
  /* re-init if dir  */
  if(type == MFS_DIRECTORY) {
    MFS_DirDataBlock_t* p_dir = (MFS_DirDataBlock_t*) wr_buffer;
    for(i=0; i< NUM_DIR_ENTRIES; i++){
      strcpy(p_dir->entries[i].name, "\0");
      p_dir->entries[i].inum = -1;
    }
    strcpy(p_dir->entries[0].name, ".\0");
    p_dir->entries[0].inum = inum; /* gw: confirm this later */
    strcpy(p_dir->entries[1].name, "..\0");
    p_dir->entries[1].inum = pinum; /* gw: confirm this later */
  }



  /* set default offset if not old mp */
  offset = p_cr->end_log;




  /* fs operations starts */
  k = inum / IMAP_PIECE_SIZE; /* imap piece num */
  fp_mp =  p_cr->imap[k];
  if(fp_mp != -1){
    /* old mp exist */
    is_old_mp = 1;

    /* nav to that mp */
    l = inum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
    /* move def out for later mp_new creation */
    //    MFS_Imap_t mp;
    lseek(fd, fp_mp, SEEK_SET);
    read(fd, &mp, sizeof(MFS_Imap_t));
    fp_nd = mp.inodes[l]; /* gw: fp denotes file pointer (location within a file) */

    /* offset will remain default until confirm old block */
  }


  /* gw: it must be a new node */
  /* if(fp_nd != -1 && is_old_mp) { /\* also skip if not old mp  *\/ */
  /*   /\* old nd exist *\/ */
  /*   is_old_nd = 1; */
  /*   /\* nav to inode *\/ */
  /*   /\* moved def out for later nd_new creation *\/ */
  /*   //    mfs_Inode_t nd; */
  /*   lseek(fd, fp_nd, SEEK_SET); */
  /*   read(fd, &nd, sizeof(MFS_Inode_t)); */

  /*   /\* gw: temp disable *\/ */
  /*   /\* if(nd.type != MFS_REGULAR_FILE) { *\/ */
  /*   /\*   perror("server_write: not a regular file_4"); *\/ */
  /*   /\*   return -1; *\/ */
  /*   /\* } *\/ */
  /*   int fp_data = nd.data[0];  /\* get fp_data *\/ */
  /*   int sz_data = nd.size; */
  /*   int num_blocks = sz_data / MFS_BLOCK_SIZE + 1; */
  /*   fp_block = nd.data[block];  */

  /*   /\* offset will remain default until confirm old block *\/ */

  /* } */


  /* gw: must be a new block */
  /* /\* write the datablock pointed by this nd and block # *\/ */
  /* if(fp_block != -1 && is_old_nd && is_old_mp) { */
  /*   /\* existing block *\/ */
  /*   is_old_block = 1; */
  /*   /\* only in this case will offset not be p_cr->end_log *\/ */
  /*   offset = fp_block; */
  /* } */


  /* writing action */
  /* offset is defined either as default (p_cr->end_log) or a pre-existing block */
  step = MFS_BLOCK_SIZE; /* DirDataBlock should be rounded to 4096 */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, wr_buffer, MFS_BLOCK_SIZE); /* write whole block */


  /* new nd */
  MFS_Inode_t nd_new;
  nd_new.size = MFS_BLOCK_SIZE;			/* gw: changed to 4096 */
  nd_new.type = type;			  /* gw: tbc */
  for (i = 0; i < 14; i++) nd_new.data[i] = -1; /* copy data from old nd */
  nd_new.data[0] = offset;			/* assign to block 0 */


  offset = p_cr->end_log;	/* after the latestly created block */
  step = sizeof(MFS_Inode_t); /* inode size */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, &nd_new, step);


  /* make mp_new */
  /* update imap */
  MFS_Imap_t mp_new;
  if(is_old_mp) {
    for(i = 0; i< IMAP_PIECE_SIZE; i++) mp_new.inodes[i] = mp.inodes[i] ; /* copy old mp's data, mp is still in memory */
    mp_new.inodes[l] = offset; 	/* fp_nd_new */
  }
  else {
    for(i = 0; i< IMAP_PIECE_SIZE; i++) mp_new.inodes[i] = -1 ; /* copy old mp's data, mp is still in memory */
    mp_new.inodes[l] = offset; 	/* fp_nd_new */
  }

  offset = p_cr->end_log;
  step = sizeof(MFS_Imap_t); /* inode size */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, &mp_new, step);

  /* update cr */
  /* now the new imap and inode is written, we update imap table */
  p_cr->imap[k] = offset; 	/* gw: fp_mp_new */
  lseek(fd, 0, SEEK_SET);
  write(fd, p_cr, sizeof(MFS_CR_t));

  fsync(fd);
  return 0;


}


int server_unlink(int pinum, char* name){

  int i=0, j=0, k=0, l=0;
  int offset = 0, step =0;
  //  int is_old_mp = 0, is_old_nd = 0, is_old_block = 0;
  int is_nd_dir = 0, is_block_empty = 0, is_mp_new_empty = 0;

  int fp_mp_par = -1, fp_nd_par = -1, fp_block_par = -1;

  int fp_mp = -1, fp_nd = -1, fp_block = -1;

  MFS_Inode_t nd;
  MFS_Imap_t mp;

  /* chk pinum */
  if(pinum < 0 || pinum >= TOTAL_NUM_INODES) {
    perror("server_unlink: invalid pinum_1");
    return -1;
  }

  /* chk inum */
  /* if not exists, unlink is success */
  int inum = server_lookup(pinum, name);
  if(inum == -1) {
    return 0;
  }

  /********** PLAN: **********/
  /* chk type */
  /* if dir, chk empty */
  /* unlink blocks, inodes[i] = -1 */
  /* unlink nd, to -1 */
  /* unlink mp[i], to -1 */
  /* unlink imap, if all mp[i] = -1 */
  /* update p_cr */

  /* nav mp_par */
  /* nav nd_par */
  /* nav block_par */
  /* unlink DirEnt of inum, (\0, -1) */


  /********** Start  **********/
  /* chk empty if dir */
  k = inum / IMAP_PIECE_SIZE; /* imap piece num */
  if(p_cr->imap[k] == -1){
    perror("server_unlink: invalid inum_2");
    return -1;
  }
  fp_mp =  p_cr->imap[k];


  /* nav to mp */
  l = inum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
  //  MFS_Imap_t mp;
  lseek(fd, fp_mp, SEEK_SET);
  read(fd, &mp, sizeof(MFS_Imap_t));
  fp_nd = mp.inodes[l]; /* gw: fp denotes file pointer (location within a file) */
  if(fp_nd == -1) {
    perror("server_lookup: invalid pinum_3");
    return -1;
  }


  /* nav to inode */
  //  MFS_Inode_t nd;
  lseek(fd, fp_nd, SEEK_SET);
  read(fd, &nd, sizeof(MFS_Inode_t));


  /* chk empty if dir */
  if(nd.type == MFS_DIRECTORY) {
    is_nd_dir = 1;
    //  int fp_data = nd.data[0];  /* get fp_data */
    //  int sz_data = nd.size;
    //  int num_blocks = sz_data / MFS_BLOCK_SIZE + 1;


    /* read the datablock pointed by this nd */
    char data_buf[MFS_BLOCK_SIZE]; /* gw: use char buffer, (no need free) */

    for(i=0; i< NUM_INODE_POINTERS; i++) {
      fp_block = nd.data[i];	/* gw: tbc */
      if(fp_block == -1) continue;

      lseek(fd, fp_block, SEEK_SET);
      read(fd, data_buf, MFS_BLOCK_SIZE);
	  
      MFS_DirDataBlock_t* dir_buf = (MFS_DirDataBlock_t*)data_buf;
      for(j=0; j<NUM_DIR_ENTRIES; j++) {
	MFS_DirEnt_t* p_de = &dir_buf->entries[j];
	if(!(p_de->inum == pinum || p_de->inum == inum || p_de->inum == -1 )) {
	  perror("server_unlink: dir not empty_4");
	  return -1;
	}
      }
    }
  } /* end if dir */

  /* now we know it is a reg file or empty dir */
  /* => how to unlink? */
  /* just update mp_new, imap */
  /* no need nd_new, but may need mp_new */
  /* lastly update cr */


  MFS_Imap_t mp_new;
  for(i = 0; i< IMAP_PIECE_SIZE; i++) mp_new.inodes[i] = mp.inodes[i]; /* gw: dubious about num inodes per imap */
  mp_new.inodes[l] = -1;	/* actual unlink, clear the fp_nd to -1 */
  /* check if all inodes in mp_new are -1 */
  is_mp_new_empty = 1;
  for(i = 0; i< IMAP_PIECE_SIZE; i++) {
    if(mp_new.inodes[i] != -1){
      is_mp_new_empty = 0;
      break;
    }
  }

  /* handle cases of mp_new: empty or non-empty */
  if(is_mp_new_empty) {
    /* mp_new is empty, we just delete corresponding entry in p_cr */
    /* update cr */
    p_cr->imap[k] = -1;		/* actual unlinking */
    lseek(fd, 0, SEEK_SET);
    write(fd, p_cr, sizeof(MFS_CR_t));

    fsync(fd);
  }
  else {
    /* mp_new still has entry , we shall update it */
    offset = p_cr->end_log;
    step = sizeof(MFS_Imap_t); /* inode size */
    p_cr->end_log += step;
    lseek(fd, offset, SEEK_SET);
    write(fd, &mp_new, step);	

    /* update cr */
    p_cr->imap[k] = offset;
    lseek(fd, 0, SEEK_SET);
    write(fd, p_cr, sizeof(MFS_CR_t));

    fsync(fd);
  }


  /* now we handle the parent, just remove the inode, name pair from DirEnt */
  /* here we know it does  exist && empty (if dir) */
  /********** pinum **********/

  k = pinum / IMAP_PIECE_SIZE; /* imap piece num */
  fp_mp_par =  p_cr->imap[k];
  if(fp_mp_par == -1){
    perror("server_unlink: invalid pinum_5");
    return -1;
  }

  /* nav to mp */
  l = pinum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
  MFS_Imap_t mp_par;	       /* imap piece of parent */
  lseek(fd, fp_mp_par, SEEK_SET);
  read(fd, &mp_par, sizeof(MFS_Imap_t));
  fp_nd_par = mp_par.inodes[l]; /* gw: fp denotes file pointer (location within a file) */
  if(fp_nd_par == -1) {
    perror("server_unlink: invalid pinum_6");
    return -1;
  }


  /* nav to inode */
  MFS_Inode_t nd_par;
  lseek(fd, fp_nd_par, SEEK_SET);
  read(fd, &nd_par, sizeof(MFS_Inode_t));
  /* assert dir */
  if(nd_par.type != MFS_DIRECTORY) {
    perror("server_unlink: invalid pinum_7");
    return -1;
  }


  /* read the datablock pointed by this nd */
  //  void* data_buf = malloc(MFS_BLOCK_SIZE);
  char data_buf[MFS_BLOCK_SIZE]; /* gw: use char buffer, (no need free) */
  MFS_DirDataBlock_t* dir_buf = NULL;
  int flag_found_entry = 0;
  int block_par = 0;
  for(i=0; i< NUM_INODE_POINTERS; i++) {

    fp_block_par = nd_par.data[i];	/* gw: tbc */
    if(fp_block_par == -1) continue; /* gw: tbc, lazy scheme now assume no need to make new block for server_creat */
    block_par = i;
    lseek(fd, fp_block_par, SEEK_SET);
    read(fd, data_buf, MFS_BLOCK_SIZE);
	  
    dir_buf = (MFS_DirDataBlock_t*)data_buf;
    for(j=0; j<NUM_DIR_ENTRIES; j++) {
      MFS_DirEnt_t* p_de = &dir_buf->entries[j];
      if(p_de->inum == inum) {
	/* found that entry */
	p_de->inum = -1;
      	strcpy(p_de->name, "\0"); /* gw: tbc, dubious */
      	flag_found_entry = 1;
      	break;
      }

      /* remove it from dir entry */
    }

    if(flag_found_entry)
      break;
  }


  /* no empty entry found */
  if(!flag_found_entry) {
    //    perror("server_creat: failed to find entry_6");
    //    return -1;
    return 0;			/* gw: not found means success */
  }

  /* datablock dir_buf is ready now*/

  /* GW: how??? */
  offset = p_cr->end_log;
  step = MFS_BLOCK_SIZE; /* DirDataBlock should be rounded to 4096 */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, dir_buf, sizeof(MFS_DirDataBlock_t)); /* TODO verify this */



  MFS_Inode_t nd_par_new;
  nd_par_new.size = nd_par.size - MFS_BLOCK_SIZE > 0? nd_par.size - MFS_BLOCK_SIZE : 0 ; /* reduce one block of size */

  nd_par_new.type = MFS_DIRECTORY;
  for (i = 0; i < 14; i++) nd_par_new.data[i] = nd_par.data[i]; /* 14 pointers per inode */
  nd_par_new.data[block_par] = offset; 	/* absolute offset */


  offset = p_cr->end_log;
  step = sizeof(MFS_Inode_t); /* inode size */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, &nd_par_new, step);	


  MFS_Imap_t mp_par_new;
  for(i = 0; i< IMAP_PIECE_SIZE; i++) mp_par_new.inodes[i] = mp_par.inodes[i]; /* gw: dubious about num inodes per imap */
  mp_par_new.inodes[l] = offset;

  offset = p_cr->end_log;
  step = sizeof(MFS_Imap_t); /* inode size */
  p_cr->end_log += step;
  lseek(fd, offset, SEEK_SET);
  write(fd, &mp_par_new, step);	



  /* now the first imap and inode is written, we update imap table */
  /* this update is done after the (imap, inode, datablock creation is donea altogether ) -> atomic */
  /* lseek(fd, 0 + 2*sizeof(int), SEEK_SET); /\* imap[0] in cr *\/ */
  /* write(fd, &offset , sizeof(int));	  /\* write offset (the newest imap's absolute offset) into p_cr *\/ */
  //	  p_cr->imap[0] = offset; /* offset of starting point of mp */
  //	  p_cr->imap[0] = (int)offset;
  p_cr->imap[k] = offset;
  lseek(fd, 0, SEEK_SET);
  write(fd, p_cr, sizeof(MFS_CR_t));


  fsync(fd);

  /********** now the parent file operation completes **********/
  return 0;


}


int server_shutdown() {
  fsync(fd);
  exit(0);
}


int print_block(int pinum, int block) {
  int i=0, j=0, k=0, l=0;

  if(pinum < 0 || pinum >= TOTAL_NUM_INODES)
    {
      perror("server_print: invalid pinum_1");
      return -1;
    }


  if( block < 0 || block > NUM_INODE_POINTERS) {
    perror("server_print: invalid block_5");
    return -1;
  }


  k = pinum / IMAP_PIECE_SIZE; /* imap piece num */
  if(p_cr->imap[k] == -1){
    perror("server_print: invalid pinum_2");
    return -1;
  }
  int fp_mp =  p_cr->imap[k];


  /* nav to mp */
  l = pinum % IMAP_PIECE_SIZE; /* inode index within a imap piece */
  MFS_Imap_t mp;
  lseek(fd, fp_mp, SEEK_SET);
  read(fd, &mp, sizeof(MFS_Imap_t));
  int fp_nd = mp.inodes[l]; /* gw: fp denotes file pointer (location within a file) */
  if(fp_nd == -1) {
    perror("server_print: invalid pinum_3");
    return -1;
  }


  /* nav to inode */
  MFS_Inode_t nd;
  lseek(fd, fp_nd, SEEK_SET);
  read(fd, &nd, sizeof(MFS_Inode_t));
  /* assert dir */
  /* if(nd.type != MFS_DIRECTORY) { */
  /*   perror("server_lookup: invalid pinum_4"); */
  /*   return -1; */
  /* } */
  int fp_data = nd.data[0];  /* get fp_data */
  int sz_data = nd.size;
  int num_blocks = sz_data / MFS_BLOCK_SIZE + 1;

  char data_buf[MFS_BLOCK_SIZE]; /* gw: use char buffer, (no need free) */
  int fp_block = nd.data[block];	/* gw: tbc */

  lseek(fd, fp_block, SEEK_SET);
  read(fd, data_buf, MFS_BLOCK_SIZE);

  if(nd.type == MFS_DIRECTORY) {
    /* read the datablock pointed by this nd */
    //  void* data_buf = malloc(MFS_BLOCK_SIZE);
    printf("\n server_print: This is dir. ");	  
    printf("\n%s \t %s", "name", "inum" );
    MFS_DirDataBlock_t* dir_buf = (MFS_DirDataBlock_t*)data_buf;
    for(j=0; j<NUM_DIR_ENTRIES; j++) {
      if(dir_buf->entries[j].inum == -1) continue;
      MFS_DirEnt_t* p_de = &dir_buf->entries[j];
      printf("\n%s \t %d", &p_de->name[0], p_de->inum );
    }
  }
  else if(nd.type == MFS_REGULAR_FILE) {

    printf("\n server_print: This is regular file. ");


  }
  else {
    perror("\n server_print: invalid file type. ");
    return -1;
  }

  return 0;

}
/********************Net lib  ********************/




enum message {
	PAK_LOOKUP,
	PAK_STAT,
	PAK_WRITE,
	PAK_READ,
	PAK_CREAT,
	PAK_UNLINK,
	PAK_RESPONSE,
	PAK_SHUTDOWN
};

typedef struct __Net_Packet {
	enum message message;

	int inum;
	int block;
	int type;

	char name[LEN_NAME];
	char buffer[MFS_BLOCK_SIZE];
	MFS_Stat_t stat;
} Net_Packet;


void serverListen(int port)
{
  int sd = UDP_Open(port);
  if(sd < 0)
    {
      printf("Error opening socket on port %d\n", port);
      exit(1);
    }

  printf("Starting server...\n");
  while (1) {
    struct sockaddr_in s;
    Net_Packet packet;
    int rc = UDP_Read(sd, &s, (char *)&packet, sizeof(Net_Packet));
    if (rc > 0) {
      Net_Packet responsePacket;

      switch(packet.message){
		    		
      case PAK_LOOKUP :
	responsePacket.inum = server_lookup(packet.inum, packet.name);
	break;

      case PAK_STAT :
	responsePacket.inum = server_stat(packet.inum, &(responsePacket.stat));
	break;

      case PAK_WRITE :
	responsePacket.inum = server_write(packet.inum, packet.buffer, packet.block);
	break;

      case PAK_READ:
	responsePacket.inum = server_read(packet.inum, responsePacket.buffer, packet.block);
	break;

      case PAK_CREAT:
	responsePacket.inum = server_creat(packet.inum, packet.type, packet.name);
	break;

      case PAK_UNLINK:
	responsePacket.inum = server_unlink(packet.inum, packet.name);
	break;

      case PAK_SHUTDOWN:
	break;
		    	
      case PAK_RESPONSE:
	break;
      }

      responsePacket.message = PAK_RESPONSE;
      rc = UDP_Write(sd, &s, (char*)&responsePacket, sizeof(Net_Packet));
      if(packet.message == PAK_SHUTDOWN)
	server_shutdown();
    }
  }
}


/******************** MFS start ********************/

char* serverHostname;
int serverPort;
int initialized = 0;

int checkName(char* );

int MFS_Init(char *hostname, int port) {
	if(port < 0 || strlen(hostname) < 1)
		return -1;
	serverHostname = malloc(strlen(hostname) + 1);
	strcpy(serverHostname, hostname);
	serverPort = port;
	initialized = 1;
	return 0;
}

int MFS_Lookup(int pinum, char *name){
	if(!initialized)
		return -1;
	
	if(checkName(name) < 0)
		return -1;

	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = pinum;
	sentPacket.message = PAK_LOOKUP;
	printf("\nMFS_Lookup: name is %s ",name);
	strcpy((char*)&(sentPacket.name), name);
	printf("\nMFS_Lookup: Packet contained name is %s ",sentPacket.name);
	int rc = sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3);
	if(rc < 0)
		return -1;
	
	rc = responsePacket.inum;
	return rc;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	if(!initialized)
		return -1;

	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = inum;
	sentPacket.message = PAK_STAT;

	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	memcpy(m, &(responsePacket.stat), sizeof(MFS_Stat_t));
	return 0;
}

int MFS_Write(int inum, char *buffer, int block){
	if(!initialized)
		return -1;
	
	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = inum;
	//strncpy(sentPacket.buffer, buffer, BUFFER_SIZE);
	memcpy(sentPacket.buffer, buffer, BUFFER_SIZE);
	sentPacket.block = block;
	sentPacket.message = PAK_WRITE;
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;
	
	return responsePacket.inum;
}

int MFS_Read(int inum, char *buffer, int block){
	if(!initialized)
		return -1;
	
	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = inum;
	sentPacket.block = block;
	sentPacket.message = PAK_READ;
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	if(responsePacket.inum > -1)
		memcpy(buffer, responsePacket.buffer, BUFFER_SIZE);
	
	return responsePacket.inum;
}

int MFS_Creat(int pinum, int type, char *name){
	if(!initialized)
		return -1;
	
	if(checkName(name) < 0)
		return -1;

	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = pinum;
	sentPacket.type = type;
	sentPacket.message = PAK_CREAT;

	/* gw: */
	printf("\n MFS_Creat: name is %s", name);
	strcpy(sentPacket.name, name);
	printf("\n MFS_Creat: Packet name is %s", sentPacket.name);
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inum;
}

int MFS_Unlink(int pinum, char *name){
	if(!initialized)
		return -1;
	
	if(checkName(name) < 0)
		return -1;
	
	Net_Packet sentPacket;
	Net_Packet responsePacket;

	sentPacket.inum = pinum;
	sentPacket.message = PAK_UNLINK;
	strcpy(sentPacket.name, name);
	
	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;

	return responsePacket.inum;
}

int MFS_Shutdown(){
	Net_Packet sentPacket, responsePacket;
	sentPacket.message = PAK_SHUTDOWN;


	if(sendPacket(serverHostname, serverPort, &sentPacket, &responsePacket, 3) < 0)
		return -1;
	
	return 0;
}

int checkName(char* name) {
	if(strlen(name) > 27)
		return -1;
	return 0;
}
