# 模块概述
- 功能：提供 GC9D01 双屏基础注册与填充控制能力，并承载显示资源生成脚本。
- 层级：驱动适配层 + 资源构建辅助层。

# 架构与设计
- 模块划分：
- `lcd_gc9d01.c/.h`：双实例设备注册与基础填充 API。
- `display_lib/`：LCD 资源脚本与生成物（左右眼素材）。
- 核心设计方式：
- 每个屏实例独立命名、独立打开、按 RGB565 帧缓冲刷新。
- 资源通过构建期脚本生成 C 数组，运行期直接引用。

# 调用流程（重点）
- 初始化流程（驱动层）：
`lcd_gc9d01_init(instance, cfg)`  
→ 组装 `DISP_SPI_DEVICE_CFG_T`  
→ `tdd_disp_spi_gc9d01_register(name, &display_cfg)`  
→ 标记实例 `inited`

- 运行流程（基础填充）：
`lcd_gc9d01_fill_color()/turn_on()/clear()`  
→ `__lcd_open_dev()`  
→ 分配 160x160 RGB565 缓冲  
→ 填充颜色  
→ `tdl_disp_dev_flush()`  
→ 释放缓冲并关闭设备

- 资源流程（构建期）：
`gen_lcd_arrays.py`  
→ 扫描 `display_lib/*/*/*.jpg`  
→ 转换/打包为 `*_lcd_assets.c/.h`

# 关键接口
- `OPERATE_RET lcd_gc9d01_init(LCD_INSTANCE_E instance, LCD_GC9D01_CFG_T *cfg)`：注册单个 LCD 实例。
- `OPERATE_RET lcd_gc9d01_fill_color(LCD_INSTANCE_E instance, uint16_t color_rgb565)`：整屏填色。
- `OPERATE_RET lcd_gc9d01_turn_on()/lcd_gc9d01_clear()`：白屏/黑屏快捷接口。
- `OPERATE_RET lcd_gc9d01_deinit(LCD_INSTANCE_E instance)`：逻辑反初始化标记。

# 核心实现逻辑
- 使用 `LCD_INSTANCE_0/1` 映射两块屏的独立名字，避免句柄混用。
- 填色走临时帧缓冲，接口简单但每次都会分配/释放内存，偏功能验证场景。
- 对外只暴露“屏实例 + SPI 参数”最小集合，硬件绑定逻辑集中在注册阶段。

# 注意事项
- `lcd_gc9d01_deinit()` 仅清状态，不做设备注销；真实电源/总线状态由下层管理。
- `__lcd_fill()` 每次动态分配 160x160x2 缓冲，频繁调用会增加内存压力。
- 当前工程主显示链路在 `tuya_main.c` 的本地动画路径，`lcd_gc9d01` 更偏通用能力封装。
