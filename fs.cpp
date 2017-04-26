//COSC 361 Spring 2017
//FUSE Project Template
//Group Los Hombres
//Group Adriano Santos
//Group Todd Selwitz

#ifndef __cplusplus
#error "You must compile this using C++"
#endif
#include <fuse.h>
#include <cstdio>
#include <map>
#include <vector>
#include <string>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fs.h>

using namespace std;

//Use debugf() and NOT printf() for your messages.
//Uncomment #define DEBUG in block.h if you want messages to show

//Here is a list of error codes you can return for
//the fs_xxx() functions
//
//EPERM          1      /* Operation not permitted */
//ENOENT         2      /* No such file or directory */
//ESRCH          3      /* No such process */
//EINTR          4      /* Interrupted system call */
//EIO            5      /* I/O error */
//ENXIO          6      /* No such device or address */
//ENOMEM        12      /* Out of memory */
//EACCES        13      /* Permission denied */
//EFAULT        14      /* Bad address */
//EBUSY         16      /* Device or resource busy */
//EEXIST        17      /* File exists */
//ENOTDIR       20      /* Not a directory */
//EISDIR        21      /* Is a directory */
//EINVAL        22      /* Invalid argument */
//ENFILE        23      /* File table overflow */
//EMFILE        24      /* Too many open files */
//EFBIG         27      /* File too large */
//ENOSPC        28      /* No space left on device */
//ESPIPE        29      /* Illegal seek */
//EROFS         30      /* Read-only file system */
//EMLINK        31      /* Too many links */
//EPIPE         32      /* Broken pipe */
//ENOTEMPTY     36      /* Directory not empty */
//ENAMETOOLONG  40      /* The name given is too long */

//Use debugf and NOT printf() to make your
//debug outputs. Do not modify this function.
#if defined(DEBUG)
int debugf(const char *fmt, ...)
{
	int bytes = 0;
	va_list args;
	va_start(args, fmt);
	bytes = vfprintf(stderr, fmt, args);
	va_end(args);
	return bytes;
}
#else
int debugf(const char *fmt, ...)
{
	return 0;
}
#endif

//////////////////////////////////////////////////////////////////
//
// START HERE W/ fs_drive()
//
//////////////////////////////////////////////////////////////////
//Read the hard drive file specified by dname
//into memory. You may have to use globals to store
//the nodes and / or blocks.
//Return 0 if you read the hard drive successfully (good MAGIC, etc).
//If anything fails, return the proper error code (-EWHATEVER)
//Right now this returns -EIO, so you'll get an Input/Output error
//if you try to run this program without programming fs_drive.
//////////////////////////////////////////////////////////////////

map<string, NODE *> nodeMap;
map<uint64_t, BLOCK *> blockMap;
map<string, NODE *>::iterator nit;
map<uint64_t, BLOCK *>::iterator bit;
map<uint64_t, BLOCK *>::reverse_iterator rbit;
PBLOCK_HEADER pbh;

