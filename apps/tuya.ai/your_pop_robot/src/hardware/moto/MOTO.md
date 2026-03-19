# 模块概述
- 功能：完成双舵机上电平滑回中（boot-home）并提供回中参数配置接口。
- 层级：硬件控制层（PWM 驱动上层的运动控制封装）。

# 架构与设计
- 模块划分：
- `moto_bringup_init()`：PWM 启动 + 初始保持 + 平滑回中。
- `moto_bringup_set_home_speed()`：设置回中角速度。
- `moto_bringup_set_home_angle()`：设置通道目标角。
- 核心设计方式：
- 轨迹插值（smoothstep）+ 固定周期步进（20ms）。
- 角度→占空比映射并支持通道反向与修正 trim。

# 调用流程（重点）
- 初始化流程：
`app_local_hw_init()`  
→ `moto_bringup_init()`  
→ 对每通道执行 `__servo_pwm_init()`  
→ 短暂保持起始位  
→ 对每通道执行 `__home_one_servo()`

- 运行流程（单通道回中）：
`__home_one_servo(cfg, speed)`  
→ 计算总角度差与持续时长  
→ 按步计算 `t/ease_in_out/cur_angle`  
→ `__servo_set_angle()`  
→ `tkl_pwm_duty_set()`  
→ 末端定点 + settle 延时

# 关键接口
- `OPERATE_RET moto_bringup_init(void)`：执行一次完整 boot-home。
- `OPERATE_RET moto_bringup_set_home_speed(uint16_t speed_dps)`：设置回中速度（有上下限）。
- `OPERATE_RET moto_bringup_set_home_angle(uint8_t ch_index, int16_t angle_deg)`：设置通道目标角。

# 核心实现逻辑
- 占空比映射固定在 50Hz / 20ms 周期下的 0.5ms~2.5ms 脉宽区间。
- 使用 `3t^2-2t^3` 的 smoothstep 轨迹，降低起停冲击和端点抖动。
- 末端强制打点并短暂保持，减少“未到位”或回弹。

# 注意事项
- `MOTO_HOLD_AFTER_HOME` 决定回中后是否持续输出 PWM，涉及保持力与静音权衡。
- PWM 通道索引固定映射（0→PWM2, 1→PWM3），改板时需同步。
- 速度、角度的边界限制不要随意放宽，避免过冲或机械限位风险。
