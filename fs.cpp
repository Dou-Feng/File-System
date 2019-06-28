#include "fs.hpp"
#include <vector>
#include <fstream>
#include <sstream>
// #define dir_debug
// #define syscall_debug
using std::stringstream;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::cout;
using std::cin;
using std::cerr;
using std::endl;

FileSystem::FileSystem() 
{
    initDisk(DISK_BLOCK_NUM * DISK_BLOCK_SIZE);
    file_dir = NULL;
    file_inode = NULL;
    file_p = 0;
    // init read/write buffer
    memset(read_buff, 0, sizeof(read_buff));
    memset(write_buff, 0, sizeof(write_buff));
    strcpy(str_dir, "/");
    #ifdef disk_debug
    char buf[DISK_BLOCK_SIZE];
    for (int i = 0; i < 10; i++) {
        sprintf(buf, "The number is %d", i);
        writeBlock(buf, i);
    }
    for (int i = 9; i >= 0; i--) {
        readBlock(buf, i);
        printf("%s, String len: %lu\n", buf, strlen(buf));
    }
    #endif
    #ifdef inode_debug
    for (int i = 0; i< 20; i++) {
        size_t inode_block_num = createInode();
        printf("inode block num is %lu\n", inode_block_num);
    }

    // test modify inode
    inode src;
    for (int i = 0; i < 20; i++) {
        src.file_id = 100 + i;
        size_t num = 13512 + i;
        printf("Before change : %lu, ", getInode(num)->last_modify_time);
        modifyInode(num, src);
        inode *m_inode = getInode(num);
        printf("m_inode:file_id %d, modify time: %lu\n", m_inode->file_id, m_inode->last_modify_time);
    }

    // test delete
    size_t de_num = 13548;
    printf("Before change : %lu, ", getInode(de_num)->last_modify_time);
    deleteInode(de_num);
    inode *de_p = getInode(de_num);
    printf("m_inode:file_id %d, modify time: %lu\n", de_p->file_id, de_p->last_modify_time);
    de_num = 13541;
    printf("Before change : %lu, ", getInode(de_num)->last_modify_time);
    deleteInode(de_num);
    de_p = getInode(de_num);
    printf("m_inode:file_id %d, modify time: %lu\n", de_p->file_id, de_p->last_modify_time);
    #endif
    #ifdef dir_debug

    addDir("/root");
    addDir("/root/1");
    addDir("root/2");
    addDir("/root/1/2");
    addDir("/root/1/2/3");
    addDir("/root/1/2/3/4.txt");
    if (getDir("/root/1/2/3/4.txt")) {
        cout << "Get Success " << endl;
    }
    lookFiles();
    changeDir("/root");
    lookFiles();
    changeDir("/");
    lookFiles();
    #endif
    #ifdef syscall_debug
    lookFiles();
    removeFile("1.txt");
    lookFiles();
    openFile("/1.txt");
    writeFile("Hello, world! I am coming.", 26);
    closeFile();
    copyFile("/1.txt", "2.txt");
    // writeFile("1234567890 Happy birthday to you!", 30);
    string test_buff = "Hello, world";
    openFile("/1.txt");
    readFile(test_buff, 26);
    cout << file_dir->data.filename <<": " << test_buff << endl;
    openFile("/2.txt");
    readFile(test_buff, 26);
    cout << file_dir->data.filename <<": " << test_buff << endl;
    closeFile();
    linkFile("/2.txt", "3.txt");
    openFile("/3.txt");
    readFile(test_buff, 26);
    cout << file_dir->data.filename <<": " << test_buff << endl;
    closeFile();
    #endif
}

FileSystem::~FileSystem() 
{
    // save inode buffer into disk
    if (inode_buf.buffer_num != 0) {
        writeBlock((char*)inode_buf.data, inode_buf.block_num, sizeof(inode) * TIMES_OF_D_AND_M);
        modifyInodeChain();
    }
    
    // write dir into disk
    writeDirintoDisk();

    // save chain into disk
    saveFileChain();
    saveInodeChain();
    fclose(disk);
}

// 分析shell中的输入
SHELL_OP FileSystem::analyse(char *input)
{
    memset(cmd, 0, sizeof(cmd));
    sscanf(input, "%s %s %s", cmd[0], cmd[1], cmd[2]);
    if (strcmp(cmd[0], "ls") == 0) {
        return LS;
    } else if (strcmp(cmd[0], "cd") == 0) {
        return CD;
    } else if (strcmp(cmd[0], "mkdir") == 0) {
        return MKDIR;
    } else if (strcmp(cmd[0], "open") == 0) {
        return OPEN;
    } else if (strcmp(cmd[0], "read") == 0) {
        return READ;
    } else if (strcmp(cmd[0], "write") == 0) {
        return WRITE;
    } else if (strcmp(cmd[0], "rm") == 0) {
        return RM;
    } else if (strcmp(cmd[0], "cp") == 0) {
        return CP;
    } else if (strcmp(cmd[0], "ln") == 0) {
        return LK;
    } else if (strcmp(cmd[0], "quit") == 0) {
        return QUIT;
    } else if (strcmp(cmd[0], "close") == 0) {
        return CLOSE;
    }
    return (SHELL_OP) 100;
}

