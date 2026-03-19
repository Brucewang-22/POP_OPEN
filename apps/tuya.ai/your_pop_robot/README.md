# your_pop_robot 工作区说明

## 概览
本工程采用“本地硬件初始化优先”模式：硬件逻辑去掉了配网，可独立调试各子模块。

## 1. 目录结构与功能
```
your_pop_robot/
├── .build                                # 构建中间目录
├── config/
│   └── LIBTECH_POP_T5AI_DECONFIG.config  # 板级配置文件
├── dist                                  # 编译产物（固件包 .bin格式）
├── include/
│   └── tuya_config.h 
├── src/
│   ├── tuya_main.c                       # 代码入口，包含硬件初始化流程
│   └── hardware/                         # 各硬件子模块
│       ├── audio/  
|       |   ├── sound_lib                 # 音频库
|       |   └── gen_audio_arrays.py       # 音频——>数组转换脚本
│       ├── imu/                         
│       ├── lcd/
|       |   └── display_lib               # 动画库
|       |       ├── left_eye   
|       |       ├── right_eye
|       |       └── display_lib/gen_lcd_arrays.py # LCD 素材构建期打包脚本
|       |                            
│       ├── microphone/                  
│       ├── moto/                        
│       └── touch/  
├── tools/                                # 硬件调试可视化脚本工具
|   ├── imu_attitude_ui.py
|   ├── mic_waveform_ui.py           
|   └── requirements.txt
├── app_default.config                    # 工程配置副本
├── build_flash.sh                        # 构建烧录脚本
├── CMakeLists.txt                                       
├── CONNECTION.md                         # 硬件接线说明 
├── Kconfig                               # 配置项定义文件
└── README.md                                
```

## 2. 关键文件说明

| 文件 | 作用 |
| --- | --- |
| `config/LIBTECH_POP_T5AI_DECONFIG.config` | 板级配置文件，硬件开关与板级选型 |
| `src/tuya_main.c` | 代码入口，包含硬件初始化流程 |
| `src/hardware/audio/sound_lib/gen_audio_arrays.py` | 音频转数组脚本。执行后会在子文件夹生成 `xxx.h`、`xxx.c`，供发音逻辑调用 |
| `src/hardware/lcd/display_lib/gen_lcd_arrays.py` | LCD 素材构建期打包脚本 |
| `tools/imu_attitude_ui.py` | IMU 数据可视化 UI，运行后可在浏览器查看 |
| `tools/mic_waveform_ui.py` | 麦克风数据可视化 UI，运行后可在浏览器查看 |
| `tools/requirements.txt` | 可视化脚本依赖清单，先执行 `pip install -r tools/requirements.txt` |
| `app_default.config` | 构建读取的工程配置副本，由 `build_flash.sh` 从 `config/` 下板级配置同步生成 |
| `build_flash.sh` | 构建烧录脚本 |
| `CMakeLists.txt` | CMake 构建入口文件 |
| `CONNECTION.md` | 硬件接线与引脚说明 |
| `Kconfig` | 配置项定义文件 |


## 3. 新增硬件标准流程

### 3.1 在 `hardware` 下新建子文件夹并写逻辑
示例：新增 `LED`

```text
src/hardware/LED/
├── LED.h
├── LED.c
└── LED.md
```

建议统一接口风格：
- `OPERATE_RET LED_init(void);`
- `OPERATE_RET LED_start(void);`
- `OPERATE_RET LED_poll(void);`
- `OPERATE_RET LED_get_status(LED_STATUS_T *status);`
- `OPERATE_RET LED_deinit(void);`

### 3.2 在构建系统接入模块
修改 `CMakeLists.txt`：
- 增加 `aux_source_directory(${APP_PATH}/src/hardware/LED LED_SRCS)`
- 增加 `list(APPEND APP_SRCS ${LED_SRCS})`
- 增加 `list(APPEND APP_INC ${APP_PATH}/src/hardware/LED)`
- 按配置开关包裹：`if (CONFIG_ENABLE_HARDWARE_LED STREQUAL "y")`

### 3.3 在 `Kconfig` 中定义开关
在 `Kconfig` 中参考上下文增加：

```kconfig
config ENABLE_HARDWARE_LED
    bool "Enable LED hardware"
    default n
    help
      Enable LED module in local hardware init flow.
```

### 3.4 在配置中打开硬件开关
在 `config/LIBTECH_POP_T5AI_DECONFIG.config` 添加：

```config
CONFIG_ENABLE_HARDWARE_LED=y
```

