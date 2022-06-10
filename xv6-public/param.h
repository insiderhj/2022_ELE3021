#define NPROC        64  // maximum number of processes
#define NTHREAD       6  // maximum number of threads
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks
#define MAXPID 2147483647 // max pid

#ifndef MODES
  #define MODES
  #define MODE_RUSR 32 // owner read
  #define MODE_WUSR 16 // owner write
  #define MODE_XUSR 8 // owner execute
  #define MODE_ROTH 4 // others read
  #define MODE_WOTH 2 // others write
  #define MODE_XOTH 1 // others execute
#endif