int fs_drive(const char *dname)
{
	FILE *fp;
	int i, blockNum;

	//BLOCK_HEADER bh;
	//PBLOCK_HEADER pbh;
	PNODE pn;
	PBLOCK pb;
	
	pbh = (PBLOCK_HEADER) malloc(sizeof(BLOCK_HEADER));
	
	fp = fopen(dname, "r"); 
	
	if(fp == NULL) {
		debugf("fs_drive: %s\n", dname);
		return -EIO;
	}

	fread(pbh, sizeof(BLOCK_HEADER), 1, fp);
	
	for(i = 0; i < 8; i++) {
		if(pbh->magic[i] != MAGIC[i]) {
			return -EINVAL;
		}
	}
	
	debugf("MAGIC: %s\n", pbh->magic);
	debugf("UNUSED: %s\n", pbh->unused);
	debugf("block_size: %ld\n", pbh->block_size);
	debugf("nodes: %ld\nblocks: %ld\n", pbh->nodes, pbh->blocks);	
	
	for(i = 0; i < (int) pbh->nodes; i++) {
		
		pn = (PNODE) malloc(sizeof(NODE));
		fread(pn, 1, ONDISK_NODE_SIZE, fp);
		pn->blocks = NULL;
		
		if(pn->size > 0) {
			blockNum = (pn->size / pbh->block_size) + 1;
			pn->blocks = (uint64_t *) malloc(blockNum * sizeof(uint64_t));
			fread(pn->blocks, 1, sizeof(uint64_t) * blockNum, fp);
		}

		debugf("name %s, id %ld, link_id %ld, mode %ld, size %ld",
				pn->name, pn->id, pn->link_id, pn->mode, pn->size);
			
		nodeMap.insert(make_pair(pn->name, pn));
		
		debugf("\n\n");
	}
	
	for(i = 0; i < (int) pbh->blocks; i++) {
		pb = (PBLOCK) malloc(sizeof(BLOCK));
		pb->data = (char *) malloc(sizeof(char) * pbh->block_size);
		
		fread(pb->data, sizeof(char), pbh->block_size, fp);
		
		blockMap.insert(make_pair(i, pb));

		debugf("%s\n\n", pb->data);
	}
	
	/*
	for(nit = nodeMap.begin(); nit != nodeMap.end(); nit++) {
		debugf("name %s, id %ld, link_id %ld, mode %ld, size %ld\n",
			nit->second->name, nit->second->id, nit->second->link_id,
			nit->second->mode, nit->second->size);
	}
	
	for(bit = blockMap.begin(); bit != blockMap.end(); bit++) {
		debugf("%s\n", bit->second->data);
	}
	*/
	return 0;
}

//////////////////////////////////////////////////////////////////
//Open a file <path>. This really doesn't have to do anything
//except see if the file exists. If the file does exist, return 0,
//otherwise return -ENOENT
//////////////////////////////////////////////////////////////////
int fs_open(const char *path, struct fuse_file_info *fi)
{
	debugf("fs_open: %s\n", path);
	
	nit = nodeMap.find(path);
	
	if(nit != nodeMap.end()) {
		if((nit->second->mode ^ (S_IFREG | nit->second->mode)) == 0)
			return 0;
	}
	return -ENOENT;
}

//////////////////////////////////////////////////////////////////
//Read a file <path>. You will be reading from the block and
//writing into <buf>, this buffer has a size of <size>. You will
//need to start the reading at the offset given by <offset>.
//////////////////////////////////////////////////////////////////
int fs_read(const char *path, char *buf, size_t size, off_t offset,
	    struct fuse_file_info *fi)
{
	debugf("fs_read: %s\n", path);
	
	int byte0, blockO, i, num;
	size_t currentBytes;
	uint64_t* start;

	nit = nodeMap.find(path);
	
	if(nit == nodeMap.end())
		return -ENOENT;

	byte0 = 0;
	blockO = 0;
	i = 0;
	num = 0;
	currentBytes = 0;
	
	if(size > nit->second->size) {
		size = nit->second->size;
	}

	if(offset != 0) {
		blockO = offset / pbh->block_size;
		byte0 = offset % pbh->block_size;
	}
	
	start = nit->second->blocks + blockO;

	if(byte0 != 0) {
		if((size - currentBytes) > (pbh->block_size - byte0)) {
			num = (pbh->block_size - byte0);
		} else {
			num = (size - currentBytes);
		}

		memcpy(buf+currentBytes, blockMap[*start]->data+byte0, num);
		currentBytes += num;
		i++;
	}

	while(currentBytes < size) {
		if(size-currentBytes > pbh->block_size) {
			num = pbh->block_size;
		} else {
			num = (size - currentBytes);
		}
		memcpy(buf+currentBytes, blockMap[*(start + i)]->data, num);
		currentBytes += num;
		i++;
	}
	
	return currentBytes;
}

