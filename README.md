# W801 VSCode CMake template

HLK-W801 Vscode 项目模板，使用 `VSCode + CMake + Ninja` 构建

本处使用的开发板为 `HLK-W801-KIT-V1.1`

> 注意：该仓库内的 SDK 有一定的改动，可能与官方的有出入

## 安装

- 安装 CMake v3.20+ 以及 Ninja

  - 打开系统的 cmd，执行 `cmake --help` 和 `ninja --help` 检查是否已安装完成

- 克隆源码，在 VSCode 中打开工程文件夹：

  - 在当前工作区打开终端，执行 `git submodule init` 和 `git submodule update`，拉取子模块（**此步骤要从 github 克隆仓库，请确保网络通畅**）

- 安装 csky 编译器（系统中已安装则可忽略该步骤）：

  - 打开工程的目录：`sdk/tools/w800/toolchain/`

  - 将 `csky-elfabiv2-tools-mingw-minilibc-20210423.7z` 编译器解压，将解压后的目录移动到你的磁盘其他位置上

  - 打开系统属性->高级->环境变量设置，将解压后编译器的路径 `xxx\xx\csky-elfabiv2-tools-mingw-minilibc-20210423\bin` 设置到环境变量 `Path` 中

  - 保存设置，之后关闭所有 VSCode 实例

- 再次打开 VSCode，打开工程的目录，在终端中执行 `csky-elfabiv2-gcc -v` 检查编译器是否已安装

- 安装 VSCode 插件：`CMake Tools`

***

## 编译

- 打开 CMake Tools 的工具栏

- 选择 `Configure All Projects` 生成配置

- 选择 `w801.fls` 为构建目标

  ![](./doc/setup_cmake_target.png)

- 之后，点击旁边的 `build` 图标启动构建

- 编译完成后，工程根目录下会生成一个 `bin` 文件夹，固件将存放在其中

  ![](./doc/build_done.png)

***

## 烧录

- 按下 `ctrl+shift+B` 打开可用任务，选择 `flash (select serialport)` 回车，之后输入串口名称，回车启动烧录

  ![](./doc/flash.png)

- 烧录结束后，将打印如下 log：

  ![](./doc/run.png)

- 由于程序烧录完后立即开始运行，而烧录器串口打开较慢，因此 log 有些缺失，按下复位键重启程序即可看到完整 log：

  ![](./doc/run2.png)


本例程中，默认开启固件加密以及数字签名，上电后将校验固件签名
