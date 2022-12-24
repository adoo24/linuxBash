//
// Simple FIle System
// Student Name :   김준서
// Student Number :    B735113
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/
#include <errno.h>
#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"
static char path_name[100];
void dump_directory();
int findEmpty();
/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))
#define SFS_BENTRYPERBLOCK (SFS_BLOCKSIZE/sizeof(struct bitmap))
void disk_reading(void *data, u_int32_t block,const char *path,unsigned int size)
{
    char *cdata = data;
    int i;
    for(i=0;i<512-size;i++){
        cdata[i]='\0';
    }
    u_int32_t tot=0;
    long len;
    int fd = open(path,O_RDWR);
    assert(fd>=0);

    if (lseek(fd, block*512, SEEK_SET)<0) {
        err(1, "lseek");
    }
    tot=size;
    while (tot < 512) {
        len = read(fd, cdata, 512 - tot);
        if (len < 0) {
            if (errno==EINTR || errno==EAGAIN) {
                continue;
            }
            err(1, "read");
        }
        if (len==0) {
            break;
        }
        tot += len;
    }
}
void disk_writing(const void *data, u_int32_t block,const char *path,unsigned int size)
{
    const char *cdata = data;
    u_int32_t tot=0;
    long len;
    int fd=open(path,O_RDWR);
    assert(fd>=0);

    if (lseek(fd, block*512, SEEK_SET)<0) {
        err(1, "lseek");
    }
    tot=size;
    while (tot < 512) {
        len = write(fd, cdata, 512 - tot);
        if (len < 0) {
            if (errno==EINTR || errno==EAGAIN) {
                continue;
            }
            err(1, "write");
        }
        if (len==0) {
            break;
        }
        tot += len;
    }
}
struct bitmap{
    unsigned char byte;
};
struct indirect{
    int addr;
};

static struct sfs_super spb;	// superblock
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
    case -11:
        printf("%s: %s: Directory not empty\n",message, path); return;
    case -12:
            printf("%s: %s: input file size exceeds the max file size\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}
void free_block(int num){
    if (num==0) return;
    int number,k;
    int inode_num = num/(512*8)+2;
    int inode_offset=num%(512*8);
    struct bitmap bit[SFS_BENTRYPERBLOCK];
    disk_read(&bit, inode_num);
    number=bit[inode_offset/8].byte;
    k=inode_offset%8;
    bit[inode_offset/8].byte=bit[inode_offset/8].byte-(1<<k);
    disk_write(&bit, inode_num);
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
    strcpy(path_name,path);
	disk_read( &spb, SFS_SB_LOCATION );

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
	//skeleton implementation
    int i=0,j=0,k=0;
	struct sfs_inode si;
    int makeIf;
	disk_read( &si, sd_cwd.sfd_ino );

	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );

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
    for(j=0;j<15;j++){
        if(k==100) {j--;break;}
        if(si.sfi_direct[j]==0){            //새로운 블락 만듬
            makeIf=findEmpty();
            if(makeIf==0){
                error_message("touch", path, -4);   //block 없음
            }
            struct sfs_dir dir[SFS_DENTRYPERBLOCK];
            bzero(&dir, 512);
            disk_write(dir, makeIf);
            si.sfi_direct[j]=makeIf;
            disk_write(&si, sd_cwd.sfd_ino);
        }
        disk_read( sd, si.sfi_direct[j] );
        //allocate new block
        
        for(i=0;i<SFS_DENTRYPERBLOCK;i++){
            if(strncmp(sd[i].sfd_name,path,SFS_NAMELEN)==0&&sd[i].sfd_ino!=0){
                error_message("touch", path, -6);
                return;
            }
            if(sd[i].sfd_ino==0){           //빈 dirEntry
                k=100;
                break;
            }
        
        }
        
    }
    if(k!=100){
        error_message("touch", path, -3);
        return;
    }
    if(j==15){
        j--;
    }
    int newbie_ino= findEmpty();
    if(newbie_ino==0){
        error_message("touch", path, -4);
        return;
    }
	sd[i].sfd_ino = newbie_ino;
	strncpy( sd[i].sfd_name, path, SFS_NAMELEN );
	disk_write( &sd, si.sfi_direct[j] );
	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );
	struct sfs_inode newbie;
	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write( &newbie, newbie_ino );
}
struct twoAns {
    int x;
    int y;
};
int findEmpty(){        //완벽하진 않음
    int i=0,j,num,k,tmp,bitmaps;
    struct bitmap bit[SFS_BENTRYPERBLOCK];
    struct sfs_super super;
    disk_read(&super, 0);
    if(super.sp_nblocks%(512*8)>0) bitmaps=super.sp_nblocks/(512*8)+1;
    else bitmaps=super.sp_nblocks/(512*8);
    for(i=2;i<2+bitmaps;i++){
        disk_read(&bit, i);
        for(j=0;j<512;j++){
            num=bit[j].byte;
            for(k=0;k<=7;k++){
                tmp=(num>>k)%2;
                if (tmp==0) {
                    bit[j].byte=bit[j].byte+(1<<k);
                    disk_write(&bit, i);
                    return 8*(j)+k+512*8*(i-2);
                }
            }
        }
        
    }
    return 0; //문제
}
struct twoAns Ans(int empty){
    struct twoAns twoAns;
    twoAns.x=empty/512;
    twoAns.y=empty%512;
    return twoAns;
}