// 创建一个能够交互的UI界面
/**
运行文件系统，处理用户命令
*/
void FileSystem::shell()
{
    bool isQuit = 0;
    char input[128];
    do
    {
        char buffer[3*DISK_BLOCK_SIZE];
        size_t len;
        printf("walker@walker's MBP:%s#", str_dir);
        fgets(input, 128, stdin);
		if(input[0]=='\n') 
            continue;
        switch(analyse(input))
        {
            case LS:
                if (strlen(cmd[1]) != 0) {
                    lookFiles(cmd[1]); // 查看指定目录文件
                } else {
                    lookFiles();  // 查看当前目录的文件
                }
                break;
            case CD:
                if (strlen(cmd[1]) == 0) {
                    cout << "SHELL::CMD INPUT ERROR" << endl;
                }
                changeDir(cmd[1]);
                break;
            case MKDIR:
                if (strlen(cmd[1]) == 0) {
                    cout << "SHELL::CMD INPUT ERROR" << endl;
                }
                openFile(cmd[1], DIR_FILE);
                break;
            case OPEN:
                if (strlen(cmd[1]) == 0) {
                    cout << "SHELL::CMD INPUT ERROR" << endl;
                }
                openFile(cmd[1], INDEX_FILE);
            break; 
            case WRITE:
                if (strlen(cmd[1]) == 0 ) {
                    cout << "SHELL::CMD INPUT ERROR" << endl;
                }
                len = strlen(cmd[1]);
                writeFile(cmd[1], len);
                break;
            case READ:
                memset(buffer, 0, sizeof(buffer));
                if (strlen(cmd[1]) == 0) {
                    readFile(buffer, DISK_BLOCK_SIZE);
                } else {
                    sscanf(cmd[1], "%lu", &len);
                    readFile(buffer, len);
                }
                cout << buffer << endl;
                break;
            case RM:
                if (strlen(cmd[1]) == 0) {
                    cout << "SHELL::CMD INPUT ERROR" << endl;
                }
                removeFile(cmd[1]);
                break;
            case CP:
                if (strlen(cmd[1]) == 0 || strlen(cmd[2]) == 0) {
                    cout << "SHELL::CMD INPUT ERROR" << endl;
                }
                copyFile(cmd[1], cmd[2]);
                break;
            case LK:
                if (strlen(cmd[1]) == 0 || strlen(cmd[2]) == 0) {
                    cout << "SHELL::CMD INPUT ERROR" << endl;
                }
                linkFile(cmd[1], cmd[2]);
                break;
            case QUIT:
                isQuit = 1;
            break;
            case CLOSE:
                closeFile();
            break;
            default:
                printf("SHELL::CMD INPUT ERROR\n");
                break;
        }
    }while(!isQuit);
}

// 打开一个文件，这里主要是得到file_dir和file_inode
int FileSystem::openFile(const string &f_path, const FILE_DIR_TYPE type) 
{
    file_dir = getDir(f_path);
    if (!file_dir) {
        // create a new file
        file_dir = addDir(f_path, type);
        if (!file_dir) {
            cout << "OPENFILE::INVALID PATH" << endl;
            return -1;
        }
        if (type == DIR_FILE) {
            cout << "OPENFILE::CREATE A NEW DIR" << endl;
        } else {
            cout << "OPENFILE::CREATE A NEW FILE" << endl;
        }
        // 名字和inode在addDir时就已经赋值了
        file_inode = getInode(file_dir->data.inode);
    } else if (file_dir->data.dir_type != DIR_FILE) {
        // 如果不是目录
        file_inode = getInode(file_dir->data.inode);
    } else { // 不能打开一个目录
        cout << "OPENFILE::CANT OPEN A DIR" << endl;
        file_dir = NULL;
        return -1;
    }
    file_p = 0;
    return 0;
}

// 关闭文件, 把file_dir 和 file_inode 设置为 null
// 清空缓冲区
int FileSystem::closeFile()
{
    file_dir = NULL;
    file_inode = NULL;
    file_p = 0;
    memset(write_buff, 0, sizeof(write_buff));
    memset(read_buff, 0, sizeof(read_buff));
    return 0;
}

// 读取文件，用file_p来表示读取的位置
int FileSystem::readFile(char *targ, const size_t size)
{
    if (!file_dir) {
        cout << "READFILE::FILE NOT OPEN\n" << endl;
        return -1;
    }
    if (file_p >= file_inode->file_size) {
        cout << "READFILE::BEYOND FILE SIZE" << endl;
        return -1;
    }
    int addr_index = file_p / DISK_BLOCK_SIZE;
    int offset = file_p % DISK_BLOCK_SIZE;

    // 如果是一级索引表中的块
    if (addr_index >= I_ADDR_DIRECT_INDEX_NUM 
        && addr_index < I_ADDR_DIRECT_INDEX_NUM + PRI_INDEX_TABLE_SIZE * 2) {
            // 现在不实现
    } else if (addr_index > I_ADDR_DIRECT_INDEX_NUM + PRI_INDEX_TABLE_SIZE * 2) {
        // 二级索引表中，现在不实现
    } else {
        // 从磁盘中得到对应的块
        memset(read_buff, 0, sizeof(read_buff));
        readBlock((char*)read_buff, file_inode->i_addr[addr_index]);
        // cout << read_buff << endl;
        // 如果不用跨越两个磁盘块
        if (offset + size <= DISK_BLOCK_SIZE) {
            if (offset + size <= file_inode->file_size)
                memcpy(targ, read_buff+offset, size);
            else 
                memcpy(targ, read_buff+offset, file_inode->file_size - file_p);
            strcat(targ, "\0\0");
            file_p = (size+file_p > file_inode->file_size)?file_inode->file_size:(size+file_p);
        } else {
            // 如果下一个索引表项也是直接索引表项
            if (addr_index+1 < I_ADDR_DIRECT_INDEX_NUM) {
                // 暂时不考虑
            } else {
                // 如果下一个索引表不再直接索引表项
                // 暂时不考虑
            }
        }
    }
    return 0;
}

