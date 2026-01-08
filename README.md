# MP3 Stream Play Demo
## 介绍
本示例用来演示MP3在线播放。只需要RAM提供一个至多20KB的ringbuffer，即可实现MP3边下载边播放，几MB甚至十几MB大小的文件都可以支持，不需要占用FLASH空间。

ringbuffer大小默认16KB。buffer越大，网络不稳定导致卡顿的概率越小。网络足够好的情况下，buffer大约6~8KB就够了。

支持在线歌单，实现原理主要参考了listen1_chrome_extension项目。目前只支持了一个音乐平台，其它音乐平台原理类似。

基于以下示例程序修改：
- bt_pan:       提供基于蓝牙PAN的网络接入。
- local_music:  提供MP3播放功能。           修改点：默认的文件播放改为BUFFER播放，并支持了ringbuffer。
- lv_demos:     提供播放器UI。              修改点：基于music示例修改了UI，对接播放接口。

主要功能实现：
- mp3online:
    - mp3_dl.c:         MP3下载功能。
    - mp3_playlist.c:   获取播放列表功能。
    - mp3_song.c:       获取歌曲信息功能。
    - mp3_jpg.c:        jpeg图片下载显示相关功能。
    - mp3_lyric.c:      歌词下载和显示功能。

## 工程编译及下载：
**注意：如遇编译不通过，请重点看此节说明。**

- 本项目使用的sifli-sdk版本为release/v2.4分支的v2.4.2。
- 本项目的sifli-sdk目录下是需要修改的文件，可自行与完整的sifli-sdk对比合入。
- 由于sifli-sdk MP3播放的BUFFER是完整的文件，不支持边下载边播放，因此修改了部分文件，用于支持ringbuffer方式播放。主要修改了audio_mp3ctrl.c文件，可以搜索MP3_RINGBUFF宏定义，比较修改点合入。
- 由于sifli-sdk 默认不支持动态下载的jpeg图片解码。因此修改了lv_gpu.c文件，可以搜索LV_GPU_SOFT_DECODER宏定义，比较修改点合入。

具体编译下载方法参考sifli-sdk其它示例工程，所需文件在project目录下。

## 免责声明：
本项目代码仅供学习和研究使用，禁止用于任何商业用途。
作者不对因使用本项目代码导致的版权纠纷、技术故障或商业损失承担任何责任。
