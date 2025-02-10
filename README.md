# Project 5 Device Driver

## 完成```C-core```

完成task1~4，支持两网卡互传

## 启动项目

```sh
  make all
  make run-net
```
在qemu中输入```loadbootm```启动kernel


## 上板运行

### 将镜像加载至sd卡

将sd卡插入本地电脑，运行```make floppy```

### 连接板卡并插入sd卡

运行```make minicom```连接板卡，其余步骤与qemu相同


## 实现细节

### task1

在os中执行```exec send```
本人的windows笔记本下：
wireshark-以太网端口查看接收的包

### task2

在os中执行```exec recv2 -n72 -l80 -c```
CMD中管理员模式下运行```pktRxTx.exe -m 1```，在本人windows笔记本中，选择设备7（未显示名称，可能是pktRxTx程序的bug），运行send 80，查看os接收的包

### task4

由于队友未完成c-core，因此选择在两板卡上同时运行本os进行互传。

c-core接收程序：```myrecv```，c-core发送程序```mysend```。

```mysend```会生成1024个开头为0x42的1KB垃圾包，并计算checksum值，且记录发送耗时。

```mysend```会接收包，并计算checksum，并记录接收耗时。

两板卡互传时，发送速度为1024kb/s，接收速度为180kb/s。值得一提的是接收速度的计算是计算的程序运行的总时间，包括运行程序但send程序还未发送包的时间，因此接收速度小于发送速度。