// 写入文件，追加写入,不考虑其他写入模式（覆盖，插入）
int FileSystem::writeFile(const string buf, const size_t size)
{
    if (!file_dir) {
        cout << "WRITEFILE::FILE NOT OPEN" << endl;
        return -1;
    }

    // 根据文件大小可以分为三种情况
    // 1. 直接索引块就可以存放下所有内容
    // 2. 要用到一级索引才可以存放下所有内容
    // 3. 要用二级索引
    // 初步模型中不考虑2、3两种情况
    size_t file_size = file_inode->file_size;
    int addr_index = file_size / DISK_BLOCK_SIZE;
    int offset = file_size % DISK_BLOCK_SIZE;  // 磁盘块内偏移
    if (addr_index >= I_ADDR_DIRECT_INDEX_NUM 
        && addr_index < I_ADDR_DIRECT_INDEX_NUM + PRI_INDEX_TABLE_SIZE * 2) {
        // 如果addr_index 大于8并且小于2*256+9，那么block_num将会变成一级索引表所在的磁盘块
        // 一级索引表号
        int primary_table_index = (addr_index-I_ADDR_DIRECT_INDEX_NUM) / DISK_BLOCK_SIZE + I_ADDR_DIRECT_INDEX_NUM; 
        // 一级索引表内偏移
        int offset2 = (addr_index-I_ADDR_DIRECT_INDEX_NUM) % DISK_BLOCK_SIZE;
        int primary_table[PRI_INDEX_TABLE_SIZE];
        readBlock((char*)primary_table, file_inode->i_addr[primary_table_index]);
        int primary_block_num = primary_table[offset2];
        // 一级索引需要读取两次，把实际数据读入到read_buff中
        readBlock((char*)write_buff, primary_block_num);
    } else if (addr_index > I_ADDR_DIRECT_INDEX_NUM + PRI_INDEX_TABLE_SIZE * 2) {
        // 如果是二级索引，需要读三次磁盘
        // 二级索引表的磁盘块
        int sec_index_block_num = file_inode->i_addr[I_ADDR_DIRECT_INDEX_NUM+I_ADDR_PRIMARY_INDEX_NUM];
        // 二级索引表
        int sec_table_1[PRI_INDEX_TABLE_SIZE], sec_table_2[PRI_INDEX_TABLE_SIZE];
        // 读出一级索引表
        readBlock((char*)sec_table_1, sec_index_block_num);
        // 一级索引表的中的偏移
        int sec_table_index_1 = (addr_index - I_ADDR_DIRECT_INDEX_NUM - 2 * PRI_INDEX_TABLE_SIZE) 
                                / DISK_BLOCK_SIZE; 
        // 二级索引表中的偏移
        int sec_table_index_2 = (addr_index - I_ADDR_DIRECT_INDEX_NUM - 2 * PRI_INDEX_TABLE_SIZE) 
                                % DISK_BLOCK_SIZE; 
        // 读出二级索引表
        readBlock((char *)sec_table_2, sec_table_index_1);
        // 读出数据块
        readBlock((char*)write_buff, sec_table_index_2);

    } else { // 实现最简单的，直接索引方式来存储文件
        // 如果得到的文件磁盘块号为0，说明还未初始化
        if (file_inode->i_addr[addr_index] == 0) {
            file_inode->i_addr[addr_index] = getWriteableBlock(FILE_BLOCK);
            modifyFileChain(CHAIN_OP_REMOVE, file_inode->i_addr[addr_index]);
        }
        // 如果是直接索引，只用读一次
        readBlock((char*)write_buff, file_inode->i_addr[addr_index]);
        // 如果当前buff可以存放下所有内容
        int new_offset = size + offset;
        file_inode->file_size += size; // 修改文件大小
        if (new_offset <= DISK_BLOCK_SIZE) {
            memcpy(write_buff+offset, buf.data(), size);  // 追加内容到buffer中
            writeBlock((char*)write_buff, file_inode->i_addr[addr_index]);
        } else {
            // 如果当前buff不能存放下所有的内容
            int diff_size = DISK_BLOCK_SIZE - offset;
            
            // cout << "String:\n" << buf << endl;
            // cout << write_buff << endl;
            memcpy(write_buff+offset, buf.data(), diff_size);
            // cout << write_buff << endl;
            // 如果还存在直接索引表
            if (addr_index+1 < I_ADDR_DIRECT_INDEX_NUM) {
                file_inode->i_addr[addr_index+1] = getWriteableBlock(FILE_BLOCK);
                modifyFileChain(CHAIN_OP_REMOVE, file_inode->i_addr[addr_index+1]);
                // 先把之前写入的内容存到磁盘中
                writeBlock((char*)write_buff, file_inode->i_addr[addr_index]);
                // 然后把新的内容磁盘块
                memset(write_buff, 0, sizeof(write_buff));
                memcpy(write_buff, buf.data()+diff_size, new_offset - DISK_BLOCK_SIZE);
                // cout << write_buff << endl;
                writeBlock((char*)write_buff, file_inode->i_addr[addr_index+1]);
            } else { // 需要申请一级索引块
                // 暂时不考虑
            }
        }
    }
    return 0;
}

/* 文件系统操作 */
// 复制文件
int FileSystem::copyFile(const string &src, const string &targ)
{   
    // 如果存在目标文件，先删除这个文件
    if (removeFile(targ) == -2) {
        cout << "COPYFILE::TARGET PATH NAME IS A DIR" << endl;
        return -1;
    }
    if (openFile(targ) == -1) {
        cout << "COPYFILE::TARGET PATH ERROR" << endl;
        return -1;
    }
    // openfile， 初始化了file_dir 和 file_inode、file_p三个变量
    dir src_dir = getDir(src);
    if (!src_dir) {
        cout << "COPYFILE::GET SROUCE FILE FAIL" << endl;
        return -1;
    }
    if (src == targ) {
        cout << "COPYFILE::NO NEED TO COPY" << endl;
        return -1;
    }
    if (src_dir->data.dir_type == DIR_FILE) {
        cout << "COPYFILE::CANT COPY A DIR" << endl;
        return -1;
    }
    inode *src_inode = getInode(src_dir->data.inode);

    // 这里拷贝的意思是并不共享inode，所以对于targ的inode的磁盘索引表不应该和src的inode索引表一致
    // 复制要改变的点都在inode当中
    // 复制的意识是删除原来文件中的数据
    
    // 首先改变inode当中的addr
    int src_addr_size = src_inode->file_size / DISK_BLOCK_SIZE;
    inode new_inode;
    memset(&new_inode, 0, sizeof(inode));
    for (int i = 0; i <= src_addr_size; i++) {
        int block_num = getWriteableBlock(FILE_BLOCK);
        new_inode.i_addr[i] = block_num;
        modifyFileChain(CHAIN_OP_REMOVE, block_num);
        // 读出源文件的磁盘块数据
        readBlock((char*)write_buff, src_inode->i_addr[i]);
        // 写入到刚刚申请到的磁盘块中
        writeBlock((char*)write_buff, block_num);
    }
    new_inode.file_id = src_inode->file_id;
    new_inode.file_size = src_inode->file_size;
    new_inode.i_link = 1;
    modifyInode(file_dir->data.inode, new_inode);
    closeFile();
    return 0;
}
// 删除文件
int FileSystem::removeFile(const string &f_path)
{
    dir targ_dir = getDir(f_path);
    if (!targ_dir) {
        cout << "REMOVEFILE::INVALID FILE PATH" << endl;
        return -1;
    }
    size_t inode_num = targ_dir->data.inode;
    inode *targ_inode = getInode(inode_num);
    // 删除目录文件，如果是目录，是不被允许的
    if (deleteDir(f_path) == -1) {
        return -2;
    }
    int addr_size = targ_inode->file_size / DISK_BLOCK_SIZE;
    // 如果文件仅仅充满了直接索引表项
    if (addr_size < I_ADDR_DIRECT_INDEX_NUM) {
        // 释放磁盘空间，把空闲块加入到链表中
        for (int i = addr_size; i >= 0; i--) {
            modifyFileChain(CHAIN_OP_ADD, targ_inode->i_addr[i]);
        }
        // 释放Inode的空间
        deleteInode(inode_num);
    } else {
        // 如果涉及到1、2级索引结构，暂时不实现
    }
    return 0;
}

