# In Kernel

## IoCreateDevice

```c
NTSTATUS IoCreateDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG DeviceExtensionSize,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN DEVICE_TYPE DeviceType,
    IN ULONG DeviceCharacteristics,
    IN BOOLEAN Exclusive,
    OUT PDEVICE_OBJECT *DeviceObject
);
```

​    DriverObject 可以直接从 DriverEntry 的参数中获得

​    DeviceExtensionSize 表示设备扩展的大小，在后面“应用设备扩展”中会描述

​    DeviceName 是设备名，作为控制设备，一般要提供一个设备名，以便应用程序打开

​    DeviceType 是设备类型，定义成 FILE_DEVICE_UNKNOWN 即可

​    DeviceCharacteristics 是一组设备属性，大多数都设定为 FILE_DEVICE_SECURE_OPEN

​    Exclusive 表示是否独占一个设备

​    *DeviceObject 就是生成的设备对象的指针



​    使用 IoCreateDevice 生成的设备及具有默认的安全属性，其结果就是必须要具有管理员权限才能打开它。因此在学习中可以使用另一个不安全的函数来强迫生成一个任何用户都可以打开的设备。

```c
NTSTATUS IoCreateDeviceSecure(
    IN PDRIVER_OBJECT DriverObject,
    IN ULONG DeviceExtensionSize,
    IN PUNICODE_STRING DeviceName OPTIONAL,
    IN DEVICE_TYPE DeviceType,
    IN ULONG DeviceCharacteristics,
    IN BOOLEAN Exclusive,
    IN PCUNICODE_STRING DefaultSDDLString,
    INT LPCGUID DeviceClassGuid,
    OUT PDEVICE_OBJECT *DeviceObject
);
```

​    可以看见增加了两个参数，其中一个是 DefaultSDDLString，这个字符串表示设备对象的安全设置；另一个是 DeviceClassGuid，它是这个设备的 GUID，即所谓的全球唯一标识符。

​    DefaultSDDLString 填写 L"D:P(A;;GA;;;WD)"，据 WDK 说支持任何用户直接打开设备的字符串

​    DeviceClassGuid 理论上需要用 CoCreateGuid 来生成（调用一次以后一直使用这个GUID），实际不与其他相同就行

```c
PDEVICE_OBJECT g_cdo = NULL;

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
    UNICODE_STRING sddl = RTL_CONSTANT_STRING(L"D:P(A;;GA;;;WD)");
    UNICODE_STRING cdo_name = RTL_CONSTANT_STRING(L"\\Device\\slbk_3948d33e");
    //...
    
    //生成一个控制设备对象
    status = IoCreateDeviceSecure(driver, 0, &cdo_name, FILE_DEVICE_UNKNOWN,
                                  FILE_DEVICE_SECURE_OPEN, FALSE, &sddl,
                                  (LPCGUID)&SLBKGUID_CLASS_MYCDO, &g_cdo);
    if(!NT_SUCCESS(status))
        return status;
}

```

##  IoCreateSymbolicLink

​    应用层无法通过该设备的名字来打开对象，但如果使用符号链接，就可以在应用层中暴露出来了。生成符号链接的函数是：

```c
NTSTATUS IoCreateSymbolicLink(
    IN PUNICODE_STRING SymbolicLinkName,
    IN PUNICODE_STRING DeviceName
);
```

​    两个参数都很容易理解作用。值得注意的是，如果一个符号链接已经在系统里存在了，那么这个函数会返回失败。所以最稳妥的方式还是使用 GUID 的方式来访问设备。

```c
#define CWK_CDO_SYB_NAME L"\\??\\slbkcdo_3948d33e"
//...
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
    //...
    UNICODE_STRING cdo_name = RTL_CONSTANT_STRING(L"\\Device\\cwk_3948d33e");
    UNICODE_STRING cdo_syb = RTL_CONSTANT_STRING(CWK_CDO_SYB_NAME);
    //...
    
    //生成符号链接
    status = IoCreateSymbolicLink(&cdo_syb, &cdo_name);
    if(!NT_SUCCESS(status))
    {
        //删除之前生成的设备对象
        IoDeleteDevice(g_cdo);
        return status;
    }
}
```

## IoDeleteSymbolicLink