//////////////////////////////////////////////////////////////////
//Write a file <path>. If the file doesn't exist, it is first
//created with fs_create. You need to write the data given by
//<data> and size <size> into this file block. You will also need
//to write data starting at <offset> in your file. If there is not
//enough space, return -ENOSPC. Finally, if we're a read only file
//system (fi->flags & O_RDONLY), then return -EROFS
//If all works, return the number of bytes written.
//////////////////////////////////////////////////////////////////
int fs_write(const char *path, const char *data, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
		uint64_t* newBlocksize, *start;
		unsigned int i;
		PBLOCK pb;
		off_t block0, byte0;
		int num;
		size_t currentBytes = 0;

		i = 0;
		num = 0;
		block0 = 0;
		byte0 = 0;
		
		if(fi->flags & O_RDONLY) return -EROFS;

		nit = nodeMap.find(path);
		
		if(nit == nodeMap.end()) return -ENOENT;

		if(S_ISDIR(nit->second->mode)) return -EISDIR;

		if(MAX_DRIVE_SIZE < 
		  pbh->block_size * (((size + offset - nit->second->size) / pbh->block_size) + 1 + blockMap.size()))
				
				return -ENOSPC;

		if((nit->second->size / pbh->block_size) < 
		  ((size+offset) / pbh->block_size)) {

				newBlocksize = (uint64_t *) malloc(sizeof(uint64_t) * (10 + ((size+offset) / pbh->block_size)));
				memcpy(newBlocksize, nit->second->blocks, sizeof(uint64_t) * (1 + (nit->second->size / pbh->block_size)));
				free(nit->second->blocks);
				nit->second->blocks = newBlocksize;
				
				for(i = 1 + (nit->second->size / pbh->block_size); i < 4 + ((size+offset) / pbh->block_size); i++) {
					rbit = blockMap.rbegin();
					pb = (PBLOCK) malloc(sizeof(BLOCK));
					pb->data = (char*) malloc(sizeof(char) * pbh->block_size);
					nit->second->blocks[i] = rbit->first + 1;
					blockMap.insert(make_pair(1 + rbit->first, pb));
				}
				
				nit->second->size = size+offset;
		}
		if(offset != 0) {
			block0 = offset / pbh->block_size;
			byte0 = offset % pbh->block_size;
  	}
	
		start = nit->second->blocks + block0;

  	if(byte0 != 0) {
	  	if((size - currentBytes) > (pbh->block_size - byte0)) {
		  	num = (pbh->block_size - byte0);
	  	} 
			else {
			  num = (size - currentBytes);
		  }
		  memcpy(blockMap[*start]->data+byte0, data + currentBytes, num);
			currentBytes += num;
			i++;
	  }

	  while(currentBytes < size) {
		  if(size-currentBytes > pbh->block_size) {
			  num = pbh->block_size;
		  } 
			else {
			  num = (size - currentBytes);
		  }
		  memcpy(blockMap[*(start + i)]->data, data + currentBytes, num);
		  currentBytes += num;
		  i++;
	  }
	
	return currentBytes;	
}

//////////////////////////////////////////////////////////////////
//Create a file <path>. Create a new file and give it the mode
//given by <mode> OR'd with S_IFREG (regular file). If the name
//given by <path> is too long, return -ENAMETOOLONG. As with
//fs_write, if we're a read only file system
//(fi->flags & O_RDONLY), then return -EROFS.
//Otherwise, return 0 if all succeeds.
//////////////////////////////////////////////////////////////////
//use touch command or mode with ifreg
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	string pth, command;

	debugf("fs_create: %s\n", path);

	if(strlen(path) > NAME_SIZE) return -ENAMETOOLONG;
	if(fi->flags & O_RDONLY) return -EROFS;
		
	nit = nodeMap.find(path);

	if(nit != nodeMap.end()) {
		return -EACCES;
	}
	
	NODE *n;

	n = (NODE *) malloc(sizeof(NODE));
	strcpy(n->name, path);
	n->mode = S_IFREG | mode;
	n->ctime = time(NULL);
	n->atime = time(NULL);
	n->mtime = time(NULL);
	n->uid = getuid();
	n->gid = getgid();
	n->size = 0;
	n->blocks = NULL;
	
	nodeMap.insert(make_pair(path, n));
	pbh->nodes++;	
	
	return 0;
}

