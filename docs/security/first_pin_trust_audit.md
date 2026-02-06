# 首次 Pin 信任流程与审计闭环

## 目标
- 消除“首次信任”中的社会工程窗口，避免错误指纹被长期信任。
- 为每次首次 Pin 建立可追溯审计记录，支持事后复盘。

## 适用范围
- 新环境首次接入。
- 证书轮换后首次重连。
- 客户端提示 `pending_server_trust` 或 `pending_peer_trust` 的场景。

## 强制流程（SAS 双人确认）
1. 用户在客户端看到 `pending_*_pin` 后，不得单人确认。
2. 由第二确认人通过独立信道（电话/线下/企业 IM）复核 Pin/SAS。
3. 两人确认一致后，才允许执行 `mi_client_trust_pending_server` 或 `mi_client_trust_pending_peer`。
4. 若不一致，必须拒绝并上报安全事件，禁止重试覆盖。

## 审计记录字段（最小集合）
- `event_time_utc`
- `actor_primary`
- `actor_secondary`
- `device_id`
- `peer_or_server`
- `presented_pin`
- `confirmed_pin`
- `result`（approved/rejected）
- `channel`（phone/offline/im）
- `ticket_id`

## 留痕与保存策略
- 记录进入集中审计系统（SIEM 或等效日志平台）。
- 保存期至少 180 天。
- 审计日志必须防篡改（WORM 或不可变桶）。

## 例外处理
- 紧急故障窗口允许单人放行，但必须在 24 小时内补录二次确认与审批单。

## 月度检查项
- 抽样 10% 首次信任记录，核验双人确认是否真实存在。
- 检查是否存在“同一人主/复核”违规。
- 检查拒绝事件是否触发了安全工单与处置。
