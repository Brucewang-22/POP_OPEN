# 模块概述
- 功能：提供外置触摸开关（GPIO 数字电平）的多通道抽象与读取接口。
- 层级：硬件输入驱动适配层。

# 架构与设计
- 模块划分：
- `touch_sensor_init_channel()`：单通道初始化与参数登记。
- `touch_sensor_read_channel()`：读取触摸状态。
- `touch_sensor_deinit()`：释放所有通道。
- 兼容接口：`touch_sensor_init/read/get_pin`（通道 0 封装）。
- 核心设计方式：
- 每通道保存独立配置（GPIO、有效电平、上拉下拉）。
- 运行时按电平与 `active_level` 比较得出触摸状态。

# 调用流程（重点）
- 初始化流程：
`app_local_hw_init()`  
→ 循环 `touch_sensor_init_channel(ch, cfg)`  
→ `tkl_gpio_init()`  
→ 标记通道 `inited`

- 运行流程：
`touch poll 定时器`  
→ `touch_sensor_read_channel(ch, &touched)`  
→ `tkl_gpio_read()`  
→ 比较 `lv == active_level`  
→ 上层输出状态变化日志

# 关键接口
- `OPERATE_RET touch_sensor_init_channel(uint8_t ch, const TOUCH_SENSOR_CFG_T *cfg)`：初始化指定通道。
- `OPERATE_RET touch_sensor_read_channel(uint8_t ch, bool *touched)`：读取指定通道触摸状态。
- `OPERATE_RET touch_sensor_deinit(void)`：反初始化所有通道。
- `TUYA_GPIO_NUM_E touch_sensor_get_pin_channel(uint8_t ch)`：获取已注册引脚。

# 核心实现逻辑
- 初始化时将通道配置复制进 `sg_cfg[ch]`，后续读取只依赖缓存配置。
- 输入模式下将 `gpio_cfg.level` 置为与有效电平相反，减少初态误判。
- 状态判断完全基于电平比较，逻辑简单可预测，便于上层做边沿检测。

# 注意事项
- 本模块针对外置触摸模块数字输出，不是 T5 内置 touch 测试通道。
- `TOUCH_SENSOR_CH_MAX=3`，新增通道需同步修改数组与调用侧循环。
- 未初始化通道读取会返回参数错误，上层需先完成 init 再进入轮询。
