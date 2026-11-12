# 装甲板识别（静态图片检测）

RoboMaster 前哨站画面中装甲板的传统视觉检测程序：

```
亮度分割 → 灯条候选提取 → 灯条配对 → 装甲板外接框 + 中心点 + 有序四角点
```

本阶段处理单张静态图片，但检测流程已封装成可重复调用的**单帧函数**，
下一阶段接入视频时只需套一层循环即可。

---

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
```

或在 CLion 中直接 Build（`Ctrl + F9`）。

## 运行

必须在**项目根目录**下运行，因为程序使用相对路径：

```bash
# 使用默认图片 images/outpost_frame.png
armor_detector.exe

# 指定其他图片
armor_detector.exe images/other.png
```

运行后弹出 `image` 和 `result` 两个窗口，按任意键退出。

## 输出文件

运行后在 `output/` 目录生成：

| 文件 | 内容 |
| --- | --- |
| `input.png` | 处理用的输入图 |
| `target_mask.png` | 亮度阈值分割得到的原始 mask |
| `mask_close.png` | 形态学处理后的 mask |
| `lights.png` | 灯条候选图（旋转矩形 + 中心点 + 编号） |
| `result.png` | 最终结果图（外接框 + 四角点编号 + 中心点） |

---

## 项目结构

代码按三层组织，各层的职责互不重叠：

| 层 | 文件 | 职责 |
| --- | --- | --- |
| 流程层 | `main.cpp` | 读图、调用检测、保存调试图、显示结果 —— **只做 I/O 与展示** |
| 算法层 | `detect_armors` / `Photo` | 分割、轮廓、灯条、配对、装甲板 —— **纯计算，不写文件** |
| 数据层 | `Light` / `Armor` | 灯条与装甲板对象，构造即完整 |

关键约束：**算法层没有任何 `imwrite`**，是否落盘由调用方决定。
这样接视频时可以逐帧调用而不会产生磁盘 I/O 开销。

---

## 单帧检测接口

```cpp
// 单帧检测：输入一张彩色图，返回装甲板候选列表
vector<Armor> detect_armors(const Mat &image, DetectDebug *debug = nullptr);
```

| 参数 / 返回值 | 说明 |
| --- | --- |
| `image` | 输入彩色图（RGB/BGR 三通道） |
| `debug` | **可选**输出，传 `nullptr` 或省略表示不需要调试图 |
| 返回值 | 装甲板候选列表 `vector<Armor>` |

```cpp
// 需要调试图（静态图模式）
DetectDebug dbg;
vector<Armor> armors = detect_armors(image, &dbg);
imwrite("output/target_mask.png", dbg.mask);
imwrite("output/mask_close.png",  dbg.mask_morphology);

// 不需要调试图（视频逐帧模式）
vector<Armor> armors = detect_armors(image);
```

`DetectDebug` 结构体：

```cpp
struct DetectDebug {
    Mat           mask;             // 原始阈值 mask
    Mat           mask_morphology;  // 形态学后的 mask
    vector<Light> lights;           // 灯条候选
};
```

`Armor` 提供的信息：

| 字段 | 含义 |
| --- | --- |
| `color` | 颜色（`red` / `blue` / `unknown`） |
| `rect` | 装甲板旋转矩形 |
| `center` | 中心点 |
| `corners[4]` | 四个角点，顺序见下文 |
| `left_light_id` / `right_light_id` | 构成该装甲板的左右灯条编号 |
| `valid` | 是否有效 |
| `print_information()` | 打印上述信息 |

**下一阶段接入视频时**，只需：

```cpp
while (cap.read(frame)) {
    Mat image;
    flip(frame, image, 0);                    // 翻正
    vector<Armor> armors = detect_armors(image);   // ← 一行
    // 绘制、显示……
    waitKey(1);                                // 注意不能用 waitKey(0)
}
```

---

## 检测流程

1. **预处理**：`cvtColor` 转灰度 → `GaussianBlur(3×3)` → `threshold` 阈值 200
2. **形态学**：`Size(1,3)` 竖向闭运算，补齐灯条内部断口（横向不动，避免细灯条被腐蚀变短）
3. **灯条候选**：`findContours` → 面积 `30 ~ 2000` → 长宽比 ≥ 3.0 → `minAreaRect`
4. **颜色判定**：在灯条旋转矩形的外接框内采样原彩色图，跳过 `S ≤ 60` 或 `V ≤ 100`
   的像素（过曝白芯与暗背景），取色相 H 的中位数；
   `H < 30` 或 `H > 160` 判红，`100 ~ 130` 判蓝
5. **配对**：需同时满足
   - 已知颜色一致（`unknown` 视为无法判断，不参与比较）
   - 长度比在 `0.6 ~ 1.6`
   - 长轴倾角之差 ≤ 20°
   - 中心上下偏差 ≤ 0.5 × 平均灯条长
   - 水平间距在 `1.5 ~ 6` 倍平均灯条长之间
6. **装甲板**：8 个灯条顶点求 `minAreaRect`，构造有序四角点与中心点
7. **有效性**：四角点均在图像内、矩形面积 > 0、长宽比在 `1.5 ~ 6`
8. **去重**：两块装甲板中心距离 < 0.3 × 板宽时视为同一块，只保留一个

配对条件全部使用**相对量**（除以灯条长度），不依赖固定像素距离，
因此目标远近变化时仍然成立。

---

## 四角点顺序约定

```
0 左上 → 1 右上 → 2 右下 → 3 左下        （图像坐标，y 向下）
```

顺序由**构造**保证，而不是对顶点排序：

1. 由 `RotatedRect` 的角度得到宽边方向 `u = (cosθ, sinθ)`
2. 取垂直方向 `v`，按尺寸判断哪条是长轴
3. 强制长轴指向下方，消除"同一方向两种表示"的 180° 歧义
4. 从中心按 `中心 ± u·半宽 ± v·半高` 四种组合直接算出四角

因为不依赖坐标大小排序，目标倾斜时语义也不会翻转，
可直接作为下一阶段 `solvePnP` 的图像点输入。

结果图上会标出 `0 1 2 3` 用于检查顺序。

---

## 异常处理

| 情况 | 行为 |
| --- | --- |
| 图片读取失败 | 打印路径与提示，返回码 1，不继续处理空图像 |
| `output/` 不存在 | 启动时自动创建（`filesystem::create_directories`） |
| 未检测到装甲板 | 打印"未检测到装甲板"，正常结束 |
| 只有一个灯条候选 | 无法配对，输出 0 个候选，不崩溃 |

---

## 已知局限

- **亮度阈值固定为 200**，依赖当前光照；目标过远或画面过暗时可能漏检
- **只保留了灯条白芯**：灰度阈值只取最亮部分，实测灯条宽度约 5~6 px（真实 8~9 px），
  `minAreaRect` 的角度估计存在少量噪声，装甲板框比真实尺寸略小
- **侧视会漏检**：机器人在转动时装甲板接近侧视，只能看到一根灯条，单帧无法配对
- **预处理含一次垂直翻转**（`flip(0)`），前提是视频帧上下颠倒；换用其他来源的图片时需确认朝向
- **单帧无跟踪**：帧与帧之间没有关联，目标短暂丢失后无法靠历史信息恢复
