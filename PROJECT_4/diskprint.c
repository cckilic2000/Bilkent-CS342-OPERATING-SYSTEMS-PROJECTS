#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
//#include <linux/ext2_fs.h>

#define MAXFILENAME 256
#define BLOCKSIZE 4096

struct ext2_super_block {
    __u32	s_inodes_count;		/* Inodes count */
    __u32	s_blocks_count;		/* Blocks count */
    __u32	s_r_blocks_count;	/* Reserved blocks count */
    __u32	s_free_blocks_count;	/* Free blocks count */
    __u32	s_free_inodes_count;	/* Free inodes count */
    __u32	s_first_data_block;	/* First Data Block */
    __u32	s_log_block_size;	/* Block size */
    __s32	s_log_frag_size;	/* Fragment size */
    __u32	s_blocks_per_group;	/* # Blocks per group */
    __u32	s_frags_per_group;	/* # Fragments per group */
    __u32	s_inodes_per_group;	/* # Inodes per group */
    __u32	s_mtime;		/* Mount time */
    __u32	s_wtime;		/* Write time */
    __u16	s_mnt_count;		/* Mount count */
    __s16	s_max_mnt_count;	/* Maximal mount count */
    __u16	s_magic;		/* Magic signature */
    __u16	s_state;		/* File system state */
    __u16	s_errors;		/* Behaviour when detecting errors */
    __u16	s_minor_rev_level; 	/* minor revision level */
    __u32	s_lastcheck;		/* time of last check */
    __u32	s_checkinterval;	/* max. time between checks */
    __u32	s_creator_os;		/* OS */
    __u32	s_rev_level;		/* Revision level */
    __u16	s_def_resuid;		/* Default uid for reserved blocks */
    __u16	s_def_resgid;		/* Default gid for reserved blocks */
    __u32	s_first_ino; 		/* First non-reserved inode */
    __u16   s_inode_size; 		/* size of inode structure */
    __u16	s_block_group_nr; 	/* block group # of this superblock */
    __u32	s_feature_compat; 	/* compatible feature set */
    __u32	s_feature_incompat; 	/* incompatible feature set */
    __u32	s_feature_ro_compat; 	/* readonly-compatible feature set */
    __u8	s_uuid[16];		/* 128-bit uuid for volume */
    char	s_volume_name[16]; 	/* volume name */
    char	s_last_mounted[64]; 	/* directory where last mounted */
    __u32	s_algorithm_usage_bitmap; /* For compression */
    __u8	s_prealloc_blocks;	/* Nr of blocks to try to preallocate*/
    __u8	s_prealloc_dir_blocks;	/* Nr to preallocate for dirs */
    __u16	s_padding1;
    __u32	s_reserved[204];	/* Padding to the end of the block */
};


struct ext2_inode {
    __u16	i_mode;		/* File mode */
    __u16	i_uid;		/* Low 16 bits of Owner Uid */
    __u32	i_size;		/* Size in bytes */
    __u32	i_atime;	/* Access time */
    __u32	i_ctime;	/* Creation time */
    __u32	i_mtime;	/* Modification time */
    __u32	i_dtime;	/* Deletion Time */
    __u16	i_gid;		/* Low 16 bits of Group Id */
    __u16	i_links_count;	/* Links count */
    __u32	i_blocks;	/* Blocks count */
    __u32	i_flags;	/* File flags */
    union {
        struct {
            __u32  l_i_reserved1;
        } linux1;
        struct {
            __u32  h_i_translator;
        } hurd1;
        struct {
            __u32  m_i_reserved1;
        } masix1;
    } osd1;				/* OS dependent 1 */
    __u32	i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
    __u32	i_generation;	/* File version (for NFS) */
    __u32	i_file_acl;	/* File ACL */
    __u32	i_dir_acl;	/* Directory ACL */
    __u32	i_faddr;	/* Fragment address */
    union {
        struct {
            __u8	l_i_frag;	/* Fragment number */
            __u8	l_i_fsize;	/* Fragment size */
            __u16	i_pad1;
            __u16	l_i_uid_high;	/* these 2 fields    */
            __u16	l_i_gid_high;	/* were reserved2[0] */
            __u32	l_i_reserved2;
        } linux2;
        struct {
            __u8	h_i_frag;	/* Fragment number */
            __u8	h_i_fsize;	/* Fragment size */
            __u16	h_i_mode_high;
            __u16	h_i_uid_high;
            __u16	h_i_gid_high;
            __u32	h_i_author;
        } hurd2;
        struct {
            __u8	m_i_frag;	/* Fragment number */
            __u8	m_i_fsize;	/* Fragment size */
            __u16	m_pad1;
            __u32	m_i_reserved2[2];
        } masix2;
    } osd2;				/* OS dependent 2 */
};


