# DataArray

## 读取 API

```angelscript
CKDataArray@ table = ctx.BorrowDataArrayByName("CurrentLevel");
int BML::CK::GetRowCount(CKDataArray@ table);
int BML::CK::GetColumnCount(CKDataArray@ table);
int BML::CK::FindColumn(CKDataArray@ table, const string &in name);
string BML::CK::GetString(CKDataArray@ table, int row, int col, const string &in defaultValue);
int BML::CK::GetInt(CKDataArray@ table, int row, int col, int defaultValue);
float BML::CK::GetFloat(CKDataArray@ table, int row, int col, float defaultValue);
bool BML::CK::GetBool(CKDataArray@ table, int row, int col, bool defaultValue);
```

## 写入 API

```angelscript
bool BML::CK::SetString(CKDataArray@ table, int row, int col, const string &in value);
bool BML::CK::SetInt(CKDataArray@ table, int row, int col, int value);
bool BML::CK::SetFloat(CKDataArray@ table, int row, int col, float value);
bool BML::CK::SetBool(CKDataArray@ table, int row, int col, bool value);
```

Returns true on success. Fails if: table null, row/col out of range, type mismatch.

## 安全读取模板

```angelscript
CKDataArray@ table = ctx.BorrowDataArrayByName("CurrentLevel");
if (table is null) return;
int rows = BML::CK::GetRowCount(table);
if (rows <= 0) return;
int col = BML::CK::FindColumn(table, "Level ID");
if (col < 0) return;
int value = BML::CK::GetInt(table, 0, col, -1);
```

## 写入回滚模板

```angelscript
// 保存
int oldValue = BML::CK::GetInt(table, 0, col, 0);
// 写入
BML::CK::SetInt(table, 0, col, newValue);
// 恢复（在 PRE_EXIT_LEVEL 或 OnUnload）
BML::CK::SetInt(table, 0, col, oldValue);
```

## 已知 DataArray

| DataArray | 行数 | 关键列 | 写入风险 |
|-----------|------|--------|----------|
| CurrentLevel | 1 | Level ID, ActiveBall, Ball_Pos_Frame, CurrentResetpoint, Activation Phase?, Points | 高 |
| AllLevel | 13 | Levelfile, StartBall, StartResetpoint, Sky, Light, Skytranslation, LevelBonus, Music | 中高 |
| Energy | 1 | Points, Lifes, StartPoints, StartLifes, Timefactor, LifeBonus | 高 |
| IngameParameter | 1 | Activetrafo, Activate Sector, Deactivate Sector, Tutorial activ?, RollSound activate? | 高 |
| Physicalize_Balls | 2 | Group Name, Friction, Elasticity, Mass, Radius | 中高 |
| PH_Groups | 23 | Group Names, Amount, Activation, Reset | 高 |
| Checkpoints | varies | Matrix, Object | 高 |
| ResetPoints | varies | Resetpoint | 高 |

## 规则

- 行列索引从 0 开始
- 用 FindColumn 按名字查列，不要硬编码列号
- 每次操作前重新 BorrowDataArrayByName
- 写入前必须备份，退出关卡时回滚
- 读取安全，写入有风险
