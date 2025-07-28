# LVGL v8 Demos
## 介绍
本示例用来演示LVGL V8的官方示例，使用官方提供的demo应用程序。
可以使用menuconfig来选择演示的demo应用程序。包含的应用程序有：
- Show some widget 演示lvgl widget的使用
- Demonstrate the usage of encoder and keyboard 演示键盘
- Benchmark your system 演示benchmark
- Stress test for LVGL 压力测试
- Music player demo 演示音乐播放

## 工程编译及下载：
板子工程在project目录下可以通过指定board来编译适应相对board的工程，
- 比如想编译可以在HDK 563上运行的工程，执行scons --board=eh-lb563即可生成工程
- 下载可以通过build目录下的download.bat进行，比如同样想烧录上一步生成的563工程，可以执行.\build_eh-lb563\download.bat来通过jlink下载
- 特别说明下，对于SF32LB52x/SF32LB56x系列会生成额外的uart_download.bat。可以执行该脚本并输入下载UART的端口号执行下载
模拟器工程在simulator目录下，
- 使用 scons 进行编译，simulator/msvc_setup.bat文件需要相应修改，和本机MSVC配置对应
- 也可以使用 scons --target=vs2017 生成 MSVC工程 project.vcxproj, 使用Visual Studio 进行编译。

```{note}
如果不是使用VS2017, 例如 VS2022, 加载工程的时候，会提示升级MSVC SDK, 升级后就可以使用了。
```
            
      