void sfs_cd(const char* path)
{
    int i,j;
    struct sfs_inode si;
    struct sfs_inode inode;
    disk_read( &si, sd_cwd.sfd_ino );

    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    if(path==NULL){
        sd_cwd.sfd_ino = 1;        //init at root
        sd_cwd.sfd_name[0] = '/';
        sd_cwd.sfd_name[1] = '\0';
        return;
    }
    for(i=0;i<SFS_NDIRECT;i++){
        disk_read(sd, si.sfi_direct[i]);
        for(j=0;j<SFS_DENTRYPERBLOCK;j++){
            if(sd[j].sfd_ino==0) continue;
            disk_read(&inode,sd[j].sfd_ino);
            if(strncmp(sd[j].sfd_name, path, SFS_NAMELEN)==0){
                if(inode.sfi_type!=SFS_TYPE_DIR){
                    error_message("cd", path, -2);
                    return;
                }
                sd_cwd.sfd_ino=sd[j].sfd_ino;
                strcpy(sd_cwd.sfd_name,sd[j].sfd_name);
                return;
            }
            
            
        }
        if (j==SFS_DENTRYPERBLOCK){
            error_message("ls", path, -1);
            return;
        }


    }
    error_message("cd", path, -1);
    return;
    
    
}

void sfs_ls(const char* path)
{
    int i,j=0;
    struct sfs_inode si,sa;
    struct sfs_inode inode;
    if(path==NULL)  disk_read( &si, sd_cwd.sfd_ino );
    else{
        disk_read(&sa, sd_cwd.sfd_ino);
        struct sfs_dir sf[SFS_DENTRYPERBLOCK];
        for(i=0;i<SFS_NDIRECT;i++){
            if(sa.sfi_direct[i]==0) break;
            if(j==999999) break;
            disk_read(sf, sa.sfi_direct[i]);
            for(j=0;j<SFS_DENTRYPERBLOCK;j++){
                if(sf[j].sfd_ino==0) continue;
                if(strncmp(sf[j].sfd_name, path, SFS_NAMELEN)==0){
                    disk_read(&inode, sf[j].sfd_ino);
                    if(inode.sfi_type!=SFS_TYPE_DIR){
                        error_message("ls", path, -2);
                        return;
                    }
                    disk_read(&si, sf[j].sfd_ino);
                    j=999999;
                    break;
                }
            }
            if (j==SFS_DENTRYPERBLOCK){
                error_message("ls", path, -1);
                return;
            }
        }
    }
    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    
    if (si.sfi_type == SFS_TYPE_DIR) {
        for(i=0; i < SFS_NDIRECT; i++) {
            if (si.sfi_direct[i] == 0) break;
            disk_read(sd, si.sfi_direct[i]);
            for(j=0; j < SFS_DENTRYPERBLOCK;j++) {
                if(sd[j].sfd_ino==0) continue;
                printf("%s", sd[j].sfd_name);
                disk_read(&inode,sd[j].sfd_ino);
                if (inode.sfi_type == SFS_TYPE_DIR) {
                    printf("/");
                }
                printf("\t");
            }
        }
        printf("\n");
    }
    
    
    
}
struct sfs_inode parent(struct sfs_inode inode){
    struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
    struct sfs_inode tmp;
    disk_read(&dir_entry, inode.sfi_direct[0]);
    disk_read(&tmp,dir_entry[1].sfd_ino);
    return tmp;
}
void sfs_mkdir(const char* org_path) 
{
    int i=0,j=0,k=0,makeIf;
    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );

    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    //block access
    for(j=0;j<SFS_NDIRECT;j++){
        if(k==100) {j--;break;}
        if(si.sfi_direct[j]==0){            //새로운 블락 만듬
            makeIf=findEmpty();
            if(makeIf==0){
                error_message("mkdir", org_path, -4);   //block 없음
                return;
            }
            struct sfs_dir dir[SFS_DENTRYPERBLOCK];
            bzero(&dir, 512);
            disk_write(dir, makeIf);
            si.sfi_direct[j]=makeIf;
            disk_write(&si,sd_cwd.sfd_ino);
        }
        disk_read( sd, si.sfi_direct[j] );
        //allocate new block
        for(i=0;i<SFS_DENTRYPERBLOCK;i++){
            if(strncmp(sd[i].sfd_name,org_path,SFS_NAMELEN)==0&&sd[i].sfd_ino!=0){
                error_message("mkdir", org_path, -6);
                return;
            }
            if(sd[i].sfd_ino==0){           //빈 dirEntry
                k=100;
                break;
            }
        
        }
    }
    if(k!=100){
        error_message("mkdir", org_path, -3);
        return;
    }
    if(j==15){
        j--;
    }
    int newbie_ino= findEmpty();
    if(newbie_ino==0){
        error_message("mkdir", org_path, -4);   //block 없음
        return;
    }
    int newbie_ino2= findEmpty();
    if(newbie_ino2==0){
        error_message("mkdir", org_path, -4);   //block 없음
        return;
    }
    sd[i].sfd_ino = newbie_ino;             //dir Entry 갱신
    strncpy( sd[i].sfd_name, org_path, SFS_NAMELEN );
    disk_write( &sd, si.sfi_direct[j] );
    si.sfi_size += 64;
    disk_write( &si, sd_cwd.sfd_ino );      //현재 디렉토리 파일크기 갱신
    struct sfs_inode newbie;
    bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
    newbie.sfi_size = 128;
    newbie.sfi_type = SFS_TYPE_DIR;
    newbie.sfi_direct[0]=newbie_ino2;
    disk_write( &newbie, newbie_ino );
                                        
    //dir파일 inode 쓰기 완료
    
    struct sfs_dir newbie2[SFS_DENTRYPERBLOCK];
    bzero(&newbie2, SFS_BLOCKSIZE);
    newbie2[0].sfd_ino=newbie_ino;
    newbie2[0].sfd_name[0]='.';
    newbie2[1].sfd_name[0]='.';
    newbie2[1].sfd_name[1]='.';
    newbie2[1].sfd_ino=sd_cwd.sfd_ino;
    disk_write(&newbie2, newbie_ino2);
    
}

