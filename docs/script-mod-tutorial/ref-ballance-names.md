# Ballance 名称速查

## 组名

| 组 | 用途 |
|----|------|
| P_Extra_Point | 分数拾取 |
| P_Extra_Life | 生命拾取 |
| PC_Checkpoints | 检查点标记 |
| PR_Resetpoints | 重生点标记 |
| PE_Levelende | 关卡终点 |
| PS_Levelstart | 关卡起点 |
| Sector_01 ~ Sector_XX | 区段对象集 |

## 检查点层级

| 名称 | 类型 | 层 |
|------|------|-----|
| PC_Checkpoints | Group | 作者内容列表 |
| PC_TwoFlames_01 | Level object | 作者放置的标记 |
| Checkpoints | DataArray | 运行时表 |
| PC_TwoFlames_MF | 3D Entity | 运行时实体 |
| PC_TwoFlames_Flame_Big | 3D Entity | 火焰实体 |

## 重生点层级

| 名称 | 类型 |
|------|------|
| PR_Resetpoints | Group |
| PR_Resetpoint_01 | Level object |
| ResetPoints | DataArray |

## 球类型

| 名称 | 用途 |
|------|------|
| P_Ball_Wood | 木球 |
| P_Ball_Stone | 石球 |
| P_Ball_Paper | 纸球 |

活动球名在 CurrentLevel.ActiveBall 字段中。

## 变换器

P_Trafo_Stone_XX, P_Trafo_Wood_XX, P_Trafo_Paper_XX

## DataArray 名称

CurrentLevel, AllLevel, Energy, IngameParameter, Physicalize_Balls, Physicalize_Floors, Physicalize_Convex, PH_Groups, PH, Checkpoints, ResetPoints

## 资源路径

```
3D Entities\PH\P_Ball_Wood.nmo
3D Entities\PH\P_Modul_01.nmo
3D Entities\Level\Level_01.NMO
```

## 查找入口

| 目标 | API |
|------|-----|
| 3D 实体 | ctx.Borrow3dEntityByName(name) |
| 组 | ctx.BorrowGroupByName(name) |
| DataArray | ctx.BorrowDataArrayByName(name) |
| 行为脚本 | ctx.BorrowScriptByName(name) |
