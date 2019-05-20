#ifndef FS_H
#define FS_H

#define ROOTINO 1  
#define BSIZE 512

struct superblock {
  uint size;         
  uint nblocks;      
  uint ninodes;
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           
  short major;         
  short minor;          
  short nlink;          
  uint size;            
  uint addrs[NDIRECT+1];   
};

#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + 2)

#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + (ninodes)/IPB + 3)

#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#endif
