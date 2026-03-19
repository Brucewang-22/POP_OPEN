# 模块概述
- 功能：完成本地音频链路拉起并播放一次内置 MP3 测试音。
- 层级：应用硬件适配层（介于 `tuya_main` 与音频驱动/播放器之间）。

# 架构与设计
- 模块划分：
- `local_audio_start_test()`：外部入口，做设备/播放器初始化与播放任务启动。
- `__local_audio_play_once_task()`：后台线程，按分块流式送音频数据。
- 核心设计方式：
- 线程 + 流式推送（`AI_AUDIO_PLAYER_TTS_START/DATA/STOP`）。
- 板级功放强制使能（P39 拉高）确保本地模式可发声。

# 调用流程（重点）
- 初始化流程：
`app_local_hw_init()`  
→ `local_audio_start_test()`  
→ `tdl_audio_find()`  
→ `tdl_audio_open()`  
→ `ai_audio_player_init()`  
→ `ai_audio_player_set_vol(70)`  
→ `tkl_gpio_write(P39, HIGH)`  
→ `tal_thread_create_and_start(audio_once)`

- 运行流程：
`audio_once` 线程  
→ `ai_audio_play_tts_stream(START)`  
→ 循环 `ai_audio_play_tts_stream(DATA, 2048 bytes)`  
→ `tal_system_sleep(20ms)`  
→ `ai_audio_play_tts_stream(STOP)`

# 关键接口
- `OPERATE_RET local_audio_start_test(void)`：
拉起本地音频链路并启动一次播放线程；在本地硬件初始化阶段调用。

# 核心实现逻辑
- 音源来自构建期生成的 `test_audio_assets`（内置数组），不依赖文件系统读取。
- 采用 2048 字节分块推送，避免一次性喂入导致缓冲压力或截断。
- 先硬开功放再播放，减少“播放器启动成功但无声”的板级状态差异。

# 注意事项
- `P39=HIGH` 依赖当前板级功放极性定义；若更换板卡需同步确认极性。
- `audio_once` 是“一次播放”逻辑，不会自动循环。
- `AUDIO_CODEC_NAME` 依赖板级注册，未注册会导致 `tdl_audio_find/open` 失败。
