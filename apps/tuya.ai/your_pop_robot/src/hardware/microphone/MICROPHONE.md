# 模块概述
- 功能：统计麦克风数据的一秒窗口帧数与平均 RMS（permille）。
- 层级：应用统计层（AI 事件流之上的健康/观测模块）。

# 架构与设计
- 模块划分：
- `mic_bringup_init()`：初始化上下文与互斥锁。
- `mic_bringup_on_ai_event()`：消费 MIC 数据事件并累积窗口统计。
- `mic_bringup_get_status()`：读取窗口统计结果。
- 核心设计方式：
- 滚动 1 秒桶统计 + 互斥保护。
- RMS 采用整数算法（平方和 + 整数开方）降低浮点依赖。

# 调用流程（重点）
- 初始化流程：
`app_local_hw_init()`  
→ `mic_bringup_init()`  
→ 创建锁、设置窗口起点

- 运行流程：
`AI 事件回调`  
→ `mic_bringup_on_ai_event(event)`  
→ 过滤 `AI_USER_EVT_MIC_DATA`  
→ `__calc_rms_permille()`  
→ `__mic_roll_window()`  
→ 累加 `frames_window/rms_sum_window/rms_cnt_window`

`状态读取`  
→ `mic_bringup_get_status()`  
→ 滚动窗口  
→ 输出 `frames_1s` 与平均 `rms_permille`

# 关键接口
- `OPERATE_RET mic_bringup_init(void)`：初始化统计上下文，幂等。
- `void mic_bringup_on_ai_event(AI_NOTIFY_EVENT_T *event)`：喂入麦克风事件数据。
- `OPERATE_RET mic_bringup_get_status(uint32_t *frames_1s, uint32_t *rms_permille)`：读取一秒窗口统计。

# 核心实现逻辑
- 16-bit PCM 按样本计算均方根，输出 0~1000 的 permille 归一化指标。
- 当时间跨越 >=1 秒时，窗口前移并清零累积，保证统计时效性。
- 事件处理和状态读取共享同一上下文，靠 mutex 保证并发一致性。

# 注意事项
- 该模块只做统计，不负责音频采集链路启动。
- 输入数据默认按 16-bit little-endian PCM 解释，源格式变化需同步修改。
- 大时间跳变时窗口会直接跳到最新秒边界，历史桶不会回补。
