# File operate

## OBJECT_ATTRIBUTES

​    内核中打开文件需要使用一个 OBJECT_ATTRIBUTES 结构，这个结构总是被 InitializeObjectAttributes 初始化

```c
VOID InitializeObjectAttributes(
    OUT POBJECT_ATTRIBUTES InitializedAttributes,
    IN PUNICODE_STRING ObjectName,
    IN ULONG Attributes,
    IN HANDLE RootDirectory,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor);
```

​    InitializedAttributes 是要初始化的 OBJECT_ATTRIBUTES 结构的指针，ObjectName 则是对象名字字符串

​    值得注意的是，无论打开文件、注册表还是打开设备，都会先调用 InitializeObjectAttributes 来初始化一个 OBJECT_ATTRIBUTES 

​    Attributes 只需要填写 OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE 即可。

​    OBJ_CASE_INSENSITIVE 意味着名字字符串是不区分大小写的。OBJ_KERNEL_HANDLE 表明打开的文件句柄是一个内核句柄。

​    内核文件句柄比应用层句柄使用更方便，可以不受线程和进程的限制，在任何线程中都可以读 / 写；同时打开内核文件句柄不需要顾及当前进程是否有权限访问该文件的问题。

​    RootDirectory 用于相对打开的情况，传入 NULL 即可

​    SecurityDescriptor 用于设置安全描述符。由于总是打开内核句柄，所以一般设置为 NULL



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

​    FileHandle：一个句柄的指针。如果这个函数调用返回成功，那么打开的文件句柄就返回在这个地址内

​    DesireAccess：申请的权限。有两个宏分别组合了常用的读权限和常用的写权限，分别为 GENERIC_READ 和 GENRIC_WRITE。还有一个代替全部权限，是 GENERIC_ALL。此外，如果想同步打开文件，请加上 SYNCHRONIZE。这些条件可以 | 来组合

​    Object_Attribute ：对象描述。里面包含了要打开的文件名称

​    IoStatusBlock：这个结构在内核开发中经常使用，往往用于标识一个操作的结果。结构公开如下：

```c
typedef struct _IU_STATUS_BLOCK{
    union {
          NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;
```

​    一般的，返回的结果在 Status 中，而进一步的信息将会在 Information 中，针对 ZwCreateFile ，Information 的返回值有以下几种

- FILE_CREATED：文件被成功地新建了
- FILE_OPENED：文件被打开了
- FILE_OVERWRITTE：文件被覆盖了
- FILE_SUPERSEDED：文件被替代了
- FILE_EXISTS：文件已存在
- FILE_DOES_NOT_EXIST：文件不存在

​    这些返回值和打开文件的意图有关

​    AllocationSize：指向一个64位整数的指针，这个数定义文件初始分配的大小。该参数仅关系到创建或重写文件操作，如果忽略它，那么文件长度从0开始，并随着写入而增长

​    FileAttributes：这个参数控制新建立的文件属性，一般地，设置为0或者 FILE_ATTRIBUTE_NORMAL 即可

​    ShareAccess：一共有三种共享标志可以设置：FILE_SHARE_READ、FILE_SHARE_WRITE 和 FILE_SHARE_DELETE。这三种标志可以用 | 来组合。举例如下：如果本次打开只是用了 FILE_SHARE_READ，那么这个文件在本次打开之后、关闭之前，别的代码试图以读权限打开，则被允许，可以成功打开，否则返回共享冲突

​    CreateDisposition：这个参数说明了打开的意图。可能的选择如下：

- FILE_CREATE
- FILE_OPEN
- FILE_OPEN_IF：如果不存在则新建
- FILE_OVERWRITE：覆盖
- FILE_OVERWRITE_IF
- FILE_SUPERSEDE：取代或新建

​    CreateOptions：通常使用 FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT。此时文件被同步地打开，而且打开的是文件（创建目录使用 FILE_DIRECTORY_FILE）。同步指的是每次操作文件时，不会有 STATUS_PENDING 的情况

​    此外 FILE_NO_INTERMEDIATE_BUFFERING 选项加入 CreateOptions 会使得不通过缓冲操作文件。带了这个标志后，每次读/写都必须以磁盘扇区大小对称（最常见的是512bytes）

​    举例使用如下：

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

​    路径的写法是因为 ZwCreateFile 使用的是对象路径，"C:"是一个符号链接对象，符号链接对象一般都在 "\\\\??\\\\" 路径下

​    关闭文件句柄使用 ZwClose 函数，由于打开的文件句柄是内核句柄，所以可以在任意进程中关闭，示例如下：

```c
ZwClose(file_handle);
```



## ZwReadFile/ZwWriteFile