// 创建一个硬链接，两个文件的inode编号相同
int FileSystem::linkFile(const string &src, const string &targ) {
    if (getDir(targ))  {
        cout << "LINKFILE::TARGET FILE NAME IS ALREADY USED" << endl;
        return -1;
    }
    openFile(targ);
    // 初始化了file_dir, file_inode, file_p
    dir src_dir = getDir(src);
    if (!src_dir) {
        cout << "LINKFILE::SROUCE FILE PATH IS INVALID" << endl;
        return -1;
    }
    // 链接不同于复制的地方在于，链接不需要重新分配inode，只用把dir的inode指向src的inode就行

    // 删除原先的inode，把src的inode编号分配给target
    deleteInode(file_dir->data.inode);
    file_dir->data.inode = src_dir->data.inode;
    inode *i_p = getInode(file_dir->data.inode);
    ++i_p->i_link; // 链接数加一
    // cout << "Link number: " << i_p->i_link<< ", Inode block number: "<< file_dir->data.inode << endl;
    return 0;
}

// 改变当前目录
void FileSystem::changeDir(const string &path)
{
    dir dp = getDir(path);
    if (!dp) {
        cout << "CHANGEDIR::INVALID PATH" << endl;
        return;
    }
    if (dp->data.dir_type != DIR_FILE) {
        cout << "CHANGEDIR::NOT A DIR" << endl;
        return ;
    }
    // 改变路径
    if (path[0] == '/') {
        strcpy(str_dir, path.data());
    } else {
        if (strlen(str_dir) > 0 && str_dir[strlen(str_dir)-1] == '/') {
            strcat(str_dir, path.data());
        } else {
            strcat(str_dir, "/");
            strcat(str_dir, path.data());
        }
    }
    if (path.find_last_of('/') != path.size()-1) {
        strcat(str_dir, "/");
    }
    dir_curr = dp;
}

// 查看当前目录下的所有文件
void FileSystem::lookFiles()
{
    if (!dir_curr) {
        cout << "LookFiles::Dir_curr Error!" << endl;
        exit(-1);
    }
    dir dp = dir_curr->childDir;
    while (dp) {
        cout << dp->data.filename << " ";
        dp = dp->nextDir;
    }
    cout << endl;
}

// 查看指定目录下的所有文件
void FileSystem::lookFiles(const string &path)
{
    dir dp = getDir(path);
    if (!dp) {
        cout << "LookFiles2::PATH INVALID!" << endl;
        return;
    }
    dp = dp->childDir;
    while (dp) {
        cout << dp->data.filename << " ";
        dp = dp->nextDir;
    }
    cout << endl;
}


// private methods
// 初始化一块固定大小的文件作为磁盘
void FileSystem::initDisk(const size_t d_size)
{
    disk = fopen("virtualDisk", "r+");
    if (disk == NULL) {
        disk = fopen("virtualDisk", "w+");
        if (disk == NULL) {
            cerr << "Create disk file failed!\n" << endl;
            exit(0);
        }
    }
    fpos_t end_pos = 0;
    fseek(disk, 0L, SEEK_END);
    fgetpos(disk, &end_pos);
    rewind(disk);
    // printf("%d\n", (int)end_pos);
    if (end_pos == 0) { // if file is empty, full file with 0
        char null = '\0';
        for (int i = 0; i < d_size; i++) {
            fputc(null, disk);
        }
        // init file free block
        file_block_head = (block) malloc(sizeof(freeBlock));
        file_block_head->front_num = FRONT_DATA_BLOCK_NUM;
        file_block_head->length = DISK_BLOCK_NUM - FRONT_DATA_BLOCK_NUM;
        file_block_head->next = (block) malloc(sizeof(freeBlock));
        file_block_tail = file_block_head->next;
        file_block_tail->front_num = file_block_head->front_num;
        file_block_tail->length = file_block_head->length;
        file_block_tail->next = NULL;

        // init inode free block
        inode_block_head = (block) malloc(sizeof(freeBlock));
        inode_block_head->front_num = FRONT_INODE_BLOCK_NUM;
        inode_block_head->length = INODE_BLOCK_NUM;
        inode_block_head->next = (block) malloc(sizeof(freeBlock));
        inode_block_tail = inode_block_head->next;
        inode_block_tail->front_num = inode_block_head->front_num;
        inode_block_tail->length = inode_block_head->length;
        inode_block_tail->next = NULL;

        // init inode buffer
        resetInodeBuffer();

        // init file system root dir
        dir_root = (dir) malloc(sizeof(directory));
        strcpy(dir_root->data.filename, "/");
        // specify a inode to dir_root
        dir_root->data.inode = createInode();
        dir_root->data.dir_type = DIR_FILE;
        dir_root->nextDir = NULL;
        dir_root->childDir = NULL;
        dir_curr = dir_root;

        
    } else { 
        // read initial data from disk
        // init free block chain
        readFileChain();
        readInodeChain();

        // init dir
        readDirFromDisk();
        
        // init inode buffer
        resetInodeBuffer();
    }

}
// 找到一个可写的磁盘块
int FileSystem::getWriteableBlock(BLOCK_TYPE type) 
{
    if (type == FILE_BLOCK) {
        if (file_block_head->length == 0)
            return -1;
        else 
            return file_block_tail->length + file_block_tail->front_num - 1;
    } else if (type == INODE_BLOCK) {
        if (inode_block_head->next && inode_block_head->next->length == 0) {
            printf("GetWritebaleBlock::Disk is full!\n");
            exit(0);
        } else 
            return inode_block_tail->length + inode_block_tail->front_num - 1;
    }
    return -1;
}