void sfs_rmdir(const char* org_path) 
{
    int i=0,j=0,k=0,num;
    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );
    const char a[10]=".";
    const char b[10]="..";
    if(strncmp(org_path,a,10)==0||strncmp(org_path, b, 10)==0){
        error_message("rmdir", org_path, -8);
        return;
    }
    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    //block access
    for(j=0;j<15;j++){
        if(k==100) {k=0;j--;break;}
        disk_read( sd, si.sfi_direct[j] );
        //allocate new block
        for(i=0;i<SFS_DENTRYPERBLOCK;i++){
            if(strncmp(sd[i].sfd_name,org_path,SFS_NAMELEN)==0){
                k=100;
                break;
            }
            if(sd[i].sfd_ino==0){           //빈 dirEntry
                error_message("rmdir", org_path, -1);
                return;
            }
        
        }
    }
    int shouldD_inode=sd[i].sfd_ino;      //지워야할 inode 번호(dir)
    struct sfs_inode tmp;
    disk_read(&tmp, shouldD_inode);
    if(tmp.sfi_type!=SFS_TYPE_DIR){
        error_message("rmdir", org_path, -2);
        return;
    }
    if(tmp.sfi_size>128){
        error_message("rmdir", org_path, -11);
        return;
    }
    int shouldD_entry = tmp.sfi_direct[0];     //지워야할 dir inode번호(dirEntry)
    struct bitmap bit[SFS_BENTRYPERBLOCK];
    
    
    
    
    sd[i].sfd_ino = 0;             //dir Entry 갱신
    disk_write( &sd, si.sfi_direct[j] );
    si.sfi_size -= 64;
    disk_write( &si, sd_cwd.sfd_ino );      //현재 디렉토리 파일크기 갱신
    
    int inode_num=shouldD_inode/(512*8)+2;
    int dirE_num=shouldD_entry/(512*8)+2;
    int inode_offset=shouldD_inode%(512*8);
    int dirE_offset=shouldD_entry%(512*8);
    disk_read(&bit, inode_num);
    num=bit[inode_offset/8].byte;
    k=inode_offset%8;
    bit[inode_offset/8].byte=bit[inode_offset/8].byte-(1<<k);
    disk_write(&bit, inode_num);
    
    
    disk_read(&bit, dirE_num);
    num=bit[dirE_offset/8].byte;
    k=dirE_offset%8;
    bit[dirE_offset/8].byte=bit[dirE_offset/8].byte-(1<<k);
    disk_write(&bit, dirE_num);
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
    int i=0,j=0,k=0;
    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );       //현재 inode

    assert( si.sfi_type == SFS_TYPE_DIR );
    int numi=0,numj=0,check=0;
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    //block access
    for(j=0;j<15;j++){
        if(k==100) {j--;break;}
        disk_read( sd, si.sfi_direct[j] );      //현재 아이노드의 엔트리 받아옴
        //allocate new block
        for(i=0;i<SFS_DENTRYPERBLOCK;i++){
            if(strncmp(sd[i].sfd_name,src_name,SFS_NAMELEN)==0&&sd[i].sfd_ino!=0){
                if(check==0){
                    numi=i;
                    numj=j;
                    check=1;
                }
                else{
                    error_message("mv", dst_name, -6);
                    return;
                }
            }
            if(strncmp(sd[i].sfd_name, dst_name, SFS_NAMELEN)==0&&sd[i].sfd_ino!=0){
                error_message("mv", dst_name, -6);
                return;
            }
            
            if(sd[i].sfd_ino==0){           //빈 dirEntry
                k=100;
                break;
            }
        
        }
        
        
    }
    if (numi==0&&numj==0){
        error_message("mv", src_name, -1);
        return;
    }
    else{
        disk_read(&sd, si.sfi_direct[numj]);
        strcpy(sd[numi].sfd_name,dst_name);
        disk_write(&sd, si.sfi_direct[numj]);
    }
}

