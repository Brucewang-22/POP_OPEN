/**
 * @file moto_bringup.c
 * @brief Servo bring-up: smooth boot-home with adjustable speed and no endpoint wobble.
 */

#include "moto_bringup.h"

#include "tal_api.h"
#include "tkl_pwm.h"

#define MOTO_SERVO_FREQ_HZ             50U
#define MOTO_SERVO_PWM_CYCLE           10000U
#define MOTO_SERVO_MIN_DUTY            250U   /* 0.5ms at 20ms period */
#define MOTO_SERVO_MAX_DUTY            1250U  /* 2.5ms at 20ms period */

#define MOTO_BOOT_START_ANGLE_DEG      45
#define MOTO_BOOT_HOME_ANGLE_DEG_CH0   0
#define MOTO_BOOT_HOME_ANGLE_DEG_CH1   0
#define MOTO_BOOT_HOME_TRIM_DEG_CH0    3
#define MOTO_BOOT_HOME_TRIM_DEG_CH1    3

#define MOTO_STEP_PERIOD_MS            20U    /* one PWM period */
#define MOTO_HOME_SPEED_DPS_DEFAULT    30U
#define MOTO_HOME_SPEED_DPS_MIN        10U
#define MOTO_HOME_SPEED_DPS_MAX        360U
#define MOTO_ENDPOINT_DEADBAND_DEG     0
#define MOTO_ENDPOINT_SETTLE_MS        120U
#define MOTO_HOLD_AFTER_HOME           1

typedef struct {
    TUYA_PWM_NUM_E pwm_id;
    int16_t        start_deg;
    int16_t        home_deg;
    int16_t        trim_deg;
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
    {.pwm_id = TUYA_PWM_NUM_2, .start_deg = MOTO_BOOT_START_ANGLE_DEG, .home_deg = MOTO_BOOT_HOME_ANGLE_DEG_CH0, .trim_deg = MOTO_BOOT_HOME_TRIM_DEG_CH0, .reverse = false},
    {.pwm_id = TUYA_PWM_NUM_3, .start_deg = MOTO_BOOT_START_ANGLE_DEG, .home_deg = MOTO_BOOT_HOME_ANGLE_DEG_CH1, .trim_deg = MOTO_BOOT_HOME_TRIM_DEG_CH1, .reverse = false},
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

static OPERATE_RET __servo_pwm_init(const MOTO_SERVO_CH_CFG_T *cfg)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .polarity = TUYA_PWM_POSITIVE,
        .count_mode = TUYA_PWM_CNT_UP,
        .duty = __angle_to_duty(cfg->start_deg, cfg->reverse),
        .cycle = MOTO_SERVO_PWM_CYCLE,
        .frequency = MOTO_SERVO_FREQ_HZ,
    };

    rt = tkl_pwm_init(cfg->pwm_id, &pwm_cfg);
    if (rt != OPRT_OK) {
        return rt;
    }

    rt = tkl_pwm_start(cfg->pwm_id);
    if (rt != OPRT_OK) {
        return rt;
    }

    return OPRT_OK;
}

static void __servo_pwm_stop_all(void)
{
    for (uint8_t i = 0; i < MOTO_SERVO_CH_NUM; i++) {
        (void)tkl_pwm_stop(sg_servo_cfg[i].pwm_id);
    }
}

static OPERATE_RET __servo_set_angle(const MOTO_SERVO_CH_CFG_T *cfg, int16_t deg)
{
    int16_t corrected = (int16_t)(deg + cfg->trim_deg);
    return tkl_pwm_duty_set(cfg->pwm_id, __angle_to_duty(corrected, cfg->reverse));
}

static float __ease_in_out(float t)
{
    /* smoothstep: 3t^2 - 2t^3 */
    return t * t * (3.0f - 2.0f * t);
}

static OPERATE_RET __home_one_servo(const MOTO_SERVO_CH_CFG_T *cfg, uint16_t speed_dps)
{
    OPERATE_RET rt = OPRT_OK;
    int32_t start = __clamp_angle(cfg->start_deg);
    int32_t target = __clamp_angle(cfg->home_deg);
    int32_t delta = target - start;
    int32_t abs_delta = (delta >= 0) ? delta : -delta;
    uint32_t duration_ms;
    uint32_t steps;

    if (abs_delta <= MOTO_ENDPOINT_DEADBAND_DEG) {
        rt = __servo_set_angle(cfg, (int16_t)target);
        if (rt != OPRT_OK) {
            return rt;
        }
#if !MOTO_HOLD_AFTER_HOME
        (void)tkl_pwm_stop(cfg->pwm_id);
#endif
        return OPRT_OK;
    }

    duration_ms = ((uint32_t)abs_delta * 1000U) / speed_dps;
    if (duration_ms < MOTO_STEP_PERIOD_MS) {
        duration_ms = MOTO_STEP_PERIOD_MS;
    }

    steps = duration_ms / MOTO_STEP_PERIOD_MS;
    if (steps == 0) {
        steps = 1;
    }

    for (uint32_t i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float e = __ease_in_out(t);
        int32_t cur = start + (int32_t)((float)delta * e + ((delta >= 0) ? 0.5f : -0.5f));

        rt = __servo_set_angle(cfg, (int16_t)cur);
        if (rt != OPRT_OK) {
            return rt;
        }
        tal_system_sleep(MOTO_STEP_PERIOD_MS);
    }

    /* Force exact endpoint once, keep briefly to settle, then stop PWM. */
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

    /* Keep initial pose briefly, then go home smoothly. */
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
