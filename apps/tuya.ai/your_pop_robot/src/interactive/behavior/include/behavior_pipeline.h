/**
 * @file behavior_pipeline.h
 * @brief Interactive behavior pipeline (init phase + interaction phase).
 */
#pragma once

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialization phase.
 *        Run output combo without input trigger:
 *        lcd model1 + audio model1 + moto model1.
 */
OPERATE_RET behavior_pipeline_init_phase(void);

/**
 * @brief Interaction phase placeholder (to be extended later).
 */
OPERATE_RET behavior_pipeline_interaction_phase(void);

#ifdef __cplusplus
}
#endif

