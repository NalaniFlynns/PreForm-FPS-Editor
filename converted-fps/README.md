# 转换后的 FPS（F4 → Form Studio 1）

共 83 份，由 `Form 4` 各材料参数按实测锚点（Flexible 80A / Tough 1500 V2 / Clear Cast）
换算为 `Form Studio 1` 目标物理参数后生成。

## 生成方式
- 以 ST1 自带的 `FLOPEN01`（Open Material Mode）对应层厚模板为骨架，保证可被 PreForm 识别加载。
- `metadata.base_material_identifier.machine_type_id = CLRK-1-2`（Studio 1），
  `versioned_material_code` 沿用原 F4 材料码，`source = 1`（自定义），`requires_revalidation = true`。
- `public_fields.categories[Material_Form_4_Family_Print].values` 写入可辨识物理量：
  I_norm / E_norm / t_normal_exp_s / t_early_exp_s / T_start|print|min / E_early / E_trans / t_wait / Z_stage / area_wait / A_bp。

## 覆盖范围
- 层厚 0.05 / 0.1 mm（ST1 存在该层厚）。
- 以下 8 个 F4 配置因 Studio 1 无对应层厚模板而**未生成**：0.025mm 类（FLCWPU01/FLGPBK05/FLGPCL05/FLGPGR05/FLPMBE01/FLTCLV01）、
  FLFMGR01 0.16/0.2mm。

## 重要边界（务必阅读）
- `underlying_material` 加密块来自 FLOPEN01 的开放材料模板，**不是**由 F4 数据重新加密得到；PreForm 无法离线重加密该块。
  因此这些文件的意义是：材料身份 + 可辨识物理量已按转换填入 `public_fields`，用于实验标定与对比，
  **不等同于 Formlabs 官方为该材料签发的加密配置**。
- 除三个实测锚点外，其余材料能量折减系数为按材料族外推，真机使用前必须打样校验。
