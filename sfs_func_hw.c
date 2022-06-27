//
// Simple FIle System
// Student Name : Yang Jeong Hoon
// Student Number : B711109
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

struct sfs_bitmap {
  u_int8_t sb_block;
};

static struct sfs_super spb;	// superblock
static struct sfs_bitmap sbm[SFS_BLOCKSIZE];
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read(&spb, SFS_SB_LOCATION );
  disk_read(sbm, SFS_MAP_LOCATION);
/*
  int i;
  for(i=0;i<SFS_BLOCKSIZE;i++){
    printf("%d : %d\n", i, sbm[i].sb_block);
  }
*/
  printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

void sfs_touch(const char* path)
{
  if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) {
    error_message("touch", path, -8);
    return;
  }
	//skeleton implementation
  int i, j;

  struct sfs_inode inode_tc;
  struct sfs_inode inode;
  struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

  disk_read(&inode_tc, sd_cwd.sfd_ino);

	for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_tc.sfi_direct[i] == 0) {
      break;
    }
    disk_read(dir_entry, inode_tc.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      if (strcmp(path, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) {
        error_message("touch", path, -6);
        return;
      }
    }
  }

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );

	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );

  if (si.sfi_size == SFS_BLOCKSIZE * SFS_NDIRECT) {
    error_message("touch", path, -3);
    return;
  }

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6 
	// block 0: superblock,	block 1:root, 	block 2:bitmap 
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined
	
	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	//block access
  int index = si.sfi_size / SFS_BLOCKSIZE;
  //printf("%d", index);
  disk_read(sd, si.sfi_direct[index]);

	//allocate new block
  int newbie_ino;

  for (i = 0; i < SFS_BLOCKSIZE; i++) {
    if (sbm[i].sb_block == 255) continue;
    
    for (j = 0; j < 8; j++) {
      //printf("%d : %d\n", i * 8 + j, BIT_CHECK(sbm[i].sb_block, j));
      if (!BIT_CHECK(sbm[i].sb_block, j)) {
        newbie_ino = i * 8 + j;
        //printf("newbie_ino : %d\n", newbie_ino);
        break;
      }
    }
    if (!BIT_CHECK(sbm[i].sb_block, j)) break;
  }
  BIT_SET(sbm[i].sb_block, j);

  int sd_index;

  for(i = 0; i < SFS_DENTRYPERBLOCK; i++) {
    if (sd[i].sfd_ino == 0) {
      sd_index = i;
      //printf("sd_index : %d\n", sd_index);
      break;
    }
  }

	sd[i].sfd_ino = newbie_ino;
	strncpy( sd[i].sfd_name, path, SFS_NAMELEN );

	disk_write( sd, si.sfi_direct[index] );

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );

	struct sfs_inode newbie;

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;

	disk_write( &newbie, newbie_ino );
  disk_write(sbm, SFS_MAP_LOCATION);
}

void sfs_cd(const char* path)
{
  int i, j;
  struct sfs_inode inode_cd;
  struct sfs_inode inode;
  struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
  
  disk_read(&inode_cd, sd_cwd.sfd_ino);

  if (path == NULL) {
    sd_cwd.sfd_ino = 1;
    sd_cwd.sfd_name[0] = '/';
    sd_cwd.sfd_name[1] = '\0';
    return;
  }

  if (strcmp(path, ".") == 0) return;
  
  if (strcmp(path, "..") == 0) {
    disk_read(dir_entry, inode_cd.sfi_direct[0]);
    
    sd_cwd.sfd_ino = dir_entry[1].sfd_ino;
    strncpy(sd_cwd.sfd_name, dir_entry[1].sfd_name, SFS_NAMELEN);

    return;
  }
  
  for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_cd.sfi_direct[i] == 0) {
      error_message("cd", path, -1);
      return;
    }
    disk_read(dir_entry, inode_cd.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      if (strcmp(path, dir_entry[j].sfd_name) == 0) {
        disk_read(&inode, dir_entry[j].sfd_ino);
        break;
      }
    }
    if (strcmp(path, dir_entry[j].sfd_name) == 0) break;
  }

  if (i == SFS_NDIRECT) {
    error_message("cd", path, -1);
    return;
  }

  if (inode.sfi_type == SFS_TYPE_FILE) {
    error_message("cd", path, -2);
    return;
  }

  sd_cwd.sfd_ino = dir_entry[j].sfd_ino;
  strncpy(sd_cwd.sfd_name, dir_entry[j].sfd_name, SFS_NAMELEN);
  return;
}

