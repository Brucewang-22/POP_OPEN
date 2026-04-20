/**
 * @file moto_bringup.c
 * @brief Servo bring-up: direct boot-home with adjustable speed and no endpoint wobble.
 */

#include "moto_bringup.h"

#include "tal_api.h"
#include "tkl_pwm.h"

#define MOTO_SERVO_FREQ_HZ             50U
#define MOTO_SERVO_PWM_CYCLE           10000U
#define MOTO_SERVO_MIN_DUTY            250U   /* 0.5ms at 20ms period */
#define MOTO_SERVO_MAX_DUTY            1250U  /* 2.5ms at 20ms period */

#define MOTO_BOOT_HOME_ANGLE_DEG_CH0   0
#define MOTO_BOOT_HOME_ANGLE_DEG_CH1   0
#define MOTO_BOOT_START_ANGLE_DEG_CH0  45
#define MOTO_BOOT_START_ANGLE_DEG_CH1  20
#define MOTO_BOOT_HOME_TRIM_DEG_CH0    3
#define MOTO_BOOT_HOME_TRIM_DEG_CH1    3
#define MOTO_LIMIT_DEG_CH0             60
#define MOTO_LIMIT_DEG_CH1             30

#define MOTO_HOME_SPEED_DPS_DEFAULT    10U
#define MOTO_HOME_SPEED_DPS_MIN        1U
#define MOTO_HOME_SPEED_DPS_MAX        360U
#define MOTO_HOME_STEP_DEG             1U
#define MOTO_STEP_PERIOD_MS            20U
#define MOTO_ENDPOINT_DEADBAND_DEG     0
#define MOTO_ENDPOINT_SETTLE_MS        120U
#define MOTO_HOLD_AFTER_HOME           1

typedef struct {
    TUYA_PWM_NUM_E pwm_id;
    int16_t        start_deg;
    int16_t        home_deg;
    int16_t        trim_deg;
    int16_t        limit_deg;
    bool           reverse;
} MOTO_SERVO_CH_CFG_T;

typedef struct {
    bool     inited;
    uint16_t home_speed_dps;
} MOTO_BRINGUP_CTX_T;

static MOTO_BRINGUP_CTX_T sg_moto_ctx = {
    .inited = false,
    .home_speed_dps = MOTO_HOME_SPEED_DPS_DEFAULT,
};

static MOTO_SERVO_CH_CFG_T sg_servo_cfg[] = {
    {.pwm_id = TUYA_PWM_NUM_2, .start_deg = MOTO_BOOT_START_ANGLE_DEG_CH0, .home_deg = MOTO_BOOT_HOME_ANGLE_DEG_CH0, .trim_deg = MOTO_BOOT_HOME_TRIM_DEG_CH0, .limit_deg = MOTO_LIMIT_DEG_CH0, .reverse = false},
    {.pwm_id = TUYA_PWM_NUM_3, .start_deg = MOTO_BOOT_START_ANGLE_DEG_CH1, .home_deg = MOTO_BOOT_HOME_ANGLE_DEG_CH1, .trim_deg = MOTO_BOOT_HOME_TRIM_DEG_CH1, .limit_deg = MOTO_LIMIT_DEG_CH1, .reverse = false},
};

#define MOTO_SERVO_CH_NUM ((uint8_t)(sizeof(sg_servo_cfg) / sizeof(sg_servo_cfg[0])))

static int16_t __clamp_angle(int16_t deg)
{
    if (deg < 0) {
        return 0;
    }
    if (deg > 180) {
        return 180;
    }
    return deg;
}

static uint32_t __angle_to_duty(int16_t deg, bool reverse)
{
    int32_t angle = __clamp_angle(deg);

    if (reverse) {
        angle = 180 - angle;
    }

    return (uint32_t)(MOTO_SERVO_MIN_DUTY +
                      ((angle * (int32_t)(MOTO_SERVO_MAX_DUTY - MOTO_SERVO_MIN_DUTY)) / 180));
}

