#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

// 内存分配器（Kernighan & Ritchie 实现）
// 参考：《The C Programming Language》第二版，第 8.7 节。

typedef long Align; // 用于强制内存对齐，确保分配的内存块在不同平台上都对齐到 long 边界

//union（联合体）是一种特殊的结构体，它的所有成员共享相同的内存空间，而且只能同时存储一个成员。
//也就是说，无论 union 里有多少个成员，它的大小都取决于最大的成员。

// 定义内存块的头部结构，采用联合体确保对齐
union header {
  struct {
    union header *ptr; // 指向下一个空闲块的指针（形成空闲链表）
    uint size;         // 该块的大小（以 Header 结构为单位，而不是字节）
  } s;
  Align x; // 强制对齐，确保 Header 的大小符合 Align 要求
};

//s 结构体的作用：
	//	用于管理空闲块的链表，其中 ptr 形成了一个 循环链表，size 记录块的大小。
	//	封装多个成员，使代码更清晰。例如，p->s.ptr 显示的是指向下一个块的指针，而 p->s.size 代表该块的大小。


typedef union header Header; // 重新定义 Header 作为联合体类型

static Header base;  // 空闲链表的起点，作为哨兵节点
static Header *freep = 0; // 指向空闲块链表中的某个节点（循环链表的入口）

// 释放内存，将块重新插入到空闲链表中
void free(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1; // 让 bp 指向其头部（Header 结构）
  
  // 查找正确的插入位置，确保链表保持有序
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    // 处理循环链表的边界情况
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;

  // 合并相邻的空闲块（前向合并）
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else {
    bp->s.ptr = p->s.ptr;
  }

  // 合并相邻的空闲块（后向合并）
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else {
    p->s.ptr = bp;
  }

  freep = p; // 更新空闲链表指针
}

// 申请更多的内存（向系统请求新的内存）
static Header* morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096) // 最小申请单位，避免频繁调用 sbrk
    nu = 4096;
  
  p = sbrk(nu * sizeof(Header)); // 向操作系统申请内存
  if(p == (char*)-1) // sbrk 失败
    return 0;

  hp = (Header*)p; // 将新分配的内存转为 Header 结构
  hp->s.size = nu;
  free((void*)(hp + 1)); // 将新分配的内存块加入空闲链表

  return freep;
}

// 分配指定大小的内存
void* malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  // 计算需要的 Header 数量（包括用户请求的 nbytes 和 Header 结构本身）
  nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

  if((prevp = freep) == 0){ // 如果空闲链表未初始化
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }

  // 遍历空闲链表，寻找合适的空闲块
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){ // 找到足够大的空闲块
      if(p->s.size == nunits) // 正好匹配，直接分配
        prevp->s.ptr = p->s.ptr;
      else { // 否则，拆分空闲块
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p + 1); // 返回用户数据区地址
    }
    
    if(p == freep) // 遍历一圈后仍未找到合适的块
      if((p = morecore(nunits)) == 0) // 申请新内存
        return 0;
  }
}