//////////////////////////////////////////////////////////////////
//Get the attributes of file <path>. A static structure is passed
//to <s>, so you just have to fill the individual elements of s:
//s->st_mode = node->mode
//s->st_atime = node->atime
//s->st_uid = node->uid
//s->st_gid = node->gid
// ...
//Most of the names match 1-to-1, except the stat structure
//prefixes all fields with an st_*
//Please see stat for more information on the structure. Not
//all fields will be filled by your filesystem.
//////////////////////////////////////////////////////////////////
int fs_getattr(const char *path, struct stat *s)
{
	NODE* n;

	debugf("fs_getattr: %s\n", path);
	
	nit = nodeMap.find(path);
	
	if(nit == nodeMap.end()) {
		//not in map
		return -ENOENT;
	}

	debugf("asdlkajsdkjasd: %s\n", nit->first.c_str());
	n = nit->second;

	//s->st_dev = n->id;
	s->st_nlink = 1;
	s->st_mode = n->mode;
	s->st_ctime = n->ctime;
	s->st_atime = n->atime;
	s->st_mtime = n->mtime;
	s->st_uid = n->uid;
	s->st_gid = n->gid;
	s->st_size = n->size;

	return 0;
}

//////////////////////////////////////////////////////////////////
//Read a directory <path>. This uses the function <filler> to
//write what directories and/or files are presented during an ls
//(list files).
//
//filler(buf, "somefile", 0, 0);
//
//You will see somefile when you do an ls
//(assuming it passes fs_getattr)
//////////////////////////////////////////////////////////////////
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	       off_t offset, struct fuse_file_info *fi)
{
	NODE *n;
	size_t pos;
	string pth, str, sub;
	
	map<string, NODE *>::iterator tmp;
	nit = nodeMap.find(path);

	debugf("fs_readdir: %s\n", path);
	
	if(nit == nodeMap.end()) return -ENOENT;
	
	n = nit->second;
	
	pth = nit->first;

	if((n->mode ^ (S_IFDIR | n->mode)) != 0)
		return -ENOTDIR;
	
	for(nit = nodeMap.begin(); nit != nodeMap.end(); nit++) {
		str = nit->first;

		if(str.size() < pth.size() || str == path)
			continue;

		pos = str.rfind("/");

		if(pth == "/")
			sub = str.substr(0, pos+1);
		else
			sub = str.substr(0, pos);

		if(pth == sub) {
			filler(buf, str.substr(pos+1).c_str(), 0, 0);
		}
		//debugf("%s %s\n", pth.c_str(), str.c_str());
	}
	
	filler(buf, ".", 0, 0);
	filler(buf, "..", 0, 0);
	//You MUST make sure that there is no front slashes in the name (second parameter to filler)
	//Otherwise, this will FAIL.

	return 0;
}

//////////////////////////////////////////////////////////////////
//Open a directory <path>. This is analagous to fs_open in that
//it just checks to see if the directory exists. If it does,
//return 0, otherwise return -ENOENT
//////////////////////////////////////////////////////////////////
int fs_opendir(const char *path, struct fuse_file_info *fi)
{
	debugf("fs_opendir: %s\n", path);
	
	nit = nodeMap.find((char *) path);
	
	if(nit != nodeMap.end())  {
		if((nit->second->mode ^ (S_IFDIR | nit->second->mode)) == 0)
			return 0;
	}

	return -ENOENT;
}

