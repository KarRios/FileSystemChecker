#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define SB_NINODES 200
#define SB_SIZE 1024

#include "types.h"
#include "fs.h"
#include "stat.h"


const char* IMAGE_NOT_FOUND = "image not found.";
const char* ERR_INODE       = "ERROR: bad inode.";
const char* ERR_ADDR_DIR    = "ERROR: bad direct address in inode.";
const char* ERR_ADDR_INDIR  = "ERROR: bad indirect address in inode,";
const char* ERR_DIR_ROOT    = "ERROR: root directory does not exist.";
const char* ERR_DIR_FMT     = "ERROR: directory not properly formatted.";
const char* ERR_DBMP_FREE   = "ERROR: address used by inode but marked free in bitmap.";
const char* ERR_DBMP_USED   = "ERROR: bitmap marks block in use but it is not in use.";
const char* ERR_ADDR_D_DUP  = "ERROR: direct address used more than once.";
const char* ERR_ADDR_IN_DUP = "ERROR: indirect address used more than once.";
const char* ERR_ITBL_USED   = "ERROR: inode marked use but not found in a directory.";
const char* ERR_ITBL_FREE   = "ERROR: inode referred to in directory but marked free.";
const char* ERR_FILE_REF    = "ERROR: bad reference count for file.";
const char* ERR_DIR_REF     = "ERROR: directory appears more tham once in file system.";
const chat* ERR_DIR_PARENT  = "ERROR: parent directory mismatch.";

void *image;
superblock *sb;
char *bitmap_buf;
int data;
int root = 0;

int inode_dir[SB_NINODES];
int ref_parent[SB_NINODES];
int ref_dir[SB_NINODES];
int num_ref[SB_NINODES];
int real_num[SB_NINODES];
int bitmap_data[SB_SIZE];

int inode_process(struct dinode *dip){
  if(dip->type < 0 || dip->type > 3){
    fprintf(stderr,ERR_INODE);
    fprintf(stderr,"\n");
    exit(1);
  }

  if(dip->type == 0){return 1;}

  int i,j,current_entry,block,value;
  dirent *direct_entry;
  uint *indirect_block;
  int dir_num = BSIZE / sizeof(struct dirent);
  int indir_num = BSIZE / sizeof(uint);

  for(i = 0; i <= NDIRECT; i++){
    if(dip->addrs[i] != 0 && (dip->addrs[i] < data || dip->addrs[i] > 1023)){
      fprintf(stderr,ERR_ADDR_DIR);
      fprintf(stderr,"\n");
      exit(1);
    }

    block = dip->addrs[i];
    value = bitmap_data[block];

    if(block > 0) {
      bitmap_data[block]++;

      if(bitmap_data[block] > 2){
	fprintf(stderr,ERR_ADDR_D_DUP);
	fprintf(stderr,"\n");
	exit(1);
      }

      if(value == 0){
	fprintf(stderr,ERR_DBMP_FREE);
	fprintf(stderr,"\n");
	exit(1);
      }
    }

    if(dip->type == 1){
      direct_entry = (struct dirent*)(image + (block * BSIZE));

      j = 0;

      if(strcmp(direct_entry->name,".") || strcmp((direct_entry +1)->name, ".")) {
	fprintf(stderr,ERR_DIR_FMT);
	fprintf(stderr,"\n");
	exit(1);
      }

      inode_dir[direct_entry->inum] = 1;
      
      current_entry = direct_entry->inum;
      ref_parent[current_entry] = (direct_entry + 1)->inum;

      if(direct_entry->inum == 1 && (direct_entry + 1)->inum == 1 && root == 0){
	root = 1;
      }

      j += 2;
      direct_entry += 2;
    }

    if(i < NDIRECT){
      for(;j < dir_num; j++){
	if(direct_entry->inum != 0){
	  num_ref[direct_entry->inum]++;
	  real_num[direct_entry->inum] = current;
	}

	direct_entry++;
      }
    }
  }

  block = dip-addrs[NDIRECT];
  indirect_block = (uint*)(image + (block *BSIZE));

  for(i = 0; i < indir_num; i++){
    if(*indirect_block != 0 && (*indirect_block < data || *indirect_block > 1023)){
      fprintf(stderr,ERR_ADDR_INDIR);
      fprintf(stderr,"\n");
      exit(1);
    } 

    if(*indirect_block > 0 && bitmap_data[*indirect_block] == 0){
      fprintf(stderr,ERR_DBMP_FREE);
      fprintf(stderr,"\n");
      exit(1);
    }

    if(*indirect_block > 0){
      bitmap_data[*indirect_block]++;
    }

    if(bitmap_data[*indirect_block] > 2){
      fprintf(stderr,ERR_ADDR_IN_DUP);
      fprintf(stderr,"\n");
      exit(1);
    }

    if(dip->type == 1){
      direct_entry = (struct dirent*)(image + (*indirect_block *BSIZE));
      for(j = 0; j < dir_num; j++){
	if(direct_entry->inum != 0){
	  num_ref[direct_entry->inum]++;
	  ref_dir[direct_entry->inum] = current_entry;
	}

	direct_entry++;
      }
    }

    indirect_block++;
  }

  return 0;
}