// 读取一个磁盘块
void FileSystem::readBlock(char *const targ, const size_t block_num)
{
    FILE *fp = disk;
    fseek(fp, block_num * DISK_BLOCK_SIZE, SEEK_SET);
    // size_t len = (DISK_BLOCK_SIZE > sizeof(targ))? : DISK_BLOCK_NUM;
    if (fread(targ, DISK_BLOCK_SIZE, 1, fp) > 1) {
        printf("fread error!\n");
        exit(1);
    }
}

// 从磁盘块读取固定大小内容
void FileSystem::readBlock(char *const targ, const size_t block_num, const size_t len)
{
    FILE *fp = disk;
    fseek(fp, block_num * DISK_BLOCK_SIZE, SEEK_SET);
    // size_t len = (DISK_BLOCK_SIZE > sizeof(targ))? : DISK_BLOCK_NUM;
    if (fread(targ, len, 1, fp) > 1) {
        printf("fread error!\n");
        exit(1);
    }
}


// 读取连续的几个磁盘块
void FileSystem::readBlock(char *const targ, const size_t block_num , const int num)
{
    FILE *fp = disk;
    fseek(fp, block_num * DISK_BLOCK_SIZE, SEEK_SET);
    if(fread(targ, DISK_BLOCK_SIZE, num, fp) > 1) {
        printf("fread error!\n");
        exit(1);
    }
}


// 写入信息到一个磁盘块
void FileSystem::writeBlock(const char *src, const size_t block_num)
{
    FILE *fp = disk;
    fseek(fp, block_num * DISK_BLOCK_SIZE, SEEK_SET);
    if (fwrite(src, strlen(src), 1, fp) > 1) {
        printf("fwrite error!\n");
        exit(1);
    }
}

// 写入固定大小的信息到磁盘块
void FileSystem::writeBlock(const char *src, const size_t block_num, const size_t len)
{
    FILE *fp = disk;
    fseek(fp, block_num * DISK_BLOCK_SIZE, SEEK_SET);
    if (fwrite(src, len, 1, fp) > 1) {
        printf("fwrite error!\n");
        exit(1);
    }
}

// 写入连续的几个磁盘块
void FileSystem::writeBlock(const char *src, const size_t block_num, const int num)
{
    FILE *fp = disk;
    fseek(fp, block_num * DISK_BLOCK_SIZE, SEEK_SET);
    if (fwrite(src, strlen(src), num, fp) > 1) {
        printf("fwrite error!\n");
        exit(1);
    }
}

// 修改file free chain, need debug
void FileSystem::modifyFileChain(const CHAIN_OP op, const size_t block_num)
{
    block target_block = file_block_tail;  
    block after_target_block = target_block->next;
    int notFound = 1;
    if (op == CHAIN_OP_REMOVE) {
        block bp = file_block_tail;
        if (block_num >= file_block_tail->front_num 
            && block_num < file_block_tail->front_num + file_block_tail->length) {
                // 如果是最后一个结点就不用去遍历链表
                notFound = 0;
        } else {
            block p = file_block_head->next;
            while (p) {
                if (block_num >= p->front_num && block_num < p->front_num + p->length) {
                    target_block = p;
                    after_target_block = p->next;
                    notFound = 0;
                    break;
                }
                p = p->next;
            }
        }
        if (notFound) {
            cout << "MODIFY_FILE_CHAIN::Wrong Block_Num" << endl;
            return ;
        }
        // 如果是file链表结点中的最后一个磁盘块，这样只用修改长度就可以
        if (block_num == target_block->length + target_block->front_num - 1) {
            // 如果最后长度为0，删去这个链表结点
            if (--(target_block->length) == 0) { 
                block p = file_block_head->next;
                while (p->next != target_block) p = p->next; // move p 
                target_block = p;
                free(p->next);
                target_block->next = after_target_block;
                // 以防inode_block_tail被free掉
                file_block_tail = target_block; 
            }
        } else {
            // 如果不是在链表结点的最后一个磁盘块，那么就需要把链表结点一分为二
            int front_length = block_num - target_block->front_num;
            block new_block = (block) malloc(sizeof(freeBlock));
            new_block->front_num = block_num + 1;
            // 一分为二，第一个部分的长度是front_length
            // 第二部分的长度是 all-length - front_length - the target part(1)
            new_block->length = target_block->length - front_length - 1; 
            
            // 改变长度
            target_block->length = front_length;

            // 加入新的结点到链表中
            target_block->next = new_block;
            new_block->next = after_target_block;
        }
        
    } else if (op == CHAIN_OP_ADD) { // 添加一个磁盘块到结点中
        if (block_num == file_block_tail->front_num + file_block_tail->length) {
            file_block_tail->length++;
        } else { // 如果不是就在尾节点后添加一个新的结点
            file_block_tail->next = (block) malloc(sizeof(freeBlock));
            file_block_tail = file_block_tail->next;
            file_block_tail->front_num = block_num;
            file_block_tail->length = 1;
            file_block_tail->next = NULL;
        }
    }
}

void FileSystem::resetInodeBuffer() {
    // reset inode buffer
    memset(inode_buf.data, 0, sizeof(inode) * TIMES_OF_D_AND_M);
    inode_buf.block_num =  -1;
    inode_buf.buffer_num = 0;
    memset(inode_buf.isWriteable, true, sizeof(bool) * TIMES_OF_D_AND_M);
}