//////////////////////////////////////////////////////////////////
//Change the mode (permissions) of <path> to <mode>
//////////////////////////////////////////////////////////////////
int fs_chmod(const char *path, mode_t mode)
{
	
	nit = nodeMap.find(path);
	
	if(nit == nodeMap.end()) return -ENOENT;
	
	nit->second->mode = mode;
	
	return 0;
}

//////////////////////////////////////////////////////////////////
//Change the ownership of <path> to user id <uid> and group id <gid>
//////////////////////////////////////////////////////////////////
int fs_chown(const char *path, uid_t uid, gid_t gid)
{
	debugf("fs_chown: %s\n", path);
	
	nit = nodeMap.find(path);
	
	if(nit == nodeMap.end()) {
		return -ENOENT;
	}

	nit->second->uid = uid;
	nit->second->gid = gid;
	
	return 0;
}

//////////////////////////////////////////////////////////////////
//Unlink a file <path>. This function should return -EISDIR if a
//directory is given to <path> (do not unlink directories).
//Furthermore, you will not need to check O_RDONLY as this will
//be handled by the operating system.
//Otherwise, delete the file <path> and return 0.
//////////////////////////////////////////////////////////////////

int fs_unlink(const char *path)
{
	debugf("fs_unlink: %s\n", path);
	
	nit = nodeMap.find(path);
	
	if((nit->second->mode ^ (S_IFDIR | nit->second->mode)) == 0) {
		return -EISDIR;
	}
	

	delete nit->second;
	
	nodeMap.erase(nit);
	pbh->nodes--;

	return 0;
}

//////////////////////////////////////////////////////////////////
//Make a directory <path> with the given permissions <mode>. If
//the directory already exists, return -EEXIST. If this function
//succeeds, return 0.
//////////////////////////////////////////////////////////////////
int fs_mkdir(const char *path, mode_t mode)
{
	debugf("fs_mkdir: %s\n", path);
	
	if(strlen(path) > NAME_SIZE) {
		return -ENAMETOOLONG;
	}

	nit = nodeMap.find(path);
	
	if(nit != nodeMap.end()) {
		return -EEXIST;
	}

	NODE *n;

	n = (NODE *) malloc(sizeof(NODE));
	strcpy(n->name, path);

	n->id = pbh->nodes + 1;
	n->mode = S_IFDIR | mode;
	n->ctime = time(NULL);
	n->atime = time(NULL);
	n->mtime = time(NULL);
	n->uid = getuid();
	n->gid = getgid();
	n->size = 0;
	n->blocks = NULL;

	nodeMap.insert(make_pair(path, n));
	pbh->nodes++;

	return 0;
}

//////////////////////////////////////////////////////////////////
//Remove a directory. You have to check to see if it is
//empty first. If it isn't, return -ENOTEMPTY, otherwise
//remove the directory and return 0.
//////////////////////////////////////////////////////////////////
int fs_rmdir(const char *path)
{
	size_t pos;
	string pth, str, sub;

	debugf("fs_rmdir: %s\n", path);
	
	nit = nodeMap.find(path);
	pth = nit->first;
	
	for(nit = nodeMap.begin(); nit != nodeMap.end(); nit++) {
		str = nit->first;

		if(str.size() < pth.size() || str == path)
			continue;
		
		pos = str.rfind("/");

		if(pth == "/")
			sub = str.substr(0, pos+1);
		else
			sub = str.substr(0, pos);

		if(pth == sub)
			return -ENOTEMPTY;
	}
	
	nit = nodeMap.find(path);
	delete nit->second;

	nodeMap.erase(nit);
	pbh->nodes--;

	return 0;
}