```c
NTSTATUS ZwReadFile(
	IN HANDLE FileHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE ApcRoutine,
	IN PVOID ApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IOStatusBlock,
	OUT PVOID Buffer,
	IN ULONG Length,
	IN PLARGE_INTEGER ByteOffset OPTIONAL,
	IN PULONG Key OPTIONAL);
```

​	FileHandle：文件句柄不再介绍

​	Event：一个事件，异步时使用，同步填写 NULL

​	ApcRoutine：回调例程，异步时使用，同步填写 NULL

​	IoStatusBlock：返回结果状态

​	Buffer：读文件内容缓冲区

​	Length：试图读取文件的长度

​	ByteOffset：要读取的文件的偏移量

​	Key：读取文件时使用的一种附加信息，一般设置为 NULL

​	返回值：只要读取到任意多个字节都会返回 STATUS_SUCCESS，如果仅是读取文件长度之外的部分，则返回的是：STATUS_END_OF_FILE

​	举个使用了 ZwReadFile 和 ZwWriteFile 的例子

```c
NTSTATUS MyCopyFile(
	PUNICODE_STRING target_path,
	PUNICODE_STRING source_path)
{
    HANDLE target = NULL, source = NULL;
    PVOID buffer = NULL;
    LARGE_INTEGER offset = { 0 };
    IO_STATUS_BLOCK io_status = { 0 };
    
    do{
        //get target, source and buffer with 4KB
        NTSTATUS status;

        OBJCET_ATTRIBUTES target_object_attributes, 
                        source_object_attributes;
        
        InitializeObjectAttributes(
            &target_object_attributes,&target_path,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,NULL);
        InitializeObjectAttributes(
            &source_object_attributes,&source_path,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,NULL);

        status = ZwCreateFile(
            &target,
            GENRIC_READ | GENERIC_WRITE,
            &target_object_attributes,
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
        if(!NT_SUCCESS(status))
            break;
        
        status = ZwCreateFile(
            &source,
            GENRIC_READ | GENERIC_WRITE,
            &source_object_attributes,
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
        if(!NT_SUCCESS(status))
            break;
        
        Buffer = (PVOID)ExAllocatePollWithTag(NonPagedPool, 4096, '1234');
        if(Buffer == NULL)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            //...
        }
        ///
        
        while(1){
            length = 4 * 1024;
            status = ZwReadFile(source, NULL, NULL, NULL, &io_status, 
                                buffer, length, &offset, NULL);
            if(!NT_SUCCESS(status))
            {
                if( status == STATUS_END_OF_FILE )//copy end
                    status = STATUS_SUCCESS;
                break;
            }
            length = IoStatus.Information;//actual read length
            
            status = ZwReadFile(target, NULL, NULL, NULL, &io_status, 
                                buffer, length, &offset, NULL);
            if(!NT_SUCCESS(status))
                break;
            
            offset.QuadPart += length;
        }
    }while(0);
    
    if(target != NULL)
        ZwClose(target);
    if(source != NULL)
        ZwClose(source);
    if(buffer != NULL)
        ExFreePool(buffer);
    return STATUS_SUCCESS;
}
```



# Regedit Operate

## ZwOpenKey

```c
NTSTATUS ZwOpenKey(
	OUT PHANDLE KeyHandle,
	IN ACCESS_MASK DesireAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes);
```

​	很显然，ObjectAttributes 是用来标识需要打开的子键路径，也是需要通过 InitializeObjectAttributes 来初始化的，驱动变成中子键路径与应用层稍有不同

| 应用编程中对应的子键 |       驱动编程中路径的写法       |
| :------------------: | :------------------------------: |
|  HKEY_LOCAL_MACHINE  |        \Registry\Machine         |
|      HKEY_USERS      |          \Registry\User          |
|  HKEY_CLASSES_ROOT   |          没有对应的路径          |
|  HKEY_CURRENT_USER   | 没有简单的对应路径，但是可以求得 |

 	DesiredAccess 支持一系列的组合权限，通常可以使用宏 KEY_READ，KEY_WRITE 和 KEY_ALL_ACCESS。

​	下面一个例子读取注册表中保存的 Windows 系统目录

```c
HANDLE my_key = NULL;
NTSTATUS status;

UNICODE_STRING my_key_path = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
OBJECT_ATTRIBUTE my_obj_attr = { 0 };

InitializeObjectAttributes(
    &my_obj_attr,
    &my_key_path,
    OBJ_CASE_INSENSITIVE,
    NULL, NULL);

status = ZwOpenKey(&my_key, KEY_READ, &my_obj_attr);

if(!NT_SUCCESS(status))
{
    //...Error Handler
}
```



## ZwQueryValueKey

```c
NTSTATUS ZwQueryValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueName,
    IN KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    OUT PVOID KeyValueInformation,
    IN ULONG Length,
    OUT PULONG ResultLength);
```