// 当进行inode磁盘写入的时候调用此函数，来更新链表
void FileSystem::modifyInodeChain() {
    if (inode_block_head->next == NULL) {
        printf("ERROR::ModifyInodeChain::Disk is full\n");
        exit(0); // no free block anymore
    } 
    bool cantFound = 1;
    block target_block = inode_block_tail;  
    block after_target_block = target_block->next;
    // change the data of chain using to manager free inode blocks
    // if write block is in the last block of chain. 
    if (inode_buf.block_num  >= inode_block_tail->front_num && 
        inode_buf.block_num < inode_block_tail->length + inode_block_tail->front_num) { 
            // It is awesome.
            cantFound = 0;
    } else {
        // find the target block
        block p = inode_block_head->next;
        while (p) {
            if (inode_buf.block_num >= p->front_num && 
                inode_buf.block_num < p->length + p->front_num) {
                target_block = p;
                after_target_block = p->next;
                cantFound = 0;
                break;
            } 
            p = p->next;
        }
    }
    if (cantFound) {
        printf("TIP::MODIFY_INODE_CHAIN::BUFFER IS FULL AND WRITED\n");
        if (inode_buf.buffer_num == TIMES_OF_D_AND_M) {
            // 如果存满了，那么不用修改链表
        } else {
            /*如果没存满，说明这个磁盘块有空位。但是之前的链表数据显示，这个磁盘块以前是存满的状态
             所以我们需要把这个磁盘块再次加入链表当中 */
            // 如果是在尾结点的最后一个磁盘块上
            if (inode_buf.block_num == inode_block_tail->front_num + inode_block_tail->length) {
                inode_block_tail->length++; 
            } else { // 如果不是，那么简化为直接在链表尾部添加一个结点
                inode_block_tail->next = (block) malloc(sizeof(freeBlock));
                inode_block_tail = inode_block_tail->next;
                inode_block_tail->front_num = inode_buf.block_num;
                inode_block_tail->length = 1;
                inode_block_tail->next = NULL;
            }
        }
        return ;
    }
    if (inode_buf.buffer_num != TIMES_OF_D_AND_M) {
        #ifdef inode_debug
        printf("TIP::MODIFY_INODE_CHAIN:: BUFFER NOT FULL\n");
        #endif
        return ;
    }
    // 如果是inode链表结点中的最后一个磁盘块，这样只用修改长度就可以
    if (inode_buf.block_num == target_block->length + target_block->front_num - 1) {
        // 如果最后长度为0，删去这个链表结点
        if (--(target_block->length) == 0) { 
                block p = inode_block_head->next;
                while (p->next != target_block) p = p->next; // move p 
                target_block = p;
                free(p->next);
                target_block->next = after_target_block;
        }
    } else {
        // 如果不是在链表结点的最后一个磁盘块，那么就需要把链表结点一分为二
        int front_length = inode_buf.block_num - target_block->front_num;
        block new_block = (block) malloc(sizeof(freeBlock));
        new_block->front_num = inode_buf.block_num + 1;
        // split it into 2. So the first part is front_length, 
        // and the second part is all-length - front_length - the target part(1)
        new_block->length = target_block->length - front_length - 1; 
        
        // change the target length
        target_block->length = front_length;

        // add a node into chain
        target_block->next = new_block;
        new_block->next = after_target_block;
    }
    // incase the tail block is deleted
    if (target_block->next == NULL) inode_block_tail = target_block; 
    resetInodeBuffer();
}

int FileSystem::fillInodeBufferFromDisk(const size_t num) {
    // read inode data from disk
    readBlock((char*)inode_buf.data, num, sizeof(inode) * TIMES_OF_D_AND_M);
    inode_buf.block_num = num;
    inode_buf.buffer_num = 0;
    int least_writeable_num = -1;
    // init inode buffer
    for (int i = 0; i < TIMES_OF_D_AND_M; i++) {
        if (inode_buf.data[i].create_time != 0) {
            inode_buf.isWriteable[i] = false;
            inode_buf.buffer_num++;
        } else {
            if (least_writeable_num == -1)
                least_writeable_num = i;
        }
    }
    if (least_writeable_num == -1) {
        printf("TIP::FILL INODE::BUFFER IS FULL\n");
    }
    return least_writeable_num;
}


// 从磁盘中得到一个inode
inode *FileSystem::getInode(size_t block_num)
{
    int r_block_num = block_num / TIMES_OF_D_AND_M;
    int block_offset = block_num % TIMES_OF_D_AND_M;

    // if inode wanted is in the buffer. It's awesome
    if (r_block_num == inode_buf.block_num) {
        return &(inode_buf.data[block_offset]);
    } else {
        // if buffer is not empty 
        if (inode_buf.buffer_num != 0) {
            // save buffer into disk
            writeBlock((char*)inode_buf.data, inode_buf.block_num, sizeof(inode) * TIMES_OF_D_AND_M);
            modifyInodeChain();
        }
        fillInodeBufferFromDisk(r_block_num);
        return &(inode_buf.data[block_offset]);
    }
}

// 增加一个inode
int FileSystem::createInode()
{
    int least_writeable_num = -1;
    if (inode_buf.buffer_num == TIMES_OF_D_AND_M) { // buffer is full
        // write buffer into disk
        writeBlock((char *)inode_buf.data, inode_buf.block_num, sizeof(inode) * TIMES_OF_D_AND_M);
        // if buffer is full, the inode free chain will be upadted
        modifyInodeChain();
        // get a writeable block
        size_t new_block_num = getWriteableBlock(INODE_BLOCK);
        inode_buf.block_num = new_block_num;
        // read a disk block to fill inode buffer, and get least writeable index of buffer
        least_writeable_num = fillInodeBufferFromDisk(new_block_num);
    } else { // 如果inode buffer中还有空位，那么直接创建就行，不需要去修改空闲块链表
        if (inode_buf.block_num == -1) {
                size_t block_num = getWriteableBlock(INODE_BLOCK);
                least_writeable_num = fillInodeBufferFromDisk(block_num);
        } else {
            
            // init inode buffer
            for (int i = 0; i < TIMES_OF_D_AND_M; i++) {
                if (inode_buf.isWriteable[i] == true) {
                        least_writeable_num = i;
                        break;
                }
            }
            if (least_writeable_num == -1) {
                printf("Create Inode::Buffer not full::Get Location Failed!\n");
                exit(0);
            }
        }
    }
    inode_buf.buffer_num++;
    inode_buf.isWriteable[least_writeable_num] = false;
    inode_buf.data[least_writeable_num].file_size = 0;
    inode_buf.data[least_writeable_num].create_time = time(NULL);
    inode_buf.data[least_writeable_num].last_modify_time = 
                        inode_buf.data[least_writeable_num].create_time;
    inode_buf.data[least_writeable_num].i_link = 1;
    // 因为每个磁盘块包含16个inode，所以我们乘上16加上偏移保证每个inode的编号唯一
    return inode_buf.block_num * TIMES_OF_D_AND_M + least_writeable_num;
}

// 修改一个inode
int FileSystem::modifyInode(size_t block_num, const inode &s_inode)
{
    inode *target_inode = getInode(block_num);
    target_inode->file_id = s_inode.file_id;
    target_inode->file_size = s_inode.file_size;
    memcpy(target_inode->i_addr, s_inode.i_addr, sizeof(int) * INDEX_TABLE_SIZE);
    target_inode->last_modify_time = time(NULL);
    target_inode->i_link = s_inode.i_link;
    return 0;

}

