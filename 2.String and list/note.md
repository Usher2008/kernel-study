# String

## string struct

传统C语言定义和使用字符串

```c
char *str = {"my first string"};                //ANSI字符串
wchar_t *wstr = {L"my first string"};           //Unicode字符串
size_t len strlen(str);                         //ANSI字符串求长度
size_t wlen = wcslen(wstr);                     //Unicode字符串求长度
printf("%s %ws %d %d", str, wstr, len, wlan);   //打印两种字符串
```

驱动开发定义以下结构

```c
typedef struct _UNICODE_STRING{
    USHORT  Length;
    USHORT  MaximumLength;
    PWSTR   Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _STRING{
    USHORT  Length;
    USHORT  MaximumLength;
    PSTR    Buffer;
} ANSI_STRING, *PUNICODE_STRING;
```

UNICODE_STRING 并不保证 Buffer 指向的字符串始终以空结束，所以下列做法是错误的

```c
UNICODE_STRING str;
//...
len = wcslen(str.Buffer);
DbgPrint("%ws", str.Buffer);
```

## init string

错误示范

```c
UNICODE_STRING str = {0};
//试图把字符串拷贝到新定义的字符串结构中
wcscpy(str.Buffer, L"my first string!");
str.Length = str.MaximumLength =
wcslen(L"my first string!") * sizeof(WCHAR);
```

正确做法

```c
UNICODE_STRING str = {0};
WCHAR strBuf[128] = {0};
str.Buffer = strBuf;
wcscpy(str.Buffer, L"my first string!");
str.Length = str.MaximumLength =
wcslen(L"my first string!") * sizeof(WCHAR);
```

另一种正确做法
```c
UNICODE_STRING str;
str.Buffer = L"my first string!";
str.Length = str.MaximumLength =
wcslen(L"my first string!") * sizeof(WCHAR);
```

初始化可以使用API

```c
VOID RtlInitUnicodeString(
IN OUT PUNICODE_STRING DestinationString,
IN PCWSTR SourceString
);
```

例子

```c
UNICODE_STRING str = {0};
RtlInitUnicodeString(&str, L"my first string!");
```

## copying string

例子

```c
UNICODE_STRING dst;//目标字符串
WCHAR dst_buf[256];//目前不会分配内存，所以先定义缓冲区
UNICODE_STRING src = RTL_CONSTANT_STRING(L"my source string!");
//RTL_CONSTANT_STRING是初始化一个常数字符串常用的宏

RtlInitEmptyUnicodeString(&dst, dst_buf, 256 * sizeof(WCHAR));
//把目标字符串初始化为拥有缓冲区长度为256的UNICODE_STRING空串
RtlCopyUnicodeString(&dst, &src);//字符串拷贝
//如果拷贝的内容长度大于被拷贝字符串长度，则会被截断
```



## strcat

范例

```c
NTSTATUS status;
UNICODE_STRING dst;
WCHAR dst_buf[256];
UNICODE_STRING src = RTL_CONSTANT_STRING(L"My source string!");

RtlInitEmptyUnicodeString(&dst, dst_buf, 256 & sizeof(WCHAR));
RtlCopyUnicodeString(&dst, &src);

status = RtlAppendUnicodeToString(&dst, L"my second string!");
//如果链接两个UNICODE_STRING,第二个参数可以是UNICODE_STRING指针
//返回值可能为STATUS_BUFFER_TOO_SMALL
```



## print string

c语言中经常使用 sprintf 用来将字符串与数字结合起来，宽字符版本为swprintf。虽然在驱动开发中依然可以使用，但不安全，微软建议使用 RtlSringCbPrintfW 来代替它

```c
include <ntstrsafe.h>
//需要连接库ntsafestr.lib

WCHAR dst_buf[512] = {0};
UNICODE_STRING dst;
NTSTATUS status;
UNICODE_STRING file_path = RTL_CONSTANT_STRING(L"\\??\\c:\\winddk\\7600.16385.1\\inc\\cifs.h");
USHORT file_size = 1024;

RtlInitEmptyUnicodeString(&dst, dst_buf, 512 * sizeof(WCHAR));

status = RtlStringCbPrintfW(dst.buffer, 512 * sizeof(WCHAR), L"file path = %wZ file size = %d \r\n",
                           &file_path, file_size);
dst.Length = wcslen(dst.Buffer) * sizeof(WCHAR);
```

值得注意的是，UNICODE_STRING 类型的指针，用%wZ打印可以打印出字符串，在不能保证字符串是以空结束的时候，必须避免使用%ws或者%s