void sfs_rm(const char* path) 
{
    int i=0,j=0,k=0,num,delete;
    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );

    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    struct indirect sb[512/4];
    //block access
    for(j=0;j<15;j++){
        if(k==100) {j--;break;}
        disk_read( sd, si.sfi_direct[j] );
        //allocate new block
        for(i=0;i<SFS_DENTRYPERBLOCK;i++){
            if(strncmp(sd[i].sfd_name,path,SFS_NAMELEN)==0&&sd[i].sfd_ino!=0){
                k=100;
                break;
            }
        
        }
        
    }
    
    if(k!=100){
        error_message("rm", path, -1);      //못찾음
        return;
    }
    int shouldD_inode=sd[i].sfd_ino;      //지워야할 inode 번호(dir)
    int shouldD_entry;
    struct sfs_inode tmp;
    disk_read(&tmp, shouldD_inode);
    if(tmp.sfi_type!=SFS_TYPE_FILE){
        error_message("rm", path, -10);
        return;
    }
    sd[i].sfd_ino = 0;             //dir Entry 갱신
    disk_write( &sd, si.sfi_direct[j] );
    si.sfi_size -= 64;
    disk_write( &si, sd_cwd.sfd_ino );      //현재 디렉토리 파일크기 갱신
    free_block(shouldD_inode);
    for(i=0;i<15;i++){
        shouldD_entry = tmp.sfi_direct[i];      //지워야할 dir inode번호(dirEntry)
        if(shouldD_entry==0) break;
        
        free_block(shouldD_entry);
    }
    if(tmp.sfi_indirect==0) return;
    disk_read(&sb, tmp.sfi_indirect);
    free_block(tmp.sfi_indirect);
    for(i=0;i<512/4;i++){
        shouldD_entry=sb[i].addr;
        free_block(shouldD_entry);
    }
    
}
    