// 删除一个inode
int FileSystem::deleteInode(size_t block_num)
{
    inode *target = getInode(block_num);
    if (--target->i_link != 0) {
        cout << "DELETEINODE::STILL HAS LINK FILE" << endl;
        return -1;
    }
    if (inode_buf.buffer_num == 0) 
    {
        printf("DELETE_INODE::CAN'T FOUND BLOCK NUM: %lu\n", block_num);
        return -1;
    }
    int i;
    for (i = 0; i < TIMES_OF_D_AND_M; i++) {
        if (target == &(inode_buf.data[i]) && target->create_time != 0) {
            inode_buf.isWriteable[i] = true;
            inode_buf.buffer_num--;
            memset(target, 0, sizeof(inode));
            break;
        }
    }
    if (i == TIMES_OF_D_AND_M) {
        printf("DELETE_INODE::INODE DO NOT EXIST\n");
        return -1;
    }
    return 0;
}

void FileSystem::readInodeChain() 
{
    // init inode free block chain
    freeBlock iblocks[TIMES_OF_D_AND_B]; 
    inode_block_head = (block) malloc(sizeof(freeBlock));
    inode_block_head->front_num = FRONT_INODE_BLOCK_NUM;
    inode_block_head->length = INODE_BLOCK_NUM;
    inode_block_head->next = NULL;
    inode_block_tail = inode_block_head;
    int intrp = 0;
    for (int i = FREE_BLOCK_NUM/2; i < FREE_BLOCK_NUM && !intrp; i++) {
        readBlock((char*)iblocks, i, TIMES_OF_D_AND_B * sizeof(freeBlock));
        for (int j = 0; j < TIMES_OF_D_AND_B; j++) {
            if (iblocks[j].length != 0) {
                inode_block_tail->next = (block) malloc(sizeof(freeBlock));
                inode_block_tail = inode_block_tail->next;
                inode_block_tail->front_num = iblocks[j].front_num;
                inode_block_tail->length = iblocks[j].length;
                inode_block_tail->next = NULL;
            } else {
                intrp = 1; // 
                break;
            }
        }
    }
}

// 把链表分别存放到文件中,inode free chain
void FileSystem::saveInodeChain()
{
    freeBlock iblocks[TIMES_OF_D_AND_B];
    block ip = inode_block_head->next;
    int index = 0;
    int block_num = FREE_BLOCK_NUM / 2;
    while(ip) {
        if (block_num >= FREE_BLOCK_NUM) {
            printf("SAVE_INODE_CHAIN::BEYOND_RANGE\n");
            exit(0);
        }
        iblocks[index].front_num = ip->front_num;
        iblocks[index].length = ip->length;
        iblocks[index].next = ip->next;
        ip = ip->next;
        if (++index == TIMES_OF_D_AND_B) {
            writeBlock((char*)iblocks, block_num++, sizeof(freeBlock) * TIMES_OF_D_AND_B);
            index = 0;
        }
    }
    if (block_num >= FREE_BLOCK_NUM) {
        printf("SAVE_INODE_CHAIN::BEYOND_RANGE\n");
        exit(0);
    }
    memset(iblocks+index, 0, sizeof(freeBlock) * (TIMES_OF_D_AND_B - index));
    writeBlock((char*)iblocks, block_num, sizeof(freeBlock) * TIMES_OF_D_AND_B);
    #ifdef inode_debug
    freeBlock debugBlock[TIMES_OF_D_AND_B];
    readBlock((char*)debugBlock, block_num, sizeof(freeBlock) * TIMES_OF_D_AND_B);
    for (int i = 0; i < TIMES_OF_D_AND_B && debugBlock[i].length; i++) {
        printf("In save inode chain: %d %d\n", debugBlock[i].front_num, debugBlock[i].length);
    }
    #endif

    // free inode chain
    while (inode_block_head) {
        block p = inode_block_head->next;
        free(inode_block_head);
        inode_block_head = p;
    }
}

void FileSystem::readFileChain() 
{
    freeBlock fblocks[TIMES_OF_D_AND_B]; 
    file_block_head = (block) malloc(sizeof(freeBlock));
    file_block_head->front_num = FRONT_DATA_BLOCK_NUM;
    file_block_head->length = DISK_BLOCK_NUM - FRONT_DATA_BLOCK_NUM;
    file_block_head->next = NULL;
    file_block_tail = file_block_head;
    int intrp = 0;
    for (int i = 0; i < FREE_BLOCK_NUM/2 && !intrp; i++) {
        readBlock((char*)fblocks, i, TIMES_OF_D_AND_B * sizeof(freeBlock));
        for (int j = 0; j < TIMES_OF_D_AND_B; j++) {
            if (fblocks[j].length != 0) {
                file_block_tail->next = (block) malloc(sizeof(freeBlock));
                file_block_tail = file_block_tail->next;
                file_block_tail->front_num = fblocks[j].front_num;
                file_block_tail->length = fblocks[j].length;
                file_block_tail->next = NULL;
            } else {
                intrp = 1;
                break;
            }
        }
    }
}
// 把链表分别存放到文件中,file free chain
void FileSystem::saveFileChain()
{
    freeBlock fblocks[TIMES_OF_D_AND_B];
    block fp = file_block_head->next;
    int index = 0;
    int block_num = 0;
    while(fp) {
        if (block_num >= FREE_BLOCK_NUM / 2) {
            printf("SAVE_FILE_CHAIN::BEYOND_RANGE\n");
            exit(0);
        }
        fblocks[index].front_num = fp->front_num;
        fblocks[index].length = fp->length;
        fblocks[index].next = fp->next;
        fp = fp->next;
        if (++index == TIMES_OF_D_AND_B) {
            writeBlock((char*)fblocks, block_num++, sizeof(freeBlock) * TIMES_OF_D_AND_B);
            index = 0;
        }
    }
    if (block_num >= FREE_BLOCK_NUM) {
        printf("SAVE_FILE_CHAIN::BEYOND_RANGE\n");
        exit(0);
    }
    memset(fblocks+index, 0, sizeof(freeBlock) * (TIMES_OF_D_AND_B - index));
    writeBlock((char*)fblocks, block_num, sizeof(freeBlock) * TIMES_OF_D_AND_B);
    // free inode chain
    while (file_block_head) {
        block p = file_block_head->next;
        free(file_block_head);
        file_block_head = p;
    }
    
}

