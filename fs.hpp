#include <stdlib.h>
#include <cstdio>
#include <string>
#include <ctime>
#include <iostream>

using std::string;
#define debug
/* 拟定初始化一个大小约为100MB的磁盘，以一个文件为10kB为标准，大概可以存储10000个文件。
    但是每个文件都要为之分配dir和inode，所以每个文件的大小都要多加上132B。
    那么可以用6.4MB的空间，也就是6,400个磁盘块来存放dir；同样要使用6400个磁盘块来存放inode，6.4MB，
    理论情况下最多可以存放100,000个文件.
    那么，
    0\~2 用来存放成组链表的结点信息
    3\~6,399的磁盘块用来存放dir树形结构
    6,400\~11,199个磁盘块来存放inode
    11,200~99,999的磁盘块用来存放数据和一级和二级索引表 */
// disk block size is 1024 B = 1kB
#define DISK_BLOCK_SIZE 1024
#ifdef  debug
#define DISK_BLOCK_NUM 10000
#define FREE_BLOCK_NUM 10
#define DIR_BLOCK_NUM 640
#define INODE_BLOCK_NUM 480
#else 
#define DISK_BLOCK_NUM 100000
#define FREE_BLOCK_NUM 10
#define DIR_BLOCK_NUM 6390
#define INODE_BLOCK_NUM 4800
#endif
// 定义数据区磁盘块的描述信息
#define FRONT_DATA_BLOCK_NUM (DIR_BLOCK_NUM + INODE_BLOCK_NUM + FREE_BLOCK_NUM) // 11200
#define FRONT_INODE_BLOCK_NUM (FREE_BLOCK_NUM + DIR_BLOCK_NUM)

// 定义较小的主磁盘块来存放inode和dir
#define DIR_BLOCK_SIZE sizeof(dir_data)
#define MAIN_BLOCK_SIZE sizeof(inode)
#define FREE_BLOCK_SIZE sizeof(freeBlock)

// 定义磁盘块大小和一些数据结构的倍数关系
#define TIMES_OF_D_AND_M (DISK_BLOCK_SIZE / MAIN_BLOCK_SIZE)
#define TIMES_OF_D_AND_B (DISK_BLOCK_SIZE / FREE_BLOCK_SIZE)
#define TIMES_OF_D_AND_DIR (DISK_BLOCK_SIZE / DIR_BLOCK_SIZE)

// 定义无效目录文件的filename
#define NULL_STRING "IS_INVALID_FILENAME"

// 链表法来管理空闲块
typedef struct freeBlock {
    int front_num; // 连续空闲的磁盘块的首块块号
    int length; // 接下来有多少连续的空闲块
    struct freeBlock *next; // 指向下一个连续空闲块的指针
}*block; 

// 一定与文件索引相关的常量
#define FILENAME_SIZE 48
#define INDEX_TABLE_SIZE 12
#define PRI_INDEX_TABLE_SIZE (DISK_BLOCK_SIZE / sizeof(int))

// 定义各类索引表项的个数
#define I_ADDR_DIRECT_INDEX_NUM 9
#define I_ADDR_PRIMARY_INDEX_NUM 2
#define I_ADDR_SEC_INDEX_NUM 1

// 定义读写缓冲常量
#define READ_BUFFER_SIZE DISK_BLOCK_SIZE
#define WRITE_BUFFER_SIZE DISK_BLOCK_SIZE



// 定义文件目录类型，包括索引文件，目录文件，块文件，流文件
enum FILE_DIR_TYPE { 
    INDEX_FILE, DIR_FILE, BLOCK_FILE, STREAM_FILE 
};

enum BLOCK_TYPE {
    FILE_BLOCK, DIR_BLOCK, INODE_BLOCK
};

enum CHAIN_OP {
    CHAIN_OP_ADD, CHAIN_OP_REMOVE
};
// 文件信息
/* 1. 文件名。包括文件的符号名和内部标识符（id号）
2. 文件的逻辑结构，包括文件的记录是否定长、记录长度及记录个数。
3. 文件的物理结构，即文件信息在辅存中的物理位置及排布。我这里使用索引结构来组织，所以应该记录索引表的位置。
4. 存取控制信息。
5. 管理信息，包括创建时间、上次修改日期。
6. 文件类型。包括数据文件，目录文件等。
7. 下一级目录文件指针。
8. 同级目录文件指针。
*/

struct inode {
    int file_id;   //文件id
    size_t file_size; // 文件大小，简化了记录长度和记录个数
    /* index 0 to 8 is direct index; index 9 and 10 is primary index; 
        index 11 is secondary index*/
    int i_addr[INDEX_TABLE_SIZE];
    // int premissions; // 权限暂时忽略
    time_t create_time;  // 创建时间
    time_t last_modify_time; // 上一次修改时间
    unsigned short i_link; // 代表着硬链接的个数
}; 

typedef struct dir_data {
    char filename[FILENAME_SIZE]; // 文件名
    int inode;  // 记录inode的磁盘块号
    FILE_DIR_TYPE dir_type;
}data;

