# 对象与组

## 查找 API

```angelscript
CK3dEntity@ ctx.Borrow3dEntityByName(const string &in name);
CKGroup@ ctx.BorrowGroupByName(const string &in name);
CKDataArray@ ctx.BorrowDataArrayByName(const string &in name);
CKBehavior@ ctx.BorrowScriptByName(const string &in name);
```

## 对象信息

```angelscript
bool BML::CK::IsValid(CKObject@ object);
string BML::CK::GetName(CKObject@ object);
int BML::CK::GetId(CKObject@ object);
int BML::CK::GetClassId(CKObject@ object);
bool BML::CK::IsVisible(CKObject@ object);
bool BML::CK::IsDynamic(CKObject@ object);
```

## 位置和可见性

```angelscript
VxVector BML::CK::GetPosition(CK3dEntity@ entity);
void BML::CK::SetPosition(CK3dEntity@ entity, const VxVector &in position);
VxVector BML::CK::GetScale(CK3dEntity@ entity, bool local = true);
void BML::CK::SetScale(CK3dEntity@ entity, const VxVector &in scale, bool local = true);
void BML::CK::Show(CKBeObject@ object, CK_OBJECT_SHOWOPTION show = CKSHOW, bool hierarchy = false);
```

`Show` 参数：`CKSHOW` 显示，`CKHIDE` 隐藏。`hierarchy = true` 影响子对象。

## 层级

```angelscript
int BML::CK::GetChildCount(CK3dEntity@ entity);
CK3dEntity@ BML::CK::BorrowChild(CK3dEntity@ entity, int index);
CK3dEntity@ BML::CK::BorrowParent(CK3dEntity@ entity);
```

## BeObject 操作

```angelscript
int BML::CK::GetPriority(CKBeObject@ object);
int BML::CK::GetScriptCount(CKBeObject@ object);
int BML::CK::GetAttributeCount(CKBeObject@ object);
void BML::CK::SetIC(CKBeObject@ object, bool hierarchy = false);
void BML::CK::RestoreIC(CKBeObject@ object, bool hierarchy = false);
```

## 组

```angelscript
CKGroup@ group = ctx.BorrowGroupByName("P_Extra_Point");
if (group is null) return;

int count = group.GetObjectCount();
for (int i = 0; i < count; i++) {
    CKBeObject@ member = group.GetObject(i);
    if (member !is null) {
        string name = BML::CK::GetName(member);
    }
}
```

CKGroup 成员方法（由 CKAS 提供）：

```angelscript
int GetObjectCount();
CKBeObject@ GetObject(int pos);
CKERROR AddObject(CKBeObject@ obj);
CKERROR AddObjectFront(CKBeObject@ obj);
CKERROR InsertObjectAt(CKBeObject@ obj, int pos);
CKBeObject@ RemoveObject(int pos);
void RemoveObject(CKBeObject@ obj);
void Clear();
CK_CLASSID GetCommonClassID();
```

## 资源加载

```angelscript
BML::ObjectLoadOptions options;
options.File = resourcePath;
options.Rename = true;
options.MasterName = "MyObject";
options.AddToScene = true;
options.ReuseMeshes = true;
options.ReuseMaterials = true;
options.Dynamic = true;

BML::ObjectLoadResult@ result = BML::CK::LoadObject(options);
if (result !is null && result.Success && result.Count > 0) {
    CKObject@ obj = result.BorrowObject(0);
    CKObject@ main = result.BorrowMainObject();
}
```

## 查询时机

- 菜单阶段：关卡对象不存在
- OnLoad：仅表示脚本加载，不代表关卡已加载
- 安全时机：`GAME_EVENT_START_LEVEL` 之后

## 规则

- 所有 `Borrow*` 返回值必须判空。
- 原始 CK 句柄只在对象仍存在时有效。仅缓存关卡内生命周期明确的对象，并在
  对象可能被删除或退出关卡前清空。
- 跨回调保存身份时，优先使用 CKAS 的 `ObjectRef`、`Entity3DRef` 等可重新验证
  引用并在使用前检查 `valid`；也可以保存 `CK_ID` 后在操作前重新解析。
- 名称只有在对应对象确实唯一且名称稳定时才能作为重新查找键。
- 组名查实体、实体名查组都会返回 `null`；关卡切换后必须重新查询。