//////////////////////////////////////////////////////////////////
//Rename the file given by <path> to <new_name>
//Both <path> and <new_name> contain the full path. If
//the new_name's path doesn't exist return -ENOENT. If
//you were able to rename the node, then return 0.
//////////////////////////////////////////////////////////////////
int fs_rename(const char *path, const char *new_name)
{
	size_t pos;
	string str, pth;

	debugf("fs_rename: %s -> %s\n", path, new_name);
	
	if(strlen(new_name) > NAME_SIZE)
		return -ENAMETOOLONG;

	nit = nodeMap.find(path);
	
	if(nit == nodeMap.end())
		return -EACCES;
	
	pth = new_name;

	pos = pth.rfind("/");

	if(pth == "/")
		pth = pth.substr(0, pos+1);
	else
		pth = pth.substr(0, pos);

	for(nit = nodeMap.begin(); nit != nodeMap.end(); nit++) {
		str = nit->first;
		
		if(pth == str) {
			nit = nodeMap.find(path);
			
			nodeMap.insert(make_pair(new_name, nit->second));
			
			nodeMap.erase(nit);
			
			nit = nodeMap.find(new_name);
			strcpy(nit->second->name, new_name);
			
			return 0;
		}
	}

	return -ENOTEMPTY;
}

//////////////////////////////////////////////////////////////////
//Rename the file given by <path> to <new_name>
//Both <path> and <new_name> contain the full path. If
//the new_name's path doesn't exist return -ENOENT. If
//you were able to rename the node, then return 0.
//////////////////////////////////////////////////////////////////
int fs_truncate(const char *path, off_t size)
{
  nit = nodeMap.find(path);

	if(nit == nodeMap.end()) return -ENOENT;
	if(size > nit->second->size) return -EACCES;

  nit->second->size = size;

	return 0;
}

//////////////////////////////////////////////////////////////////
//fs_destroy is called when the mountpoint is unmounted
//this should save the hard drive back into <filename>
//////////////////////////////////////////////////////////////////
void fs_destroy(void *ptr)
{
	FILE *fp;
	const char *filename = (const char *)ptr;
	
	debugf("fs_destroy: %s\n", filename);
	
	fp = fopen(filename, "w");

	if(fp == NULL) {
		debugf("fs_destroy: not %s\n", filename);
	}

	fwrite(pbh, sizeof(BLOCK_HEADER), 1, fp);

	for(nit = nodeMap.begin(); nit != nodeMap.end(); nit++) {
		debugf("%s\n", nit->first.c_str());
		fwrite(nit->second, ONDISK_NODE_SIZE, 1, fp);
		
		if(nit->second->size > 0) {
			debugf("\tsize>>0\n");
			fwrite(nit->second->blocks, sizeof(uint64_t),
				((nit->second->size / pbh->block_size) + 1), fp);
		}
	}

	for(bit = blockMap.begin(); bit != blockMap.end(); bit++) {
		fwrite(bit->second->data, sizeof(char), pbh->block_size, fp);
	}
	//Save the internal data to the hard drive
	//specified by <filename>
	
}

//////////////////////////////////////////////////////////////////
//int main()
//DO NOT MODIFY THIS FUNCTION
//////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
	fuse_operations *fops;
	char *evars[] = { "./fs", "-f", "mnt", NULL };
	int ret;

	if ((ret = fs_drive(HARD_DRIVE)) != 0) {
		debugf("Error reading hard drive: %s\n", strerror(-ret));
		return ret;
	}
	//FUSE operations
	fops = (struct fuse_operations *) calloc(1, sizeof(struct fuse_operations));
	fops->getattr = fs_getattr;
	fops->readdir = fs_readdir;
	fops->opendir = fs_opendir;
	fops->open = fs_open;
	fops->read = fs_read;
	fops->write = fs_write;
	fops->create = fs_create;
	fops->chmod = fs_chmod;
	fops->chown = fs_chown;
	fops->unlink = fs_unlink;
	fops->mkdir = fs_mkdir;
	fops->rmdir = fs_rmdir;
	fops->rename = fs_rename;
	fops->truncate = fs_truncate;
	fops->destroy = fs_destroy;

	debugf("Press CONTROL-C to quit\n\n");

	return fuse_main(sizeof(evars) / sizeof(evars[0]) - 1, evars, fops,
			 (void *)HARD_DRIVE);
}