void sfs_cpin(const char* local_path, const char* path) 
{
    int i,j=0,k=0,makeIf;
    long size,real_size;
    FILE *fp = fopen(path,"r");
    if(fp==NULL){
        error_message("cpin", path, -1);
        return;
    }
    fseek(fp,0,SEEK_END);
    if(ftell(fp)%512==0){
        size=ftell(fp)/512-1;
    }
    else size=ftell(fp)/512;
    real_size=ftell(fp);
    if(real_size>512*512/4+7680){
        error_message("cpin", local_path, -12);
        return;
    }
    struct sfs_inode si;
    struct sfs_inode inode;
    struct bitmap temp[SFS_BENTRYPERBLOCK];
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    bzero(&inode,512);
    inode.sfi_type=SFS_TYPE_FILE;
    disk_read(&si, sd_cwd.sfd_ino);
    for(i=0;i<SFS_NDIRECT;i++){
        if(k==100){break;}
        if(si.sfi_direct[i]==0){            //새로운 블락 만듬
            makeIf=findEmpty();
            if(makeIf==0){
                error_message("cpin1", local_path, -4);   //block 없음
                return;
            }
            struct sfs_dir dir[SFS_DENTRYPERBLOCK];
            bzero(&dir, 512);
            disk_write(dir, makeIf);
            si.sfi_direct[i]=makeIf;
            disk_write(&si, sd_cwd.sfd_ino);
        }
        disk_read( sd, si.sfi_direct[i] );
        for(j=0;j<SFS_DENTRYPERBLOCK;j++){
            if(strncmp(sd[j].sfd_name,local_path,SFS_NAMELEN)==0&&sd[j].sfd_ino!=0){
                error_message("cpin", local_path, -6);
                return;
            }
            if(sd[j].sfd_ino==0){
                k=100;
                break;
            }
        }
    }

    if(k!=100){
        error_message("cpin2", local_path, -4); //entry 자리가 업슴
        return;
    }
    int newbie_ino=findEmpty();
    if(newbie_ino==0){
        error_message("cpin3", local_path, -4);  //block이 없음
        return;
    }
    si.sfi_size+=64;
    sd[j].sfd_ino=newbie_ino;
    strcpy(sd[j].sfd_name,local_path);
    disk_write(&si, sd_cwd.sfd_ino);
    disk_write(&sd, si.sfi_direct[i-1]);
    inode.sfi_size=real_size;
    i=0;
    while(i<=size){
        if(i>=15) break;
        if(i==size){
            k=findEmpty();
            if(k==0){
                error_message("cpin", local_path, -4);
                return;
            }
            inode.sfi_direct[i]=k;
            disk_reading(&temp, i, path, 512-(real_size%512));
            disk_writing(&temp, k, path_name, 512-(real_size%512));
            disk_write(&inode, newbie_ino);
            return;
        }
        disk_close();
        disk_open(path);
        disk_read(&temp, i);
        disk_close();
        disk_open(path_name);
        k=findEmpty();
        if(k==0){
            error_message("cpin4", local_path, -4);
            return;
        }
        if(k!=0){
            disk_write(&temp, k);
            inode.sfi_direct[i]=k;
            i++;
            disk_close();
            disk_open(path_name);
            disk_write(&inode, newbie_ino);
        }
        else{
            error_message("cpin5", local_path, -4);
            return;
        }
    }
    int indir=findEmpty();
    if(indir==0){
        error_message("cpin6", local_path, -4);
        return;
    }
    int l=0;
    inode.sfi_indirect=indir;
    disk_write(&inode, newbie_ino);
    struct indirect ind[512/4];
    bzero(ind, 512);
    disk_write(ind, indir);
    while(i<=size){
        if(l==512/4){
            error_message("cpin7", local_path, -4);
            return;
        }
        if(i==size){
            k=findEmpty();
            if(k==0){
                error_message("cpin", local_path, -4);
                return;
            }
            ind[l].addr=k;
            disk_write(ind, indir);
            disk_reading(&temp, i, path, 512-(real_size%512));
            disk_writing(&temp, k, path_name, 512-(real_size%512));
            return;
        }
        disk_close();
        disk_open(path);
        disk_read(&temp, i);
        disk_close();
        disk_open(path_name);
        i++;
        k=findEmpty();
        if(k==0){
            error_message("cpin8", local_path, -4);
            return;
        }
        if(k==0){
            error_message("cpin9", local_path, -4);
            return;
        }
        else{
            disk_write(&temp, k);
            ind[l].addr=k;
            l++;
            disk_close();
            disk_open(path_name);
            disk_write(ind, indir);
        }
    }
    
}

