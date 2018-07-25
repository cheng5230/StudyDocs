# RTOS（实时操作系统）类操作系统 #

包含UCOS 、FreeRTOS、 RTX、 RT-Thread 

1.UCOSII、UCOSIII
> <font color=red> (Micro-Controller Operating System Two)微控制操作系统，是一个可以基于ROM运行的、可裁剪的、抢占式、实时多任务内核，具有高度可移植性，特别适合于微处理器和控制器.</font> 

2.FreeRTOS 
> 免费实时操作系统； 许多半导体厂商，尤其是WIFI、蓝牙这些厂商使用它；文件数量少，要比USOS少的多；社会占有量在2015排到第一位。内核占用4-9k字节的空间。

3.FreeRTOS配置信息

  1）"INCLUDE_"开头的宏用来表示使能或除能FreeRTOS中相应的API函数

  2）"config" 开始的宏和 "INCLUDE_"开始的宏一样，用来完成FreeRTOS的配置和裁剪

  3）"configASSERT" 类似C标准库的 assert()函数 ,断言会增加内存开销，调试完成之后会尽量去掉断言，以减小内存开销；调试时检查传入的参数是否合理；FreeRTOS会调用 configASSERT(x),当x为0的时候说明有错误发生，会调用vAssetCalled(__FILE__,__LINE__), vAssetCalled函数需要用户自行定义（可以显示或打印错误信息）。

4.多任务系统
  
  单任务系统也称作为前后台系统，中断服务函数作为前台程序，大循环(while)作为后台程序.
  <font color=red>单任务系统的缺点：系统的实时性差,各个任务/应用程序都是排队等着轮流执行。</font>
  ![](https://i.imgur.com/MzvipFa.jpg)

多任务系统会把一个大问题(应用)“分而治之”，把大问题划分成很多个小问题，逐步的把小问题解决掉，这些小问题可以单独的作为一个小任务来处理。
<font color=red>多任务系统优点：高优先级任务可以打断低优先级任务的运行而取得CPU的使用权，这就是抢占式实时多任务系统。</font>

![](https://i.imgur.com/AzAlA4g.jpg)

任务的特性：简单没有使用限制，支持抢占，支持优先级，会开销增加使用RAM量

任务的状态：运行、就绪、阻塞、挂起。

![](https://i.imgur.com/cN9lBtv.jpg)

<font color=red> FreeRTOS任务优先级数字越低表示任务优先级越低，0表示优先级越低。

UCOSIII任务优先级数字越低表示任务优先级越高，0表示优先级最高。</font>

5.时间管理

UCOSIII中的延时函数 OSTimeDly()可以设置为三种模式：相对模式、周期模式和绝对模式。

FreeRTOS中的延时函数只有绝对和相对模式；vTaskDelay()是相对模式，vTaskDelayUntil()是绝对模式。
<font color=red> vTaskDelayUntil 会阻塞任务，阻塞时间是一个绝对时间，那些需要一定频率运行的任务可以调用此函数进行处理。</font>

软件定时器：FreeRTOS提供了软件定时器功能，不过定时精度没有MCU自带的硬件定时器精度高。

<font color=red> 软件定时器有定时服务任务来提供的； 软件定时器分为单次定时器和周期定时器；复位软件定时器，即是清空定时器计数，定时器重新开始计数。</font>