void sfs_ls(const char* path)
{
  int i, j;
  struct sfs_inode inode_ls;
  struct sfs_inode inode;
  struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

  disk_read(&inode_ls, sd_cwd.sfd_ino);

  if (path == NULL || strcmp(path, ".") == 0) {
    for (i = 0; i < SFS_NDIRECT; i++) {
      if (inode_ls.sfi_direct[i] == 0) break;
      disk_read(dir_entry, inode_ls.sfi_direct[i]);
      for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
        if (dir_entry[j].sfd_ino == 0) continue;
        printf("%s", dir_entry[j].sfd_name);
        disk_read(&inode, dir_entry[j].sfd_ino);
        if (inode.sfi_type == SFS_TYPE_DIR) {
          printf("/");
        }
        printf("\t");
      }
    }
    printf("\n");
    return;
  }

  if (strcmp(path, "..") == 0) {
    disk_read(dir_entry, inode_ls.sfi_direct[0]);
    disk_read(&inode_ls, dir_entry[1].sfd_ino);
    
    for (i = 0; i < SFS_NDIRECT; i++) {
      if (inode_ls.sfi_direct[i] == 0) break;
      disk_read(dir_entry, inode_ls.sfi_direct[i]);
      for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
        if (dir_entry[j].sfd_ino == 0) continue;
        printf("%s", dir_entry[j].sfd_name);
        disk_read(&inode, dir_entry[j].sfd_ino);
        if (inode.sfi_type == SFS_TYPE_DIR) {
          printf("/");
        }
        printf("\t");
      }
    }
    printf("\n");
    return;
  }

  for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_ls.sfi_direct[i] == 0) break;
    disk_read(dir_entry, inode_ls.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      if (strcmp(path, dir_entry[j].sfd_name) == 0) {
        disk_read(&inode_ls, dir_entry[j].sfd_ino);
        break;
      }
    }
    if (strcmp(path, dir_entry[j].sfd_name) == 0) break;
  }

  if (inode_ls.sfi_direct[i] == 0 || i == SFS_NDIRECT) {
    error_message("ls", path, -1);
    return;
  }

  if (inode_ls.sfi_type == SFS_TYPE_FILE) {
    printf("%s\n", dir_entry[j].sfd_name);
    return;
  }

  for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_ls.sfi_direct[i] == 0) continue;
    disk_read(dir_entry, inode_ls.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      if (dir_entry[j].sfd_ino == 0) continue;
      printf("%s", dir_entry[j].sfd_name);
      disk_read(&inode, dir_entry[j].sfd_ino);
      if (inode.sfi_type == SFS_TYPE_DIR) {
        printf("/");
      }
      printf("\t");
    }
  }
  printf("\n");
  return;
}

