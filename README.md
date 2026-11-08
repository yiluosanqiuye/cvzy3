# 装甲板识别（静态图片检测）

RoboMaster 前哨站画面中装甲板的传统视觉检测程序：
亮度分割 → 灯条候选提取 → 灯条配对 → 装甲板外接框与有序四角点。

本阶段只处理单张静态图片，接口按"输入一张图、输出检测结果"设计，
后续可直接接入视频逐帧调用。

## 环境

| 项 | 版本 |
| --- | --- |
| 系统 | Windows 10/11 |
| 语言 | C++17 |
| 构建 | CMake ≥ 3.16 |
| 视觉库 | OpenCV 4.x（本项目使用 4.12.0） |

`CMakeLists.txt` 中的 `OpenCV_DIR` 需指向本机 OpenCV 的 install 目录。

## 编译

```bash
mkdir build && cd build
cmake ..
cmake --build .