​    驱动卸载的时候应该把符号链接删除，否则符号链接会一直存在，示例代码如下

```c
#define CWK_CDO_SYB_NAME L"\\??\\slbkcdo_3948d33e"

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
    
    //...
    driver->DriverUnload = cwkUnload;
}

void cwkUnload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING cdo_syb = RTL_CONSTANT_STRING(CWK_CDO_SYB_NAME);
    ASSERT(g_cdo != NULL);
    IoDeleteSymbolicLink(&cdo_syb);
    IoDeleteDevice(g_cdo);
}
```

## Dispatch

​    驱动包含一系列分发函数，以便处理请求并返回给 Windows。分发函数是设置在驱动对象上的。不同的分发函数处理不同的请求。这里只用到 3 种请求：

- 打开（create）：试图访问一个设备对象之前，必须先成功打开，才能发送其他的请求
- 关闭（close）：结束访问一个设备对象之后，发送关闭请求将它关闭
- 设备控制（Device Control）：设备控制请求是一种既可以用来输入（从应用到内核），又可以用来输出的请求（从内核到应用）的请求

​    一个标准的分发函数原型如下：

```c
NTSTATUS cwsDispatch(
    IN PDEVICE_OBJECT dev,
    IN PIRP irp
);
```

​    其中 dev 就是请求要发送给的目标对象；irp 则代表请求内容的数据结构的指针。无论如何，分发函数必须首先设置给驱动对象，这个工作一般在 DriverEntry 中完成。相关代码示例如下：

```c
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING reg_path)
{
    //....
    ULONG i;
    for(int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i)
    {
        driver->MajorFunction[i] = cwkDispatch;
    }
    //...
}
```

​    上面的片段将所有的分发函数都设置为同一个函数，这是一种简单的处理方案。还可以为每种请求设置不同的分发函数。

## Irp handle

​    处理请求的第一步是获得请求的当前栈空间。利用当前栈空间指针来获得主功能号。每种请求都有一个主功能号来说明这是一个什么请求。

- 打开：IRP_MJ_CREATE
- 关闭：IRP_MJ_CLOSE
- 设备控制：IRP_MJ_DEVICE_CONTROL

​    请求的栈空间可以用 IoGetCurrentIrpStackLocation 取得，然后根据主功能号做不同的处理。代码如下：

```c
NTSTATUS cwkDispatch(
    IN PDEVICE_OBJECT dev,
    IN PIRP irp
)
{
    PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
    
    //判断请求发给谁，是否是之前生成的控制设备
    if(dev != g_cdo)
    //...
    	switch(irpsp->MajorFunction)//判断请求的种类
        {
                case IRP_MJ_CREATE:
                ...
                case IRP_MJ_CLOSE:
                ...
                case IRP_MJ_DEVICE_CONTROL:
                ...
                default:
        }
    //...
    
    //返回请求
    irp->IoStatus.Information = ret_len;
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}
```

​    irp->IoStatus.Information 主要用于返回输出时，用来记录这次返回到底使用了多少输出的空间。

​    irp->IoStatus.Status 用于记录这个请求的完成状态

​    IoCompleteRequest 用于结束这个请求

​    具体如何处理请求后面再说



# In App

## CreateFile

​    应用程序中打开设备和打开文件没有什么不同，除了路径有点特殊之外。

```c
#define CWK_DEV_SYM L"\\\\.\\slbkcdo_3948d33e"

int main()
{
    HANDLE device = NULL;
    ///...
    device = CreateFile(CWK_DEV_SYM, GENERIC_READ |GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, 0);
    if(device == INVALID_HANDLE_VALUE)
    {
        printf("Open device failed.\r\n");
        return -1;
    }
    else
        printf("Open device successfully.\r\n");
    ///...
}
```

​    关闭设备非常简单，调用 CloseHandle 即可

```c
CloseHandle(device);
```

##  DeviceIoControl

​    设备控制请求可以用来输入或是输出，但是需要“功能号”，下面定义了一个发送字符串的功能号

```c
#define CWK_DVC_SEND_STR (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x911, METHOD_BUFFERED, FILE_WRITE_DATA)
```

​    这里的 CTL_CODE 是一个宏，是 SDK 里的头文件提供的。要做的是直接利用这个宏来生成一个自己的设备控制请求功能号。CTL_CODE 有4个参数。

