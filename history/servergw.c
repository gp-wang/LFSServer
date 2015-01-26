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

//int server_init(char* , int );
int server_lookup(int, char* );
int server_stat(int , MFS_Stat_t* );
int server_write(int , char* , int );
int server_read(int , char* , int );
int server_creat(int , int , char* );
int server_unlink(int , char* );
int server_shutdown();
int server_init(int , char* );
int print_block(int, int);




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

/* gw: changed order will fail */
/* typedef struct __Net_Packet { */
/* 	char name[LEN_NAME]; */
/* 	MFS_Stat_t stat; */
/* 	char buffer[MFS_BLOCK_SIZE]; */

/* 	enum MFS_REQ request; */

/* 	int inum; */
/* 	int type; */
/* 	int block; */

/* } Net_Packet; */

typedef struct __UDP_Packet {
	enum MFS_REQ request;

	int inum;
	int block;
	int type;

	char name[LEN_NAME];
	char buffer[MFS_BLOCK_SIZE];
	MFS_Stat_t stat;
} UDP_Packet;


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





int server_init(int port, char* image_path) {


  //  creat_img(port,path);
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
    strcpy(db.entries[1].name, "..\0");
    db.entries[1].inum = 0;

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
    p_cr->imap[0] = offset; /* gw: right, contains the address of imap piece "0" */
    lseek(fd, 0, SEEK_SET);
    write(fd, p_cr, sizeof(MFS_CR_t));

    fsync(fd);


    printf("\n server_init: new file created\n");

  }
  else {

    printf("Using old file of size %d\n", (int)fs.st_size);

    /* gw: read in cr region, including imap */
    lseek(fd,0, SEEK_SET);
    read(fd, p_cr, sizeof(MFS_CR_t));


  }

  //	TODO: remove comment here
  int sd=-1;
  if((sd =   UDP_Open(port))< 0){
    perror("server_init: port open fail");
    return -1;
  }


  struct sockaddr_in s;
  UDP_Packet buf_pk,  rx_pk;

  while (1) {
    if( UDP_Read(sd, &s, (char *)&buf_pk, sizeof(UDP_Packet)) < 1)
      continue;


    if(buf_pk.request == REQ_LOOKUP){
      rx_pk.inum = server_lookup(buf_pk.inum, buf_pk.name);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_STAT){
      rx_pk.inum = server_stat(buf_pk.inum, &(rx_pk.stat));
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_WRITE){
      rx_pk.inum = server_write(buf_pk.inum, buf_pk.buffer, buf_pk.block);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_READ){
      rx_pk.inum = server_read(buf_pk.inum, rx_pk.buffer, buf_pk.block);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_CREAT){
      rx_pk.inum = server_creat(buf_pk.inum, buf_pk.type, buf_pk.name);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_UNLINK){
      rx_pk.inum = server_unlink(buf_pk.inum, buf_pk.name);
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));

    }
    else if(buf_pk.request == REQ_SHUTDOWN) {
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));
      server_shutdown();
    }
    else if(buf_pk.request == REQ_RESPONSE) {
      rx_pk.request = REQ_RESPONSE;
      UDP_Write(sd, &s, (char*)&rx_pk, sizeof(UDP_Packet));
    }
    else {
      perror("server_init: unknown request");
      return -1;
    }


  }

  return 0;
}


int main(int argc, char *argv[])
{
	if(argc != 3) {
		printf("Usage: server [portnum] [file-system image]\n");
		exit(1);
	}

	int portNumber = atoi(argv[1]);
	char *fileSysPath = argv[2];

	server_init(portNumber, fileSysPath);

	return 0;
}