struct ext2_dir_entry_2 {
    __u32	inode;			/* Inode number */
    __u16	rec_len;		/* Directory entry length */
    __u8	name_len;		/* Name length */
    __u8	file_type;
    char	name[EXT2_NAME_LEN];	/* File name */
};



int main(int argc, char *argv[]){
    char file_name[MAXFILENAME];
    strcpy(file_name , argv[1]);

    int fd;
    int n;
    off_t offset;
    int blocknum;
    unsigned char buffer[BLOCKSIZE];

    fd = open(file_name , O_RDONLY);
    if(fd < 0){
        printf("can't open disk file \n");
        exit(1);
    }

    //SUPER BLOCK
    //byte 1024 to 2048
    struct ext2_super_block super;
    lseek(fd , 1024 , SEEK_SET);
    n = read(fd , &super , sizeof(super));
    if(n == BLOCKSIZE){
        printf ("block read success\n");
    }

    //print contents of super block
    //inodes count
    printf("SUPER BLOCK\n");
    printf("inodes count       : %lu\n" , (unsigned long) super.s_inodes_count);
    printf("blocks count       : %lu\n" , (unsigned long) super.s_blocks_count);
    printf("free blocks count  : %lu\n" , (unsigned long) super.s_free_blocks_count);
    printf("free inodes count  : %lu\n" , (unsigned long) super.s_free_inodes_count);
    printf("first data block   : %lu\n" , (unsigned long) super.s_first_data_block);
    printf("block size         : %lu\n" , (unsigned long) super.s_log_block_size);
    printf("blocks per group   : %lu\n" , (unsigned long) super.s_blocks_per_group);
    printf("magic signature    : %lu\n" , (unsigned long) super.s_magic);
    printf("maximal mount count: %lu\n" , (unsigned long) super.s_max_mnt_count);
    printf("OS                 : %lu\n" , (unsigned long) super.s_creator_os);
    n = 0;

    //CONTENTS OF ROOT DIRECTORY
    //CONTENTS OF THE INODE FOR EACH OF THESE DIRECTORIES
    struct ext2_inode root;
    lseek(fd, 1024 + (super.s_first_ino - 1) * sizeof(root) , SEEK_SET);
    n = read(fd , &root , sizeof(root));
    if (n == BLOCKSIZE) {
        printf ("block read success\n");
    }

    lseek(fd , root.i_block[0] * BLOCKSIZE , SEEK_SET);
    read(fd , root , BLOCKSIZE);

    printf("CONTENTS OF THE ROOT DIRECTORY\n");
    struct ext2_dir_entry_2* ent = (struct ext2_dir_entry_2*) root;
    while(ent->inode != 0){
        printf("File name: %s\n" , ent->name);
        
        struct ext2_inode inode;
        lseek(fd, 1024 + (entry->inode - 1) * sizeof(inode), SEEK_SET);
        read(fd, &inode, sizeof(inode));

        printf("   file mode        : %lu\n" , (unsigned long) inode.i_mode);
        printf("   size             : %lu\n" , (unsigned long) inode.i_size);
        printf("   access time      : %lu\n" , (unsigned long) inode.i_atime);
        printf("   creation time    : %lu\n" , (unsigned long) inode.i_ctime);
        printf("   modification time: %lu\n" , (unsigned long) inode.i_mtime);
        printf("   deletion time    : %lu\n" , (unsigned long) inode.i_dtime);
        printf("   links count      : %lu\n" , (unsigned long) inode.i_links_count);
        printf("   blocks count     : %lu\n" , (unsigned long) inode.i_blocks);
        printf("   file flags       : %lu\n" , (unsigned long) inode.i_flags);
        printf("   fragment address : %lu\n" , (unsigned long) inode.i_faddr);

        ent = (struct ext2_dir_entry_2*) ((char*) ent + ent->rec_len);
    }

    return 0;
}