void sfs_mkdir(const char* org_path) 
{
	if (strcmp(org_path, ".") == 0 || strcmp(org_path, "..") == 0) {
    error_message("touch", org_path, -8);
    return;
  }
	//skeleton implementation
  int i, j;

  struct sfs_inode inode_tc;
  struct sfs_inode inode;
  struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

  disk_read(&inode_tc, sd_cwd.sfd_ino);

	for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_tc.sfi_direct[i] == 0) {
      break;
    }
    disk_read(dir_entry, inode_tc.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      if (strcmp(org_path, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) {
        error_message("touch", org_path, -6);
        return;
      }
    }
  }

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );

	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );

  if (si.sfi_size == SFS_BLOCKSIZE * SFS_NDIRECT) {
    error_message("touch", org_path, -3);
    return;
  }

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6 
	// block 0: superblock,	block 1:root, 	block 2:bitmap 
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined
	
	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	//block access
  int index = si.sfi_size / SFS_BLOCKSIZE;
  //printf("%d", index);
  disk_read(sd, si.sfi_direct[index]);

	//allocate new block
  int newbie_ino;

  for (i = 0; i < SFS_BLOCKSIZE; i++) {
    if (sbm[i].sb_block == 255) continue;
    
    for (j = 0; j < 8; j++) {
      //printf("%d : %d\n", i * 8 + j, BIT_CHECK(sbm[i].sb_block, j));
      if (!BIT_CHECK(sbm[i].sb_block, j)) {
        newbie_ino = i * 8 + j;
        //printf("newbie_ino : %d\n", newbie_ino);
        break;
      }
    }
    if (!BIT_CHECK(sbm[i].sb_block, j)) break;
  }
  BIT_SET(sbm[i].sb_block, j);

  int sd_index;

  for(i = 0; i < SFS_DENTRYPERBLOCK; i++) {
    if (sd[i].sfd_ino == 0) {
      sd_index = i;
      //printf("sd_index : %d\n", sd_index);
      break;
    }
  }

	sd[i].sfd_ino = newbie_ino;
	strncpy( sd[i].sfd_name, org_path, SFS_NAMELEN );

	disk_write( sd, si.sfi_direct[index] );

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );
  
  int newbie_ino2;

  for (i = 0; i < SFS_BLOCKSIZE; i++) {
    if (sbm[i].sb_block == 255) continue;
    
    for (j = 0; j < 8; j++) {
      //printf("%d : %d\n", i * 8 + j, BIT_CHECK(sbm[i].sb_block, j));
      if (!BIT_CHECK(sbm[i].sb_block, j)) {
        newbie_ino2 = i * 8 + j;
        //printf("newbie_ino : %d\n", newbie_ino);
        break;
      }
    }
    if (!BIT_CHECK(sbm[i].sb_block, j)) break;
  }
  BIT_SET(sbm[i].sb_block, j);

  struct sfs_inode newbie;
  struct sfs_dir new_sd[SFS_DENTRYPERBLOCK];

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
  bzero(new_sd, sizeof(struct sfs_dir)*SFS_DENTRYPERBLOCK);
  newbie.sfi_direct[0] = newbie_ino2;
  new_sd[0].sfd_ino = newbie_ino;
  strcpy(new_sd[0].sfd_name, ".\0");
  //printf("%d %s\n", new_sd[0].sfd_ino, new_sd[0].sfd_name);
  new_sd[1].sfd_ino = sd_cwd.sfd_ino;
  strcpy(new_sd[1].sfd_name, "..\0");
  //printf("%d %s\n", new_sd[1].sfd_ino, new_sd[1].sfd_name);
  disk_write(new_sd, newbie.sfi_direct[0]);
	newbie.sfi_size = 128;
	newbie.sfi_type = SFS_TYPE_DIR;
  

	disk_write( &newbie, newbie_ino );
  disk_write(sbm, SFS_MAP_LOCATION);
}