​	ValueName：欲读取的值的名字

​	KeyValueInformationClass：所需要查询的信息类型，包括三种信息类型

- KeyValueBasicInformation：包含值名和类型
- KeyValueFullInformation：包含值名、类型和值的数据
- KeyValuePartInformation：包含类型和值数据

​	很显然名字是已知的，而 ZwQueryValueKey 正是用来获取类型和值数据，所以通常使用 KeyValuePartInformation

​	KeyValueInformation：当 KeyValuePartInformation 被设置时， KEY_VALUE_PARTIAL_INFORMATION 结构将被返回到这个结构所指的内存中

```c
typedef struct _KEY_VALUE_PARTIAL_INFORMATION {
    ULONG TitleIndex;    //忽略
    ULONG Type;          //数据类型
    ULONG DataLength;    //数据长度
    UCHAR Data[1];       //可变长度的数据
}KEY_VALUE_PARTIAL_INFORMATION ,*KEY_VALUE_PARTIAL_INFORMATION;
```

​	Type 有很多种可能，但是最常见的有一下集中

- REG_BINARY：可变长的二进制数据
- REG_DWORD：四字节整数
- RED_DWORD_BIG_ENDIAN：大端四字节整数
- REG_EXPAND_SZ：空结尾的 Unicode 串，包含 %-escapes 环境变量
- EGE_MULTI_SZ：一个或多个空结尾的 Unicode 串，最后有一个额外的 null
- REG_SZ：空结尾的 Unicode 串

​	Length：用户传入的输出空间 KeyValueInformation 的长度

​	ResuleLength：返回实际需要的长度

​	返回值：如果实际需要的长度比 Length 要大，那么返回 STATUS_BUFFER_OVERFLOW 或者 STATUS_BUFFER_TOO_SMALL。成功读出了全部数据则返回 STATUS_SUCCESS，其他的情况返回一个错误码

​	下面将上节的例子补充完整

```c
UNICODE_STRING my_key_name = RTL_CONSTANT_STRING(L"SystemRoot");
KEY_VALUE_PARTIAL_INFORMATION key_infor;
PKEY_VALUE_PARTIAL_INFORMATION ac_key_infor;
ULONG ac_length;

status = ZwQueryValueKey(my_key, &my_key_name, 
                         KEY_VALUE_PARTIAL_INFORMATION, &key_infor,
                        sizeof(KEY_VALUE_PARTIAL_INFORMATION), &ac_length);
if(!NT_SUCCESS(status) && 
   status != STATUS_BUFFER_OVERFLOW &&
   status != STATUS_BUFFER_TOO_SMALL)
{
    //Error handle
}

ac_key_infor = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(NonpagePool, ac_length, '1234');
if(ac_key_infor == NULL)
{
    status = STATUS_INSUFFICIENT_RESOURCES;
    //Error handle
}
status = ZwQueryValueKey(my_key, &my_key_name, 
                         KEY_VALUE_PARTIAL_INFORMATION, &ac_key_infor,
                        sizeof(KEY_VALUE_PARTIAL_INFORMATION), &ac_length);
if(!NT_SUCCESS(status))
{
    //Error handle
}

UNICODE_STRING key_value;
RtlInitUnicodeString(&key_value, ac_key_infor->Data);

DbgPrint((L"%wz : %wz \r\n", &my_key_name, &key_value));
if(my_key)
    ZwClose(my_key);
if(ac_key_infor)
    ExFreePool(ac_key_infor);
```



## ZwSetValueKey

```c
NTSTATUS ZwSetValueKey(
    IN HANDLE KeyHandle,
    IN PUNICODE_STRING ValueNmae,
    IN ULONG TitleIndex OPTIONAL,
    IN ULONG Type,
    IN PVOID Data,
    IN ULONG DataSize);
```

​	TileIndex：始终填入 0 即可

​	KeyHandle、Vlaue、Type 与 ZwQueryValueKey 中的参数相同

​	Data：要写入的数据的开始地址

​	DataSize：要写入的数据的长度

​	下面的代码写入一个名字为 "test"、值为 "My Test Value" 的字符串值，假设 my_key 是一个已经打开的子键的句柄

```c
UNICODE_STRING name = RTL_CONSTANT_STRING(L"test");
UNICODE_STRING value = RTL_CONSTANT_STRING(L"My Test Value");

NTSTATUS status;

status = ZwSetValueKey(my_key, &name, NULL, REG_SZ, value.Buffer, sizeof(WCHAR) * value.Length + 1);//长度加一是为了写入空结束符

if(!NT_SUCCESS(status))
{
    //Error handle
}

if(my_key)
    ZwClose(my_key);
```



# Time and Timer

