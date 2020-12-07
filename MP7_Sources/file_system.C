/*
     File        : file_system.C

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Implementation of simple File System class.
                   Has support for numerical file identifiers.
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define FREE 0x0000
#define USED 0xFFFF

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file_system.H"


/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

FileSystem::FileSystem() {
    Console::puts("In file system constructor.\n");
	curDataNode=0;
	numFiles=0;
	files=NULL;
	memset(buf,0, 512); 
    
}

/*--------------------------------------------------------------------------*/
/* FILE SYSTEM FUNCTIONS */
/*--------------------------------------------------------------------------*/

bool FileSystem::Mount(SimpleDisk * _disk) {
    Console::puts("mounting file system form disk\n");
	disk=_disk;
	return true;
}

bool FileSystem::Format(SimpleDisk * _disk, unsigned int _size) {
    Console::puts("formatting disk\n");
	FILE_SYSTEM->setdisk(_disk);
	int blockSize = 512;
	memset(buf,0,BLOCKSIZE);

	size = _size;
	diskBlocks = size / blockSize;
	memset(buf,0, BLOCKSIZE);
	for (int i = 0;i < diskBlocks; i++)
		_disk->write(i,buf);
	return true;
}

File * FileSystem::LookupFile(int _file_id) {
    Console::puts("looking up file\n");
    for (int i = 0; i <= numFiles; i++) {
        if (files[i].file_id  == _file_id) {
            return &files[i];
        }
    }
    return NULL;
}

bool FileSystem::CreateFile(int _file_id) {
    Console::puts("creating file\n");
	File* file=(File*) new File();
	memset(buf, 0, 512);
	file->file_id=_file_id;
	

	File* new_files= new File[numFiles+1];
	if (files==NULL) {
		files = new_files;
		files[0] = *file;
		numFiles++;
	}
	else {
		for (unsigned int i = 0; i < numFiles; i++)
			new_files[i]=files[i];
		new_files[numFiles]=*file;
		numFiles++;
		
		delete files; 
		files=new_files;
	}
	return true;
}

bool FileSystem::DeleteFile(int _file_id) {
    Console::puts("deleting file\n");
    File* newFiles= new File[numFiles];
	int q = 0;
	bool flagBool = false; 
	for (unsigned int i = 0; i < numFiles; i++) {
		if (files[i].file_id == _file_id){
			files[i].Rewrite();
			flagBool = true;
			q = i + 1;
		}
		if(flagBool) {
			q = i + 1;
		}
		else {
			q = i;
		}
		newFiles[i] = files[q];
	}
	if (q >= numFiles)
		numFiles = numFiles-1;
	
	delete files;
	files = newFiles;
	return flagBool;
}

unsigned int FileSystem::getNode(){
	memset(buf, 0, 512);

	disk->read(curDataNode,buf);
	for ( int i = 0; i < (diskBlocks - 1); i++) {
		if(dataNode->useState == FREE)
			break;
		curDataNode++;
		disk -> read(curDataNode, buf);
	}
	disk->read(curDataNode,buf);
	curDataNode++;
	dataNode->useState=USED;
	disk->write(curDataNode,buf);
	return curDataNode;
}