void sfs_cpout(const char* local_path, const char* path) 
{
    int i=0,j=0,k=0,check;
    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );
    FILE *file;
    if(access( path, F_OK ) != -1){
        error_message("cpout", path, -6);
        return;
    }
    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    struct indirect sb[512/4];
    bzero(sb, 512);
    //block access
    for(j=0;j<15;j++){
        if(k==100) {j--;break;}
        disk_read( sd, si.sfi_direct[j] );
        //allocate new block
        for(i=0;i<SFS_DENTRYPERBLOCK;i++){
            if(strncmp(sd[i].sfd_name,local_path,SFS_NAMELEN)==0&&sd[i].sfd_ino!=0){
                k=100;
                break;
            }
        }
    }
    if(k!=100){
        error_message("cpout", local_path, -1);      //못찾음
        return;
    }
    file=fopen(path, "w");
    int shouldC_inode=sd[i].sfd_ino;      //복사해야할 inode 번호(dir)
    int shouldC_entry;
    struct sfs_inode tmp;
    struct bitmap temp[SFS_BENTRYPERBLOCK];
    disk_read(&tmp, shouldC_inode);
    disk_write( &sd, si.sfi_direct[j] );
    disk_write( &si, sd_cwd.sfd_ino );      //현재 디렉토리 파일크기 갱신
    for(i=0;i<15;i++){
        shouldC_entry = tmp.sfi_direct[i];      //지워야할 dir inode번호(dirEntry)
        if(shouldC_entry==0) break;
        if(i<14){
            check = tmp.sfi_direct[i+1];
            if(check==0){
                int a=tmp.sfi_size%512;
                if(a==0){
                    disk_reading(&temp, shouldC_entry, path_name, 0);
                    disk_writing(&temp, i, path, 0);
                }
                else {
                    struct bitmap ttmp[a];
                    disk_reading(ttmp,shouldC_entry,path_name,512-(tmp.sfi_size%512));
                    disk_writing(ttmp, i, path,512-(tmp.sfi_size%512));
                }
                disk_close();
                disk_open(path_name);
                break;
            }
        }
        disk_read(&temp,shouldC_entry);
        disk_close();
        disk_open(path);
        disk_writing(&temp, i,path,0);
        disk_close();
        disk_open(path_name);
    }
    if(tmp.sfi_indirect==0) {
        return;
    }
    disk_read(&sb, tmp.sfi_indirect);
    for(i=0;i<512/4;i++){
        shouldC_entry=sb[i].addr;
        if(shouldC_entry==0) continue;
        check=sb[i+1].addr;
        if(check==0){
            int a = tmp.sfi_size%512;
            if(a==0){
                disk_reading(&temp, shouldC_entry, path_name, 0);
                disk_writing(&temp, i+15, path, 0);
            }
            else{
                struct bitmap ttmp[a];
                disk_reading(ttmp,shouldC_entry,path_name,512-a);
                disk_writing(ttmp, i+15, path, 512-a);
            }
            disk_close();
            disk_open(path_name);
            break;
        }
        disk_reading(&temp, shouldC_entry,path_name,0);
        disk_close();
        disk_open(path);
        disk_writing(&temp, i+15,path,0);
        disk_close();
        disk_open(path_name);
    }
    
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
//void sfs_fsck(){
//    printf("Not Implemented\n");
//}
//void sfs_bitmap(){
//    struct bitmap bit[SFS_BENTRYPERBLOCK];
//    int i,j,k;
//    for(i=2;i<5;i++){                           //비트맵 갯수에 따라
//        disk_read(&bit,i);
//        printf("BITMAP %d==============\n",i);
//        for(k=0;k<SFS_BENTRYPERBLOCK;k++){
//            printf("[%d]    ",k);
//            for(j=0;j<=7;j++){
//                printf("%d",(bit[k].byte>>j)%2);
//            }
//            printf("\n");
//        }
//    }
//}