// 得到一个文件目录的指针
dir FileSystem::getDir(const string &path)
{
    if (!dir_root) {
        printf("GET DIR::dir root is NULL!\n");
        exit(-1);
    }
    // 如果path为空，或者为"/"直接返回root
    if (path.empty() || path == "/") {
        return dir_root;
    }
    // 初期先不考虑 "." 和".."这两个特殊的文件目录
    // 如果路径是从根目录开始
    dir dp = NULL;
    size_t index = 0;
    if (path[0] == '/') {
        dp = dir_root;
        index = 1; // 忽略第一个字符
    } else {
        dp = dir_curr; // 如果不是，那么就从当前目录下开始搜索
    }

    // 保存所有的路径名称
    string lpath = path.substr(index);
    vector<string> sub_files;
    int n_index = index;
    while ((n_index = lpath.find("/")) != string::npos) {
        sub_files.push_back(lpath.substr(0, n_index));
        lpath = lpath.substr(n_index+1);
    }
    if (!lpath.empty()) {
        sub_files.push_back(lpath.substr(n_index+1));
    }
    // 移动dp 直到指向正确的目录文件
    for (int i = 0; i < sub_files.size(); i++) {
        dp = dp->childDir;
        string s = sub_files[i];
        while (dp && strcmp(s.data(), dp->data.filename) != 0) {
                dp = dp->nextDir;
        }
        if (!dp) {
            // printf("GET DIR::CURRENT DIR NOT EXIST THIS FILE!\n");
            return NULL;
        }
    }
    return dp;
}

// 添加一个文件目录
dir FileSystem::addDir(const string &path, FILE_DIR_TYPE type)
{
    // 如果进行存在同名文件，无法创建
    if (getDir(path)) {
        cout << "addDir::File is already existed!" << endl;
        return NULL;
    }
    size_t l_index = path.rfind("/");
    string file_name = path.substr(l_index+1);
    dir parent_dir = l_index==-1 ? dir_curr : getDir(path.substr(0, l_index));
    #ifdef dir_debug
    cout << path.substr(0, l_index) <<", " << file_name << endl;
    #endif
    // 如果路径错误
    if (!parent_dir) {
        cout << "Add_DIR:: PATH IS WRONG!" << endl;
        return NULL;
    }
    
    // 如果上一级目录下已经有了文件，那么需要把指针移动到最后一个文件处
    dir p =NULL;
    if (parent_dir->childDir) {
        parent_dir = parent_dir->childDir;
        while (parent_dir->nextDir) {
            parent_dir = parent_dir->nextDir;
        }
        parent_dir->nextDir = (dir) malloc(sizeof(directory));
        p = parent_dir->nextDir;
    } else {
        parent_dir->childDir = (dir) malloc(sizeof(directory));
        p = parent_dir->childDir;
    }
    
    strcpy(p->data.filename, file_name.data());
    // 直接创建一个新的inode说明这个文件系统不具备链接文件功能
    p->data.inode = createInode(); 
    p->data.dir_type = type; // 类型
    p->nextDir = NULL;
    p->childDir = NULL;
    // parent_dir->nextDir->type =  // dismiss type
    return p;
}

// 修改一个文件目录
int FileSystem::modifyDir(const string &path, const directory &s_dir)
{
    dir dp = getDir(path);
    if (!dp) {
        cout << "MODIFYDIR::PATH WRONG!" << endl;
        return -1;
    }

    // 这里只提供修改两项，第一项是文件名，下一项是inode编号
    strcpy(dp->data.filename, s_dir.data.filename);
    dp->data.inode = s_dir.data.inode; 
    dp->data.dir_type = s_dir.data.dir_type;
    return 0;
}

// 删除一个文件目录
int FileSystem::deleteDir(const string &path)
{
    dir dp = getDir(path);
    if (!dp) {
        cout << "DELETEDIR::PATH WRONG!" << endl;
        return -1;
    }
    if (dp->childDir) {
        cout << "DELETEDIR::FORBID DELETING DIR!" << endl;
        return -1;
    }
    size_t l_index = path.rfind("/");
    dir parent = l_index==-1 ? dir_curr : getDir(path.substr(0, l_index));
    // 如果父目录下第一个子目录就是目标目录
    if (parent->childDir == dp) {
        parent->childDir = dp->nextDir;
    } else {
        // 如果不是直接子目录，需要遍历子目录，然后得到目标子目录
        parent = parent->childDir;
        while(parent->nextDir != dp) {
            parent = parent->nextDir;
        }
        parent->nextDir = dp->nextDir;
    }
    free(dp);
    dp = NULL;
    return 0;
}

// 递归的把树数据写入到文件中
static void writeDirTree(dir p, ofstream &os) {
    if (!p) {
        dir_data dirData;
        strcpy(dirData.filename, NULL_STRING);
        dirData.inode = 0;
        dirData.dir_type = INDEX_FILE;
        os << dirData.filename << " " << dirData.inode << " " << dirData.dir_type << " ";
    } else {
        os << p->data.filename << " " << p->data.inode << " " << p->data.dir_type << " ";
        writeDirTree(p->childDir, os);
        writeDirTree(p->nextDir, os);
    }
}


// 递归的读取文件中的数据，重建一棵树
static void readDirTree(dir &p, ifstream &is) {
    int inode;
    char filename[FILENAME_SIZE];
    int type;
    is >> filename >> inode >> type;
    if (inode == 0) {
        p = NULL;
        return;
    } else {
        p = (dir) malloc(sizeof(directory));
        p->data.inode = inode;
        p->data.dir_type = (FILE_DIR_TYPE) type;
        strcpy(p->data.filename, filename);
        readDirTree(p->childDir, is);
        readDirTree(p->nextDir, is);
    }
}

static void freeDirTree(dir p) {
    if (!p) {
        return;
    } else {
        freeDirTree(p->childDir);
        freeDirTree(p->nextDir);
        free(p);
    }
}

// 把dir树写入磁盘中
void FileSystem::writeDirintoDisk()
{
    ofstream os("dirTree.dat");
    writeDirTree(dir_root, os);
    freeDirTree(dir_root);
    os.close();
}
// 从磁盘中读取dir树
void FileSystem::readDirFromDisk()
{
    ifstream is("dirTree.dat");
    readDirTree(dir_root, is);
    dir_curr = dir_root;
    is.close();
}