int setup(){
  int i;
  for(i = data; i < sb->size; i++){
    if((bitmap_buf[i/8] & (0x1 << (i%8))) > 0){
      bitmap_data[i] = 1;
    }else{
      bitmap_data[i] = 0;
    }
  }
  return 0;
}

int check_map(){
  int i;

  for(i = data; i < (data + sb->nblocks + 1); i++){
    if(bitmap_data[i] == 1){
      fprintf(stderr,ERR_DBMP_USED);
      fprintf(stderr,"\n");
      exit(1);
    }
  }

  return 0;
}

int post_check(){
  check_map();

  int i;

  for(i = 0; i < sb->ninodes; i++){
    if(inode_dir[i] == 1){
      if(num_ref[i] > 1 || real_num[i] > 1){
	fprintf(stderr,ERR_DIR_REF);
	fprintf(stderr,"\n");
	exit(1);
      }

      if(ref_parent[i] !=  ref_dir[i]){
	fprintf(stderr,ERR_DIR_PARENT);
	fprintf(stderr,"\n");
	exit(1);
      }
    }

    if(num_ref[i] == 0 && real_num[i] > 0){
      fprintf(stderr,ERR_ITBL_USED);
      fprintf(stderr,"\n");
      exit(1);
    }else if(num_ref[i] > 0 && real_num[i] == 0){
      fprintf(stderr,ERR_ITBL_FREE);
      fprintf(stderr,"\n");
      exit(1);
    }

    if(inode_dir[i] == 0 && num_ref[i] != real_num[i]){
      fprintf(stderr,ERR_FILE_REF);
      fprintf(stderr,"\n");
      exit(1);
    }
  }

  return 0;
}

int table(){
  int i,j;

  int num = 0;
  int inode_num = sb->ninodes / IPB;

  setup();

  ref_dir[ROOTINO] = 1;
  num_ref[ROOTINO] = 1;

  for(i = 2; i < inode_num + 2; i++){
    struct dinode *dip = (struct dinode*)(image + (i * BSIZE));

    for(j = 0; j < IPB; j++){
      inode_process(dip);

      if(dip->type != 0){
	if(dip->nlink > 1){
	  real_num[inode_num] = dip->nlink;
	}else{
	  real_num[inode_num] = 1;
	}
      }

      if(inode_num == 1 && root == 0){
	fprintf(stderr,ERR_DIR_ROOT);
	fprintf(stderr,"\n");
	exit(1);
      }

      inode_num++;
      dip++;
    }
  }

  return 0;
}

int buf_setup(char *filename){
  int fd = open(filename,O_RDONLY);

  if(fd < 0){
    fprintf(stderr,IMAGE_NOT_FOUND);
    fprintf(stderr,"\n");
    exit(1);
  }

  int rc;
  struct stat sbuf;
  rc = fstat(fd,&sbuf);
  assert(rc == 0);

  image = mmap(NULL,sbuf.st_size,PROT_READ,MAP_PRIVATE,fd,0);
  assert(image != MAP_FAILED);

  sb = (struct superblock*)(image +BSIZE);

  uint bitmap_block = sb->ninodes / IPB +3;
  bitmap_buf = (char *)(image + (bitmap_block * BSIZE));
  
  uint bit_num = sb->size / (BSIZE * 8) + 1;
  data = bitmap_block + bit_num;

  return 0;
}

int main(int argc, char *argv[]){
  if(argc != 2){
    fprintf(stderr, "Usage: xcheck file_syste_image\n");
    exit(1);
  }

  buf_setup(argv[1]);
  table();
  post_check();

  return 0;
}
  
	
