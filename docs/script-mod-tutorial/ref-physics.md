# 物理 API

## PhysicalizeDefinition

物理化参数结构体：

```angelscript
BML::PhysicalizeDefinition physics;
physics.Fixed = false;
physics.Friction = 0.6f;
physics.Elasticity = 0.2f;
physics.Mass = 2.0f;
physics.CollisionGroup = "";
physics.StartFrozen = false;
physics.EnableCollision = true;
physics.CalcMassCenter = false;
physics.LinearDamp = 0.6f;
physics.RotDamp = 0.1f;
physics.CollisionSurface = "P_Ball_Wood_Mesh";
physics.MassCenter = VxVector(0.0f, 0.0f, 0.0f);
```

## 物理化

```angelscript
bool BML::Physics::PhysicalizeBall(CK3dEntity@ target,
    const BML::PhysicalizeDefinition &in definition,
    const VxVector &in center, float radius);

bool BML::Physics::PhysicalizeConvex(CK3dEntity@ target,
    const BML::PhysicalizeDefinition &in definition,
    CKMesh@ mesh = null);

bool BML::Physics::PhysicalizeConcave(CK3dEntity@ target,
    const BML::PhysicalizeDefinition &in definition,
    CKMesh@ mesh = null);

bool BML::Physics::Unphysicalize(CK3dEntity@ target);
```

## 物理操作

```angelscript
bool BML::Physics::WakeUp(CK3dEntity@ target);

bool BML::Physics::Impulse(CK3dEntity@ target,
    const VxVector &in position, CK3dEntity@ positionReference,
    const VxVector &in direction, CK3dEntity@ directionReference,
    float impulse);

bool BML::Physics::SetForce(CK3dEntity@ target,
    const VxVector &in position, CK3dEntity@ positionReference,
    const VxVector &in direction, CK3dEntity@ directionReference,
    float force);

bool BML::Physics::ClearForce(CK3dEntity@ target);
```

| API | 效果 | 持续性 |
|-----|------|--------|
| WakeUp | 唤醒休眠物理对象 | 瞬时 |
| Impulse | 瞬间施力 | 瞬时 |
| SetForce | 持续施力 | 持续，直到 ClearForce |
| ClearForce | 清除持续力 | 瞬时 |

## 用法示例

```angelscript
// 物理化一个球
BML::PhysicalizeDefinition physics;
physics.Fixed = false;
physics.Friction = 0.6f;
physics.Elasticity = 0.2f;
physics.Mass = 2.0f;
physics.EnableCollision = true;
physics.LinearDamp = 0.6f;
physics.RotDamp = 0.1f;
physics.CollisionSurface = "P_Ball_Wood_Mesh";

bool ok = BML::Physics::PhysicalizeBall(ball, physics, VxVector(0, 0, 0), 2.0f);
```

```angelscript
// 施加冲量
BML::Physics::WakeUp(ball);
BML::Physics::Impulse(ball,
    VxVector(0, 0, 0), null,    // 作用位置，参考坐标系
    VxVector(1, 0, 0), null,    // 方向，参考坐标系
    1.5f);                      // 冲量大小
```

```angelscript
// 持续力
BML::Physics::SetForce(ball,
    VxVector(0, 0, 0), null,
    VxVector(1, 0, 0), null,
    0.6f);

// 清除
BML::Physics::ClearForce(ball);
```

## 清理模板

```angelscript
if (ball !is null && BML::CK::IsValid(ball)) {
    BML::Physics::ClearForce(ball);
    BML::Physics::Unphysicalize(ball);
    BML::CK::Show(ball, CKHIDE, true);
}
@ball = null;
```

清理时机：`GAME_EVENT_PRE_EXIT_LEVEL`、`GAME_EVENT_PRE_LOAD_LEVEL`、`OnUnload`。

## OnPhysicalize 回调

```angelscript
void OnPhysicalize(const BML::ModContext &in ctx, const BML::PhysicalizeEvent &in event)
```

### PhysicalizeEvent 属性

| 属性 | 类型 |
|------|------|
| TargetId | int |
| TargetName | string |
| Fixed | bool |
| Friction | float |
| Elasticity | float |
| Mass | float |
| CollisionGroup | string |
| StartFrozen | bool |
| EnableCollision | bool |
| AutoCalcMassCenter | bool |
| LinearDamp | float |
| RotDamp | float |
| CollisionSurface | string |
| MassCenter | VxVector |
| BallCount | int |
| ConvexCount | int |
| ConcaveCount | int |

方法：

```angelscript
CK3dEntity@ event.BorrowTarget();
VxVector event.GetBallCenter(int index);
float event.GetBallRadius(int index);
CKMesh@ event.BorrowConvexMesh(int index);
CKMesh@ event.BorrowConcaveMesh(int index);
```

## OnUnphysicalize 回调

```angelscript
void OnUnphysicalize(const BML::ModContext &in ctx, const BML::ObjectEvent &in event)
// event.TargetId, event.TargetName, event.BorrowTarget()
```

## 球物理参数参考

| Ball | Friction | Elasticity | Mass | Radius |
|------|----------|------------|------|--------|
| P_Ball_Wood | 0.6 | 0.2 | 2 | 2 |
| P_Ball_Stone | 0.7 | 0.1 | 10 | 2 |

## 规则

- WakeUp 应在 Impulse/SetForce 之前调用
- SetForce 是持续的，必须手动 ClearForce
- BorrowTarget() 仅在回调内有效
- 清理顺序：ClearForce -> Unphysicalize -> Show(CKHIDE) -> null
- Impulse/SetForce 的 positionReference/directionReference 传 `null` 表示世界坐标