另外是常见的输出打印。printf 函数只有在有控制台输出的情况下才有意义，驱动中没有控制台，但可以使用 WindDbg 查看打印的调试信息。

可以调用 DbgPrint 函数来打印调试信息，这个函数的使用和 printf 基本相同。一个缺点是 DbgPrint 无论是 release 版本还是 debug 版本都会有效。

WDK 中有针对这种情况的给宏指令，名为 KdPrint。由于只支持一个参数，所以必须把所有参数都括起来当成一个参数传入，例如如下代码

```c
status = KdPrint((L"file path = %wz file size = %d \r\n", &file_path, file_size));
```

最后，在使用 DbgPrint 输出 Unicode 字符串时，必须确保当前中断级别是 PASSIVE_LEVEL，可以通过 KeGetCurrentIrql 函数获取当前中断级别



# Memory and List

## alloc and free

c语言中最常用的分配内存的函数是 malloc，然而这个函数在驱动中不再有效。驱动中最常用分配内存的函数是 ExAllocatePoolWithTag。下面的例子是把一个字符串 src 拷贝到字符串 dst。

```c
NTSTATUS status;
#define MEM_TAG 'MtTt'
//定义一个内存分配标记
UNICODE_STRING dst = {0};
UNICODE_STRING src = RTL_CONSTANT_STRING(L"My source string!");

dst.Buffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, src.Length, MEM_TAG);
if(dst.Buffer == NULL)
{
    status = STATUS_INSUFFICIENT_RESOURCES;
}
dst.Length = dst.MaximumLength = src.Length;
RtlCopyUnicodeString(&dst, &src);
```

NonPagedPool 表明分配的内存是锁定内存。这些内存永远真实存在于物理内存上，不会被分页交换到硬盘中。此外可以分配可分页内存，使用 PagedPool 即可。

内存的分配标识用于检测内存泄漏。根据内存的标识，能大概知道泄露的来源。一般每个驱动程序都定义一个自己的内存标识，也可以再每个模块中定义单独的内存标识。内存标识是随意的32位数字，即使冲突也不会有问题。

ExAllocatePoolWithTag 分配的内存可以使用 ExFreePool 来释放。例如：

```c
ExFreePool(dst.Buffer);
dst.Buffer = NULL;
dst.Length = dst.MaximumLength = 0;
```

ExFreePool 不能用来释放一个栈空间的指针，否则系统立即崩溃。例如：

```c
UNICODE_STRING src = RTL_CONSTANT_STRING(L"source string");
ExFreePool(src.Buffer);
```

## LIST_ENTRY

LIST_ENTRY 是一个由 Windows 内核开发者自己开发的双向链表结构。定义如下

```c
typedef struct _LIST_ENTRY{
    struct _LIST_ENTRY *Flink;
    struct _LIST_ENTRY *Blink;
}LIST_ENTRY, *PLIST_ENTRY;
```

其中 Flink 为指向下一个节点的指针，Blink 为指向前一个节点的指针

如果 LIST_ENTRY 作为链表的头，那么在使用之前，必须调用 InitalizeListHead 来初始化，下面是示例代码：

```c
LIST_ENTRY my_list_head;

void MyFileInforInilt()
{
    InitializeListHead(&my_list_head);
}

typedef struct{
    LIST_ENTRY list_entry;
    PFILE_OBJECT file_object;
    PUNICODE_STRING file_name;
    LARGE_INTEGER file_length;
} MY_FILE_INFOR, *PMY_FILE_INFOR;
//FILE_OBJECT 与 LARGE_INTEGER 后文再介绍

NTSTATUS MyFileInforAppendNode(
    PFILE_OBJECT file_object,
    PUNICODE_STRING file_name,
    PLARGE_INTEGER file_length)
{
    PMY_FILE_INFOR my_file_infor =
        (PMY_FILE_INFOR)ExAllocatePoolWithTag(
            PagedPool, sizeof(MY_FILE_INFOR), MEM_TAG);
    if(my_file_infor == NULL)
        return STATUS_INSUFFICIENT_RESOURES;
    
    my_file_infor->file_object = file_object;
    my_file_infor->file_name   = file_name;
    my_file_infor->file_length = file_length;
    
    InsertHeadList(&my_list_HEAD, (PLIST_ENTRY)&my_file_infor);
    return STATUS_SUCCESS;
}
```

链表初始化函数 InitializeListHead 很简单，只有一个参数，即要初始化的链表头。

