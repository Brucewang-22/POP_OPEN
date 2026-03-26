# AGENT_DEV

## 目的
用于约束 AGENT 在 `POP_OPEN` 工程内的开发流程、阅读顺序与文件读写权限。

## 强制阅读顺序（按序执行）
1. 阅读 `POP_OPEN/agentme/SDK_AGENT_README.md`
   - 目标：明确整个 SDK 架构、驱动、API、硬件引脚定义等。

2. 阅读 `POP_OPEN/rules/DEVELOPMENT_README.md`
   - 目标：明确整个工程的开发流程。

3. 阅读 `POP_OPEN/rules/PROMPT_ENGINEERING.md`
   - 目标：明确该文件是开发完毕后，为 `hardware` 目录下硬件生成说明文档的依据。

4. 阅读 `POP_OPEN/apps/tuya.ai/your_pop_robot/README.md`
   - 目标：明确 `your_pop_robot` 应用架构与开发流程。
   - 说明：`your_pop_robot` 架构借鉴同级 `your_otto_robot`，可参考其实现思路；但不允许相互调用，避免逻辑交叉粘连。

5. 阅读 `POP_OPEN/apps/tuya.ai/your_pop_robot/CONNECTION.md`
   - 目标：明确该文档是 GPIO 映射唯一依据。
   - 约束：硬件接线必须按此文档执行，并按引脚定义进行交叉排查，确认无冲突。

6. 阅读 `POP_OPEN/rules/PROMPT_ENGINEERING.md`
   - 目标：再次确认该文件是开发完毕后，为 `hardware` 目录下硬件生成说明文档的依据。

7. 阅读 `POP_OPEN/rules/CODE_STYLE.md`
   - 目标：明确代码风格统一化依据。

7. 阅读 `POP_OPEN/rules/VERSIONING_README.md`
   - 目标：明确代码日志生成。

## 文件读写约束
- `POP_OPEN/agentme/` 内文件：只读。
- `POP_OPEN/rules/DEVELOPMENT_README.md`：需用户允许后可修改。
- `POP_OPEN/apps/tuya.ai/your_pop_robot/README.md`：需用户允许后可修改。
- `POP_OPEN/apps/tuya.ai/your_pop_robot/CONNECTION.md`：需用户允许后可修改。
- `POP_OPEN/rules/VERSIONING_README.md`：只读。
- `POP_OPEN/rules/PROMPT_ENGINEERING.md`：只读。
- `POP_OPEN/rules/CODE_STYLE.md`：只读。
- `POP_OPEN/rules/VERSIONING_README.md`:只读。

## 执行原则
- 未完成上述阅读顺序前，不进入实现与改码阶段。
- AGENT_DEV.md 为只读，不可修改
- 遇到路径大小写或拼写差异时，以仓库实际存在文件为准，但不得改变本约束语义。
- 阅读本文件所列 README/规范文件时，必须在 agent 窗口按顺序反馈阅读进度。
  - 反馈格式建议：`已完成第N步阅读：<文件路径>；下一步：第N+1步`。
  - 每完成一步都要反馈一次，不可合并跳步反馈。

## 防幻觉约束
- 不确定的信息必须显式标注为“推测/待确认”，禁止表述为既定事实。
- 先给证据再下结论：优先引用代码路径、函数名、配置项、日志与命令输出。
- 涉及文件、函数、宏、配置项时，必须先在仓库中检索确认存在后再描述。
- 未执行的动作必须明确写“未执行/未验证”，禁止虚构编译、烧录、测试结果。
- 改动前后需一致性校验：说明“计划改动文件”和“实际改动文件”，不一致要说明原因。
- 高风险操作（删除、回退、批量替换、覆盖配置）前必须先获得用户确认。
- 涉及配置生效判断时，必须同时核对 `Kconfig` 定义与 `.config` 实际取值，避免单点推断。
