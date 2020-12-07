/*
     File        : file.C

     Author      : Riccardo Bettati
     Modified    : 2017/05/01

     Description : Implementation of simple File class, with support for
                   sequential read/write operations.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define NODE_RANGE 500

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "file.H"

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

File::File() {
    /* We will need some arguments for the constructor, maybe pointer to disk
     block with file management and allocation data. */
    Console::puts("In file constructor.\n");
    file_id = 0;
	file_size = 0;
	cur_dataNode = 0;
	curDataNodes = NULL;
}

/*--------------------------------------------------------------------------*/
/* FILE FUNCTIONS */
/*--------------------------------------------------------------------------*/

int File::Read(unsigned int _n, char * _buf) {
    Console::puts("reading from file\n");
	int num = _n;
    int size = 512;
	memset(buf, 0, size);

	while (num > 0) {
		if (curDataNodes == NULL)
			assert(false);
		FILE_SYSTEM -> disk -> read(curDataNodes[cur_dataNode], buf);
		for (cur_position; !EoF() && (cur_position < NODE_RANGE); cur_position++){
			if(num == 0) {
				break;
			}
            else {
                memcpy(_buf,buf + cur_position + 12,1);
                num--;
                _buf++;
                if (cur_position == NODE_RANGE){
                    cur_position =0 ;
                    cur_dataNode++;
                    break;
                }
            }
		}
	}
	return (_n - num);
}


void File::Write(unsigned int _n, const char * _buf) {
    Console::puts("writing to file\n");
    int size = 512;
    memset(buf, 0, size);
	int num=_n;
	while (NODE_RANGE <= num || curDataNodes == NULL){
		if (EoF()) {
			GetNode();
		}

		memcpy(buf + 12,_buf,NODE_RANGE); 
		FILE_SYSTEM->disk->write(curDataNodes[cur_dataNode],buf);
		num-= NODE_RANGE;
	}
	return;
}

void File::Reset() {
    Console::puts("reset current position in file\n");
    cur_position = 0;
	cur_dataNode = 0;
    
}

void File::Rewrite() {
    Console::puts("erase content of file\n");
    for(int i = 0; i < file_size; i++){
		FILE_SYSTEM -> disk->read(curDataNodes[i],buf);
		dataNode->useState = 0x0000; 
		FILE_SYSTEM -> disk->write(curDataNodes[i],buf);
	}
	file_size = 0;
	curDataNodes = NULL;
	cur_dataNode = 0;
	cur_position = 0;
}


bool File::EoF() {
    Console::puts("testing end-of-file condition\n");
    return ((curDataNodes==NULL) || (cur_position == NODE_RANGE-1 ));
}

void File::GetNode(){
	unsigned int tempNode = FILE_SYSTEM -> getNode();
	unsigned int* newDataNodes= new unsigned int[file_size+1];
	for (unsigned int i = 0; i < file_size; i++)
		newDataNodes[i] = curDataNodes[i];
	if (curDataNodes!=NULL) newDataNodes[file_size] = tempNode;
	else newDataNodes[0] = tempNode;
	file_size++;
	delete curDataNodes;
    curDataNodes = newDataNodes;
}