void sfs_rmdir(const char* org_path) 
{
  if (strcmp(org_path, ".") == 0 || strcmp(org_path, "..") == 0) {
    error_message("rmdir", org_path, -8);
    return;
  }
  int i, j;
  struct sfs_inode inode_rm;
  struct sfs_inode inode;
  struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

  int find_entrynum = -1;
  int find_dirnum = -1;

  disk_read(&inode_rm, sd_cwd.sfd_ino);

	for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_rm.sfi_direct[i] == 0) {
      break;
    }
    disk_read(dir_entry, inode_rm.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      disk_read(&inode, dir_entry[j].sfd_ino);
      if (strcmp(org_path, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) {
        if (inode.sfi_type != SFS_TYPE_DIR){
          error_message("rmdir", org_path, -2);
          return;
        }
        find_dirnum = i;
        find_entrynum = j;
        break;
      }
    }
    if (strcmp(org_path, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) break;
  }

  if (inode.sfi_size != 128) {
    error_message("rmdir", org_path, -7);
    return;
  }

  if (find_entrynum == -1) {
    error_message("rmdir", org_path, -1);
    return;
  }

  int tmp = dir_entry[find_entrynum].sfd_ino;
  int b_i, b_j;
  b_i = tmp / 8;
  b_j = tmp % 8;
  BIT_CLEAR(sbm[b_i].sb_block, b_j);

  tmp = inode_rm.sfi_direct[i];
  b_i = tmp / 8;
  b_j = tmp % 8;

  dir_entry[find_entrynum].sfd_ino = SFS_NOINO;
  if (find_entrynum == 0) {
    inode_rm.sfi_direct[find_dirnum] = 0;
  }
  inode_rm.sfi_size -= sizeof(struct sfs_dir);

  disk_write(dir_entry, inode_rm.sfi_direct[find_dirnum]);
  disk_write(&inode_rm, sd_cwd.sfd_ino);
  disk_write(sbm, SFS_MAP_LOCATION);

  return;
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
  if (strcmp(src_name, ".") == 0 || strcmp(src_name, "..") == 0) {
    error_message("mv", src_name, -8);
    return;
  }
  if (strcmp(dst_name, ".") == 0 || strcmp(dst_name, "..") == 0) {
    error_message("mv", dst_name, -8);
    return;
  }

  int i, j;
  struct sfs_inode inode_mv;
  struct sfs_inode inode;
  struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

  int find_entrynum = -1;
  int find_dirnum = -1;

  disk_read(&inode_mv, sd_cwd.sfd_ino);

	for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_mv.sfi_direct[i] == 0) {
      break;
    }
    disk_read(dir_entry, inode_mv.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      if (strcmp(dst_name, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) {
        error_message("mv", dst_name, -6);
        return;
      }
      if (strcmp(src_name, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) {
        find_dirnum = i;
        find_entrynum = j;
      }
    }
  }

  if (find_entrynum == -1) {
    error_message("mv", src_name, -1);
    return;
  }

  strcpy(dir_entry[find_entrynum].sfd_name, dst_name);

  disk_write(dir_entry, inode_mv.sfi_direct[find_dirnum]);
  disk_write(&inode_mv, sd_cwd.sfd_ino);

  return;
}

void sfs_rm(const char* path) 
{
	if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) {
    error_message("rm", path, -8);
    return;
  }
  int i, j;
  struct sfs_inode inode_rm;
  struct sfs_inode inode;
  struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
  struct sfs_dir dir_entry_rm[SFS_DENTRYPERBLOCK];

  int find_entrynum = -1;
  int find_dirnum = -1;

  disk_read(&inode_rm, sd_cwd.sfd_ino);

	for (i = 0; i < SFS_NDIRECT; i++) {
    if (inode_rm.sfi_direct[i] == 0) {
      break;
    }
    disk_read(dir_entry, inode_rm.sfi_direct[i]);
    for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
      disk_read(&inode, dir_entry[j].sfd_ino);
      if (strcmp(path, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) {
        if (inode.sfi_type != SFS_TYPE_FILE){
          error_message("rm", path, -10);
          return;
        }
        find_dirnum = i;
        find_entrynum = j;
        break;
      }
    }
    if (strcmp(path, dir_entry[j].sfd_name) == 0 && dir_entry[j].sfd_ino != 0) break;
  }

  if (find_entrynum == -1) {
    error_message("rm", path, -1);
    return;
  }

  int tmp = dir_entry[find_entrynum].sfd_ino;
  //printf("%d\n", tmp);
  int b_i, b_j;
  b_i = tmp / 8;
  b_j = tmp % 8;
  //printf("%d %d \n", b_i, b_j);
  BIT_CLEAR(sbm[b_i].sb_block, b_j);
  dir_entry[find_entrynum].sfd_ino = SFS_NOINO;
  if (find_entrynum == 0) {
    inode_rm.sfi_direct[find_dirnum] = 0;
  }
  inode_rm.sfi_size -= sizeof(struct sfs_dir);

  disk_write(dir_entry, inode_rm.sfi_direct[find_dirnum]);
  disk_write(&inode_rm, sd_cwd.sfd_ino);
  disk_write(sbm, SFS_MAP_LOCATION);
  return;
}

void sfs_cpin(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void sfs_cpout(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
