#ifndef __MFSGW_h__
#define __MFSGW_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)
#define MFS_BYTE_STEP_SIZE   (16384)
#define NUM_INODE_POINTERS   (14)
#define TOTAL_NUM_INODES (4096)
#define IMAP_PIECE_SIZE   (16)		
#define NUM_IMAP TOTAL_NUM_INODES / IMAP_PIECE_SIZE /* gw: 256 */
#define LEN_NAME 60

/* gw: TODO to change */
#define NUM_DIR_ENTRIES 14

enum MFS_CMD {
  MFS_CMD_INIT,
  MFS_CMD_SHUTDOWN,
  MFS_CMD_LOOKUP,
  MFS_CMD_STAT ,
  MFS_CMD_WRITE,
  MFS_CMD_READ ,
  MFS_CMD_CREAT,
 MFS_CMD_UNLINK
};

/* ******************** */
#define SZ_BLOCK 4096
#define SZ_CHUNK 8 * 4096	/* gw: may be 4 is enough */
//#define NUM_INODE_POINTERS 14		/* number of direct pointers in inode */
//#define TOTAL_NUM_INODES 4096
//#define INODES_PER_BLOCK 16		/* num of entries of each imap piece */
//#define NUM_IMAP TOTAL_NUM_INODES / INODES_PER_BLOCK
//#define LEN_NAME 60

/* make LOCAL */
//#define INODES_PER_BLOCK	14 /* gw: should be changable */

/* typedef struct __MFS_Imap_t{ */
/*   /\* note:sizeof(int)==4 *\/ */
/*   int inum[INODES_PER_BLOCK]; */
/* }MFS_Imap_t; */

/* typedef struct __MFS_CR_t { */
/*   void* end; */
/*   MFS_Imap_t* imap[NUM_IMAP]; 	/\* an array of ptrs to imap pieces *\/ */
/* }MFS_CR_t; */

/* typedef struct __MFS_Inode_t{ */
/*   int size;			/\* number of last byte in file *\/ */
/*   enum FILE_TYPE type; */
/*   void* data[NUM_INODE_POINTERS];	/\* each a direct pointer to data block *\/ */
/* }MFS_Inode_t; */

/* typedef struct __MFS_Dirent_t{ */
/*   char name[LEN_NAME]; */
/*   int inum; */
/* }MFS_Dirent_t; */
/* ******************** */

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[LEN_NAME];  
	int  inum;      	/* inume is the index of each inode, not address */
} MFS_DirEnt_t;

typedef struct __MFS_Protocol_t {
  //char cmd;   // Command type
  enum MFS_CMD cmd;
	int  ipnum; // inode | parent inode
	int  block; // block number
	int  ret;
	char datachunk[4096];
} MFS_Protocol_t;

typedef struct __MFS_Inode_t{
	int size;
	int type;
	int data[NUM_INODE_POINTERS]; /* pointer to data blocks (direct) */
} MFS_Inode_t;

/* gw: strictly, this is just one piece of imap, total 256 pieces */
typedef struct __MFS_Imap_t{
	int inodes[IMAP_PIECE_SIZE]; /* gw: 16 */
} MFS_Imap_t;

typedef struct __MFS_CR_t{
	int inode_count;
	int end_log;
	int imap[NUM_IMAP];	/* 256, NUM_IMAP = TOTAL_NUM_INODES / IMAP_PIECE_SIZE */
} MFS_CR_t;

/* gw: data block for dir */
typedef struct __MFS_DirDataBlock_t {
  MFS_DirEnt_t entries[NUM_DIR_ENTRIES];
} MFS_DirDataBlock_t;



int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);

#endif // __MFS_GW_h__