static int16_t __clamp_angle_by_cfg(const MOTO_SERVO_CH_CFG_T *cfg, int16_t deg)
{
    int16_t min_deg;
    int16_t max_deg;

    if (cfg == NULL) {
        return __clamp_angle(deg);
    }

    min_deg = (int16_t)(cfg->home_deg - cfg->limit_deg);
    max_deg = (int16_t)(cfg->home_deg + cfg->limit_deg);

    if (min_deg < 0) {
        min_deg = 0;
    }
    if (max_deg > 180) {
        max_deg = 180;
    }
    if (deg < min_deg) {
        return min_deg;
    }
    if (deg > max_deg) {
        return max_deg;
    }
    return deg;
}

static OPERATE_RET __servo_pwm_init(const MOTO_SERVO_CH_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .polarity = TUYA_PWM_POSITIVE,
        .count_mode = TUYA_PWM_CNT_UP,
        .duty = __angle_to_duty((int16_t)(cfg->start_deg + cfg->trim_deg), cfg->reverse),
        .cycle = MOTO_SERVO_PWM_CYCLE,
        .frequency = MOTO_SERVO_FREQ_HZ,
    };

    rt = tkl_pwm_init(cfg->pwm_id, &pwm_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    return tkl_pwm_start(cfg->pwm_id);
}

static void __servo_pwm_stop_all(void)
{
    for (uint8_t i = 0; i < MOTO_SERVO_CH_NUM; i++) {
        (void)tkl_pwm_stop(sg_servo_cfg[i].pwm_id);
    }
}

static OPERATE_RET __servo_set_angle(const MOTO_SERVO_CH_CFG_T *cfg, int16_t deg)
{
    int16_t corrected = (int16_t)(__clamp_angle_by_cfg(cfg, deg) + cfg->trim_deg);
    return tkl_pwm_duty_set(cfg->pwm_id, __angle_to_duty(corrected, cfg->reverse));
}

static OPERATE_RET __home_one_servo(const MOTO_SERVO_CH_CFG_T *cfg, uint16_t speed_dps)
{
    OPERATE_RET rt = OPRT_OK;
    int32_t start = __clamp_angle_by_cfg(cfg, cfg->start_deg);
    int32_t target = __clamp_angle_by_cfg(cfg, cfg->home_deg);
    int32_t delta = target - start;
    int32_t abs_delta = (start >= target) ? (start - target) : (target - start);
    uint32_t step_delay_ms;
    uint32_t steps;

    if (abs_delta <= MOTO_ENDPOINT_DEADBAND_DEG) {
        rt = __servo_set_angle(cfg, (int16_t)target);
        if (rt != OPRT_OK) {
            return rt;
        }
        return OPRT_OK;
    }

    steps = (uint32_t)((abs_delta + (int32_t)MOTO_HOME_STEP_DEG - 1) / (int32_t)MOTO_HOME_STEP_DEG);
    if (steps == 0U) {
        steps = 1U;
    }

    step_delay_ms = ((uint32_t)MOTO_HOME_STEP_DEG * 1000U) / speed_dps;
    if (step_delay_ms < MOTO_STEP_PERIOD_MS) {
        step_delay_ms = MOTO_STEP_PERIOD_MS;
    }

    for (uint32_t i = 1; i <= steps; i++) {
        int32_t cur;

        cur = start + (int32_t)((float)delta * ((float)i / (float)steps) +
                                ((delta >= 0) ? 0.5f : -0.5f));
        rt = __servo_set_angle(cfg, (int16_t)cur);
        if (rt != OPRT_OK) {
            return rt;
        }
        tal_system_sleep(step_delay_ms);
    }

    /* Force exact home and keep holding after ramped pull-in. */
    rt = __servo_set_angle(cfg, (int16_t)target);
    if (rt != OPRT_OK) {
        return rt;
    }
    tal_system_sleep(MOTO_ENDPOINT_SETTLE_MS);
#if !MOTO_HOLD_AFTER_HOME
    (void)tkl_pwm_stop(cfg->pwm_id);
#endif
    return OPRT_OK;
}