上面的代码使用 InsertHeadList 时，一个 MY_FILE_INFOR 看起来就像是一个 LIST_ENTRY，这是 LIST_ENTRY 插入到 MY_FILE_INFOR 结构头部的好处。因此通过 LIST_ENTRY 结构的地址获取所在节点的地址时，有个地址编译计算的过程。可以在下面的一个典型的遍历链表的示例中看到：

```c
for(p = my_list_head.Flink; p != &my_list_head.Flink; p = p->Flink)
{
    PMY_FILE_INFOR elem = CONTAINING_RECORD(p, MY_FILE_INFOR, list_entry);
    ...
}
```

CONTAINING_RECORD 是一个已定义的宏，作用是通过一个 LIST_ENTRY 结构的指针，找到这个结构所在节点的指针。定义如下：

```c
#define CONTAINING_RECORD(address, type, field)((type *)(\
    (PCHAR)(address) - \
    (ULONG_PTR)(&((type *)0)->field)))
```

从上面代码可以看出：LIST_ENTRY 中的数据成员 Flink 指向下一个 LIST_ENTRY。整个链表中的最后一个 LIST_ENTRY 的 Flink 不为空，而是指向头节点。得到 LIST_ENTRY 之后，要用 CONTAINING_RECORD 来得到链表节点中的数据。



## LARGE_INTEGER

驱动中经常使用这个结构体来代替 long long 长度的变量，定义如下

```c
typedef union _LARGE_INTEGER{
    struct {
        ULONG LowPart;
        LONG HighPart;
    };
    struct {
        ULONG LowPart;
        LONG HighPart;
    }u;
    LONGLONG QuadPard;
} LARGE_INTEGER;
```

简单的利用如下：

```c
LARGE_INTEGER a, b;
a.QuadPard = 100;
a.QuadPard *= 100;
b.QuadPard = a.QuadPard;

KdPrint("LowPart = %x HighPart = %x", b.u.LowPart, b.HighPart);
```



# Spin lock

## use lock

驱动程序中也有锁来保证函数的线程安全，如下代码初始化获取一个自旋锁

```c
KSPIN_LOCK my_spin_lock;
KeInitializeSpinLock(&my_spin_lock);
```

KeAcquireSpinLock 和 KeReleseSpinLock 之间的代码是只有单线程执行的，但是由于会提高当前中断级，所以使用一个变量来保存之前的中断级

```c
KIRQL irql;
KeAcquireSpinLock(&my_spin_lock, &irql);
//do something
KeReleseSpinLock(&my_spin_lock, &irql);
```

值得注意的是，锁变量应该是静态或是全局的，否则锁将毫无意义



## ExInterlockedInsertHeadList

链表是一个很好的使用锁的例子，LIST_ENTRY 本身并不保证多线程安全性，不过 LIST_ENTRY 有一系列操作，只需要为每个链表定义并初始化一个锁即可使用

```c
LIST_ENTRY my_list_head;
KSPIN_LOCK my_list_lock;

void MyFileInforInilt()
{
    InitializeListHead(&my_list_head);
    KeInitializeSpinlock(&my_list_lock);
}
```

插入节点普通的操作代码如下

```c
InsertHeadList(&my_list_head, (PLIST_ENTRY)& my_file_infor);
my_file_infor = RemoveHeadList(&my_list_head);
```

加锁的操作方式如下

```c
ExInterlockedInsertHeadList(&my_list_head, (PLIST_ENTRY)& my_file_infor, &my_list_lock);
my_file_infor = ExInterlockedRemoveHeadList(&my_list_head, &my_list_lock);
```

相比之下增加了一个自旋锁类型的指针作为参数



## KeAcquireInStackQueuedSpinLock

除了普通的自旋锁之外，还有一种队列自旋锁，这种自旋锁在多 CPU 的平台上有更高的性能表现，并且遵守“谁先等待，谁先获取自旋锁”的原则

使用方式基本一样，如下：

```c
KSPIN_LOCK my_Queue_Spinlock;
KeInitializeSpinlock(&my_Queue_Spinlock);

KLOCK_QUEUE_HANDLE my_lock_queue_handle;
KeAcquireInStackQueuedSpinLock(&my_Queue_Spinlock, &my_lock_queue_handle);
//do something
KeReleseInStackQueuedSpinLock(&my_lock_queue_handle);
```

值得注意的是，一个自旋锁要么按照普通原子锁方式来使用，要么按照列队自旋锁方式来使用
