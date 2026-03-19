# 模块概述
- 功能：基于 MPU6050 + Mahony 算法输出姿态角（roll/pitch/yaw）与惯导状态。
- 层级：硬件算法中间层（传感器驱动之上、应用轮询之下）。

# 架构与设计
- 模块划分：
- `imu_ahrs_init()`：设备初始化、WHO_AM_I 检查、陀螺仪静态标定。
- `imu_ahrs_poll()`：采样 + 去偏 + Mahony 更新 + 状态刷新。
- `imu_ahrs_get_status()`：读出快照状态。
- 核心设计方式：
- 定时轮询 + 状态缓存。
- 启动阶段固定样本数陀螺零偏标定（100 次）。

# 调用流程（重点）
- 初始化流程：
`app_local_hw_init()`  
→ `imu_ahrs_init()`  
→ `mpu6050_init()`  
→ `mpu6050_read_who_am_i()`  
→ `mahony_init()`  
→ `__gyro_calibrate()`

- 运行流程：
`IMU 定时器(5ms)`  
→ `imu_ahrs_poll()`  
→ `mpu6050_read_sample()`  
→ 偏置补偿 + 单位换算  
→ `mahony_update_imu()`  
→ `mahony_get_euler_deg()`  
→ 更新 `sg_imu_ctx.status`

`IMU 状态定时器(20ms)`  
→ `imu_ahrs_get_status()`  
→ 上层日志/业务消费

# 关键接口
- `OPERATE_RET imu_ahrs_init(void)`：初始化与标定入口，幂等。
- `OPERATE_RET imu_ahrs_poll(void)`：单次姿态更新，供周期任务调用。
- `OPERATE_RET imu_ahrs_get_status(IMU_AHRS_STATUS_T *status)`：获取最新状态快照。

# 核心实现逻辑
- `dt` 动态计算，超过阈值回退默认 `0.02s`，抑制时钟抖动对姿态积分影响。
- 陀螺仪采用启动期平均值做 bias，运行期直接减偏后送 Mahony。
- 状态同时保留角度、温度、原始 g/dps 与 m/s² 量，便于调试与融合扩展。

# 注意事项
- 标定阶段要求传感器静止，抖动会直接污染零偏。
- 该实现未做磁力计融合，`yaw` 仅由陀螺积分与加计约束间接稳定，长期可能漂移。
- 修改采样周期时要同步评估 `IMU_POLL_TIME` 与滤波参数匹配性。
