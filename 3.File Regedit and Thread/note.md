# File operate

## OBJECT_ATTRIBUTES

​	内核中打开文件需要使用一个 OBJECT_ATTRIBUTES 结构，这个结构总是被 InitializeObjectAttributes 初始化

```c
VOID InitializeObjectAttributes(
	OUT POBJECT_ATTRIBUTES InitializedAttributes,
	IN PUNICODE_STRING ObjectName,
	IN ULONG Attributes,
	IN HANDLE RootDirectory,
	IN PSECURITY_DESCRIPTOR SecurityDescriptor);
```

​	InitializedAttributes 是要初始化的 OBJECT_ATTRIBUTES 结构的指针，ObjectName 则是对象名字字符串

​	值得注意的是，无论打开文件、注册表还是打开设备，都会先调用 InitializeObjectAttributes 来初始化一个 OBJECT_ATTRIBUTES 

​	Attributes 只需要填写 OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE 即可。

​	OBJ_CASE_INSENSITIVE 意味着名字字符串是不区分大小写的。OBJ_KERNEL_HANDLE 表明打开的文件句柄是一个内核句柄。

​	内核文件句柄比应用层句柄使用更方便，可以不受线程和进程的限制，在任何线程中都可以读 / 写；同时打开内核文件句柄不需要顾及当前进程是否有权限访问该文件的问题。

​	RootDirectory 用于相对打开的情况，传入 NULL 即可

​	SecurityDescriptor 用于设置安全描述符。由于总是打开内核句柄，所以一般设置为 NULL



## ZwCreateFile

这个函数用于打开一个文件

```c
NTSTATUS ZwCreateFile(
	OUT PHANDLE FileHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES Object_Attribute,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PLARGE_INTEGER AllocationSize OPTIONAL,
    IN ULONG FileAttributes,
    IN ULONG ShareAccess,
    IN ULONG CreateDisposition,
    IN ULONG CreateOptions,
    IN PVOID EaBuffer OPTIONAL,
    IN ULONG EaLength);
```

​	FileHandle：一个句柄的指针。如果这个函数调用返回成功，那么打开的文件句柄就返回在这个地址内

​	DesireAccess：申请的权限。有两个宏分别组合了常用的读权限和常用的写权限，分别为 GENERIC_READ 和 GENRIC_WRITE。还有一个代替全部权限，是 GENERIC_ALL。此外，如果想同步打开文件，请加上 SYNCHRONIZE。这些条件可以 | 来组合

​	Object_Attribute ：对象描述。里面包含了要打开的文件名称

​	IoStatusBlock：这个结构在内核开发中经常使用，往往用于标识一个操作的结果。结构公开如下：

```c
typedef struct _IU_STATUS_BLOCK{
    union {
      	NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
```

​	一般的，返回的结果在 Status 中，而进一步的信息将会在 Information 中，针对 ZwCreateFile ，Information 的返回值有以下几种

- FILE_CREATED：文件被成功地新建了
- FILE_OPENED：文件被打开了
- FILE_OVERWRITTE：文件被覆盖了
- FILE_SUPERSEDED：文件被替代了
- FILE_EXISTS：文件已存在
- FILE_DOES_NOT_EXIST：文件不存在

​	这些返回值和打开文件的意图有关

​	AllocationSize：指向一个64位整数的指针，这个数定义文件初始分配的大小。该参数仅关系到创建或重写文件操作，如果忽略它，那么文件长度从0开始，并随着写入而增长

​	FileAttributes：这个参数控制新建立的文件属性，一般地，设置为0或者 FILE_ATTRIBUTE_NORMAL 即可

​	ShareAccess：一共有三种共享标志可以设置：FILE_SHARE_READ、FILE_SHARE_WRITE 和 FILE_SHARE_DELETE。这三种标志可以用 | 来组合。举例如下：如果本次打开只是用了 FILE_SHARE_READ，那么这个文件在本次打开之后、关闭之前，别的代码试图以读权限打开，则被允许，可以成功打开，否则返回共享冲突

​	CreateDisposition：这个参数说明了打开的意图。可能的选择如下：

- FILE_CREATE
- FILE_OPEN
- FILE_OPEN_IF：如果不存在则新建
- FILE_OVERWRITE：覆盖
- FILE_OVERWRITE_IF
- FILE_SUPERSEDE：取代或新建

​	CreateOptions：通常使用 FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT。此时文件被同步地打开，而且打开的是文件（创建目录使用 FILE_DIRECTORY_FILE）。同步指的是每次操作文件时，不会有 STATUS_PENDING 的情况

​	此外 FILE_NO_INTERMEDIATE_BUFFERING 选项加入 CreateOptions 会使得不通过缓冲操作文件。带了这个标志后，每次读/写都必须以磁盘扇区大小对称（最常见的是512bytes）

​	举例使用如下：

```c
HANDLE file_handle = NULL;

NTSTATUS status;

OBJCET_ATTRIBUTES object_attributes;
UNICODE_STRING ufile_name = RTL_CONSTANT_STRING(L"\\??\\C:\\a.dat");
InitializeObjectAttributes(
	&object_attributes,
	&ufile_name,
	OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
	NULL,
	NULL);

status = ZwCreateFile(
	&file_handle,
	GENRIC_READ | GENERIC_WRITE,
	&object_attributes,
	&io_status,
	NULL,
	FILE_ATTRIBUTE_NORMAL,
	FILE_SHARE_READ,
	FILE_OPEN_IF,
	FILE_NON_DIRECTORY_FILE |
	FILE_RANDOM_ACCESS |
	FILE_SYNCHRONOUS_IO_NONALERT,
	NULL,
	0);
```

​	路径的写法是因为 ZwCreateFile 使用的是对象路径，"C:"是一个符号链接对象，符号链接对象一般都在 "\\\\??\\\\" 路径下

​	关闭文件句柄使用 ZwClose 函数，由于打开的文件句柄是内核句柄，所以可以在任意进程中关闭，示例如下：

```c
ZwClose(file_handle);
```



## ZwReadFile