### 3.5 在主流程中接入调用
在 `src/tuya_main.c` 的 `app_local_hw_init()` 中按顺序接入：
- include 头文件
- `init/start/poll` 调用
- 失败日志与可选降级处理

### 3.6 编译与串口调试验证
```bash
sh build_flash.sh
python -m serial.tools.miniterm /dev/ttyACM1 460800
```

串口关键字建议：
- `[LED] init ok`
- `[LOCAL][LED] init ok`
- `[LOCAL][LED] init failed`

建议串口验证项：
- 出现模块初始化成功日志
- 无异常返回码
- 周期状态日志符合预期


## 代码示例 ##

###  `LED.h` 
```c
#ifndef __LED_H__
#define __LED_H__

#include "tuya_cloud_types.h"

typedef struct {
    BOOL_T inited;
    BOOL_T on;
} LED_STATUS_T;

OPERATE_RET LED_init(void);
OPERATE_RET LED_start(void);
OPERATE_RET LED_poll(void);
OPERATE_RET LED_set(BOOL_T on);
OPERATE_RET LED_get_status(LED_STATUS_T *status);
OPERATE_RET LED_deinit(void);

#endif
```

### `LED.c` 
```c
#include "LED.h"
#include "tal_log.h"
#include "tkl_gpio.h"

#define LED_PIN TY_GPIOA_10

static LED_STATUS_T s_led = {0};

OPERATE_RET LED_set(BOOL_T on)
{
    if (!s_led.inited) {
        return OPRT_COM_ERROR;
    }

    tkl_gpio_write(LED_PIN, on ? TUYA_GPIO_LEVEL_HIGH : TUYA_GPIO_LEVEL_LOW);
    s_led.on = on;
    return OPRT_OK;
}

OPERATE_RET LED_init(void)
{
    TUYA_GPIO_BASE_CFG_T cfg = {
        .direct = TUYA_GPIO_OUTPUT,
        .mode = TUYA_GPIO_PUSH_PULL,
        .level = TUYA_GPIO_LEVEL_LOW,
        .pull = TUYA_GPIO_PULLUP,
    };

    tkl_gpio_init(LED_PIN, &cfg);
    s_led.inited = TRUE;
    s_led.on = FALSE;

    TAL_LOGN("[LED] init ok, pin=%d", LED_PIN);
    return OPRT_OK;
}

OPERATE_RET LED_start(void)
{
    return LED_set(TRUE);
}

OPERATE_RET LED_poll(void)
{
    return OPRT_OK;
}

OPERATE_RET LED_get_status(LED_STATUS_T *status)
{
    if (status == NULL) {
        return OPRT_INVALID_PARM;
    }

    *status = s_led;
    return OPRT_OK;
}

OPERATE_RET LED_deinit(void)
{
    if (!s_led.inited) {
        return OPRT_OK;
    }

    LED_set(FALSE);
    s_led.inited = FALSE;
    return OPRT_OK;
}
```

### `Kconfig`
```kconfig
config ENABLE_HARDWARE_LED
    bool "Enable LED hardware"
    default n
    help
      Enable LED module in local hardware init flow.
```

###  `CMakeLists.txt` 接入示例
```cmake
if (CONFIG_ENABLE_HARDWARE_LED STREQUAL "y")
    aux_source_directory(${APP_PATH}/src/hardware/LED LED_SRCS)
    list(APPEND APP_SRCS ${LED_SRCS})
    list(APPEND APP_INC  ${APP_PATH}/src/hardware/LED)
endif()
```

###  `tuya_main.c` 调用示例
```c
#if defined(CONFIG_ENABLE_HARDWARE_LED) && (CONFIG_ENABLE_HARDWARE_LED == 1)
#include "LED.h"
#endif

static void app_local_hw_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if defined(CONFIG_ENABLE_HARDWARE_LED) && (CONFIG_ENABLE_HARDWARE_LED == 1)
    rt = LED_init();
    if (rt != OPRT_OK) {
        TAL_LOGE("[LOCAL][LED] init failed: %d", rt);
    } else {
        TAL_LOGN("[LOCAL][LED] init ok");
        LED_start();
    }
#endif
}
```

## 3. 配置文件注意事项

- 日常只改 `config/LIBTECH_POP_T5AI_DECONFIG.config`。
- 执行 `sh build_flash.sh`，由脚本自动同步到 `app_default.config`。
- 除非特殊编译需求，不建议手改 `app_default.config`。