​    其中第一个参数是设备类型。由于生成的这种控制设备和任何硬件都没有关系，所以直接定义成未知类型即可。

​    第二个参数是生成这个功能号的核心数字，这个数字直接用来和其他参数“合成”功能号。0x0~0x7ff 已经被微软预留，所以只能使用0x7ff~0xfff。如果要定义超过一个的功能号，那么不同的功能号就靠这个数字进行区分。

​    第三个参数 METHOD_BUFFERED 是说用缓冲方式。用缓冲方式的话，输入/输出缓冲会在用户和内核之间拷贝。这是比较简单和安全的一种方式。

​    最后一个参数是这个操作需要的权限。当需要将数据发送到设备时，相当于往设备上写入数据，所以标志位拥有写数据权限

​    定义的另一个从内核接收刺符传的功能号如下

```c
#define CWK_DVC_RECV_STR (ULONG)CTL_CODE(FILE_DEVICE_UNKNOWN, 0x912, METHOD_BUFFERED, FILE_READ_DATA)
```

​    下面就是发送请求的过程，除了之前的打开设备和关闭设备之外，中间增加了使用 DeviceIoControl 发送请求的过程

```c
int main()
{
    ///...
    char *msg = {"Hello driver, this is a message from app.\r\n"};
    ///...
    if(!DeviceIoControl(device, CWK_DVC_SEND_STR, msg, strlen(msg) + 1, NULL, 0, &ret_len, 0))
    {
        printf("Send message failed.\r\n");
        return -2;
    }
    else
        printf("Send message successfully.\r\n");
}
```

## DeviceIoControl Handle

​    目前应用中调用 DeviceIoControl 一定会返回错误，因为内核驱动中还没处理。现在回到内核中来修改。在处理设备控制请求时，有如下任务要完成。

- 获得功能号
- 如果有输入缓冲区，则必须获得输入缓冲区的指针以及长度
- 如果有输出缓冲区，则必须获得输出缓冲区的指针以及长度

​    这些任务可以用下面的代码来完成

```c
//...
if(irpsp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
{
    PVOID buffer = irp->AssociatedIrp.SystemBuffer;//获得缓冲区
    ULONG inlen = irpsp->Parameters.DeviceIoControl.InputBufferLength;//获得输入缓冲区的长度
    ULONG outlen = irpsp->Parameters.DeviceIoControl.OutputBufferLength;//获得输出缓冲区的长度

//...
```

​    注意，缓冲区是 irp->AssociatedIrp.SystemBuffer 的前提是，这是一个缓冲方式的设备控制请求。这里只说缓冲区，没有说是输入缓冲区还是输出缓冲区，是因为在设备控制请求中，输入缓冲区和输出缓冲区是共享的，是同一个指针。

​    下面是完整的分发函数。

```c
NTSTATUS cwkDispatch(IN PDEVICE_OBJECT dev, IN PIRP irp)
{
    PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG ret_len = 0;
    if(dev == g_cdo)
    {
        if(irpsp->MajorFunction == IRP_MJ_CREATE || irpsp->MajorFunction == IRP_MJ_CLOSE)
        {
            ;//生成和关闭请求
        }
        else if(irpsp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
        {
            PVOID buffer = irp->AssociatedIrp.SystemBuffer;
            ULONG inlen = irpsp->Parameters.DeviceIoControl.InputBufferLength;
            ULONG outlen = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
            switch(irpsp->Parameters.DeviceIoControl.IoControlCode)
            {
                case CWK_DVC_SEND_STR:
                    ASSERT(buffer != NULL);
                    ASSERT(inlen > 0);
                    ASSERT(outlen == 0);
                    DbgPrint((char*)buffer);
                    break;
                case CWK_DVC_RECV_STR:
                default:
                    status = STATUS_INVALID_PARAMETER;
                    break;
            }
        }
    }
    irp->IoStatus.Information = ret_len;
    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}
```

​    这样的处理方式有一个缺点，就是没有规定缓冲区的最大长度，这可能发生缓冲区溢出漏洞被利用，使得直接从用户态获得内核态权限。因此 ，应该严格设计通信接口，避免缓冲区溢出的可能
