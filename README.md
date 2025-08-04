# MP3 Stream Play Demo
## 介绍
本示例用来演示MP3在线播放。只需要RAM提供一个至多20KB的ringbuffer，即可实现MP3边下载边播放，几MB甚至十几MB大小的文件都可以支持，不需要占用FLASH空间。

基于以下示例程序修改：
- bt_pan: 提供基于蓝牙PAN的网络接入。修改点：新增mp3_dl.c文件用于MP3下载功能。
- local_music: 提供MP3播放功能。修改点：默认的文件播放改为BUFFER播放，并支持了ringbuffer。
- lv_demos: 提供播放器UI。修改点：基于music示例修改了UI，对接播放接口。

## 工程编译及下载：
由于sifli-sdk MP3播放的BUFFER是完整的文件，不支持边下载边播放，因此修改了部分文件，用于支持ringbuffer方式播放。
修改的文件在sifli-sdk目录下。
基于sifli-sdk release-v2.4分支，主要修改了audio_mp3ctrl.c文件，可以搜索MP3_RINGBUFF宏定义，比较修改点合入。

编译下载方法参考其它示例工程，所需文件在project目录下。