OPERATE_RET moto_bringup_set_home_speed(uint16_t speed_dps)
{
    if (speed_dps < MOTO_HOME_SPEED_DPS_MIN || speed_dps > MOTO_HOME_SPEED_DPS_MAX) {
        return OPRT_INVALID_PARM;
    }

    sg_moto_ctx.home_speed_dps = speed_dps;
    return OPRT_OK;
}

OPERATE_RET moto_bringup_set_home_angle(uint8_t ch_index, int16_t angle_deg)
{
    if (ch_index >= MOTO_SERVO_CH_NUM) {
        return OPRT_INVALID_PARM;
    }

    sg_servo_cfg[ch_index].home_deg = __clamp_angle(angle_deg);
    return OPRT_OK;
}

OPERATE_RET moto_bringup_get_home_angle(uint8_t ch_index, int16_t *angle_deg)
{
    if (ch_index >= MOTO_SERVO_CH_NUM || angle_deg == NULL) {
        return OPRT_INVALID_PARM;
    }

    *angle_deg = sg_servo_cfg[ch_index].home_deg;
    return OPRT_OK;
}

OPERATE_RET moto_bringup_get_limit_deg(uint8_t ch_index, int16_t *limit_deg)
{
    if (ch_index >= MOTO_SERVO_CH_NUM || limit_deg == NULL) {
        return OPRT_INVALID_PARM;
    }

    *limit_deg = sg_servo_cfg[ch_index].limit_deg;
    return OPRT_OK;
}

OPERATE_RET moto_bringup_resolve_target_angle(uint8_t ch_index, int16_t logical_offset_deg,
                                              int16_t *angle_deg)
{
    int16_t unclamped_deg = 0;
    int16_t clamped_deg = 0;

    if (ch_index >= MOTO_SERVO_CH_NUM || angle_deg == NULL) {
        return OPRT_INVALID_PARM;
    }

    unclamped_deg = (int16_t)(sg_servo_cfg[ch_index].home_deg + logical_offset_deg);
    clamped_deg = __clamp_angle_by_cfg(&sg_servo_cfg[ch_index], unclamped_deg);
    *angle_deg = (int16_t)(clamped_deg + sg_servo_cfg[ch_index].trim_deg);
    return OPRT_OK;
}

OPERATE_RET moto_bringup_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_moto_ctx.inited) {
        return OPRT_OK;
    }

    for (uint8_t i = 0; i < MOTO_SERVO_CH_NUM; i++) {
        rt = __servo_pwm_init(&sg_servo_cfg[i]);
        if (rt != OPRT_OK) {
            PR_ERR("[MOTO] pwm init/start failed: ch=%d rt=%d", (int)sg_servo_cfg[i].pwm_id, rt);
            __servo_pwm_stop_all();
            return rt;
        }
    }

    /* Let PWM init settle before ramped boot-home pull-in. */
    tal_system_sleep(80);

    for (uint8_t i = 0; i < MOTO_SERVO_CH_NUM; i++) {
        rt = __home_one_servo(&sg_servo_cfg[i], sg_moto_ctx.home_speed_dps);
        if (rt != OPRT_OK) {
            PR_ERR("[MOTO] home failed: ch=%d rt=%d", (int)sg_servo_cfg[i].pwm_id, rt);
            __servo_pwm_stop_all();
            return rt;
        }
    }

    sg_moto_ctx.inited = true;
    PR_NOTICE("[MOTO] boot-home done: ch=%u speed=%u dps home=%d/%d trim=%d/%d", (unsigned)MOTO_SERVO_CH_NUM,
              (unsigned)sg_moto_ctx.home_speed_dps, (int)sg_servo_cfg[0].home_deg, (int)sg_servo_cfg[1].home_deg,
              (int)sg_servo_cfg[0].trim_deg, (int)sg_servo_cfg[1].trim_deg);
    return OPRT_OK;
}