// 目录文件
typedef struct directory {
    dir_data data;
    struct directory *nextDir; // 当前目录下的下一个目录文件
    struct directory *childDir; // 当前目录下的子目录，文件不存在子目录
}*dir; 
 
// 用来记录inode缓冲，inode写入文件和读出都与这个数据结构相关
 typedef struct inode_buffer {
    int block_num; // 写入或者读入的磁盘块号 
    inode data[TIMES_OF_D_AND_M]; // inode 缓存
    bool isWriteable[TIMES_OF_D_AND_M]; // 记录是否可写
    int buffer_num; // 现有的缓冲个数
}inode_buff;

enum SHELL_OP {
    CD, MKDIR, OPEN, WRITE, READ, LS, CP, LK, RM, QUIT, CLOSE
};
class FileSystem {
public:
    FileSystem();
    ~FileSystem();
    /* 系统调用相关 */
    // 打开文件
    int openFile(const string &f_path, const FILE_DIR_TYPE type=INDEX_FILE);
    // 关闭文件
    int closeFile();
    // 读取文件
    int readFile(char *targ, const size_t size);
    // 写入文件
    int writeFile(const string buf, const size_t size);
    

    /* 文件系统操作 */
    // 复制文件
    int copyFile(const string &src, const string &targ);
    // 删除文件
    int removeFile(const string &f_path);
    // 改变当前目录
    void changeDir(const string &path);
    // 查看当前目录下的所有文件
    void lookFiles();
    // 查看指定目录下的所有文件
    void lookFiles(const string &path);
    // 创建一个文件快捷方式
    int linkFile(const string &src, const string &targ);

    // 创建一个能够交互的UI界面
    void shell();
private: 
    // 关于磁盘
    FILE *disk;  // 文件模拟磁盘的文件指针
    block file_block_head; // 文件空闲块头指针，增加头结点，使得增删改查一致
    block file_block_tail; // 文件空闲块尾指针

    block inode_block_head; // inode 链表头指针，增加头结点，使得增删改查一致
    block inode_block_tail; // 
    inode_buff inode_buf; // 读写缓冲
    // 关于文件系统
    dir dir_root; // 根目录指针
    dir dir_curr; // 当前目录指针

    // 文件操作相关数据
    dir file_dir;
    inode *file_inode;
    int file_p;
    char read_buff[READ_BUFFER_SIZE]; // 读取的缓冲区
    char write_buff[WRITE_BUFFER_SIZE]; // 写入的缓冲区
    char cmd[3][100]; // 用来保存命令行参数
    char str_dir[100]; // 用来保存路径信息

private:
    // 初始化一块固定大小的文件作为磁盘
    void initDisk(const size_t d_size);

    // 读取一个磁盘块
    void readBlock(char *const targ, const size_t block_num);
    // 读取一个磁盘块
    void readBlock(char *const targ, const size_t block_num, const int mum);
    // 读取连续的几个磁盘块
    void readBlock(char *const targ, const size_t block_num , const size_t num);

    // 写入信息到一个磁盘块
    void writeBlock(const char *src, const size_t block_num);    
    // 写入信息到一个磁盘块
    void writeBlock(const char *src, const size_t block_num, const int num);
    // 写入连续的几个磁盘块
    void writeBlock(const char *src, const size_t block_num, const size_t num);

    // 修改file free chain,
    void modifyFileChain(const CHAIN_OP op, const size_t block_num);
    // 修改inode空闲链表
    void modifyInodeChain();
    // 找到一个可写的磁盘块
    int getWriteableBlock(BLOCK_TYPE type);
    

    // 从磁盘中得到一个inode
    inode *getInode(size_t block_num);
    // 增加一个inode
    int createInode();
    // 修改一个inode
    int modifyInode(size_t block_num, const inode &s_inode);
    // 删除一个inode
    int deleteInode(size_t block_num);


    // 重置inode buffer
    void resetInodeBuffer();
    
    // 读取一个磁盘块来填充inode_buffer
    int fillInodeBufferFromDisk(const size_t num);
    
    // 把链表存放在相应的磁盘上, inode free chain
    void readInodeChain();
    // 把链表分别存放到文件中, inode free chain
    void saveInodeChain();

    // 把链表存放在相应的磁盘上, file free chain
    void readFileChain();
    // 把链表分别存放到文件中, file free chain
    void saveFileChain();

    // 得到一个文件目录的指针
    dir getDir(const string &path);
    // 添加一个文件目录
    dir addDir(const string &path, FILE_DIR_TYPE type);
    // 修改一个文件目录
    int modifyDir(const string &path, const directory &s_dir);
    // 删除一个文件目录
    int deleteDir(const string &path);

    // 从磁盘中读取dir树
    void readDirFromDisk();
    // 把dir树写入磁盘中
    void writeDirintoDisk();

    // 分析shell中的输入
    SHELL_OP analyse(char *input);
};


