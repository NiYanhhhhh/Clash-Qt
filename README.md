# Clash-Qt
Clash简易客户端，提供启动clash， 管理订阅的功能
。

![主界面](https://github.com/ForeverCyril/Clash-Qt/raw/master/res/media/pic_dashboard.png)

## Build

```shell script
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make install
```
It will install into $HOME/.local/share/clash-qt by default

## Requirement

- Qt5
- yaml-cpp
