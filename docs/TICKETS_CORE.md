# Tickets — Core

## CORE-001 — Fix Vector4::One initialized to Zero
**Priority:** P0 (blocking)
**File:** `src/Core/Math/Vector4.cpp:6`
**Description:** `Vector4::One` is initialized to `(0,0,0,0)` instead of `(1,1,1,1)`.
**Impact:** Quaternion::Identity and any normalization may be broken.
**Fix:** Change `0.0f, 0.0f, 0.0f, 0.0f` to `1.0f, 1.0f, 1.0f, 1.0f`.

## CORE-002 — Fix Vector4::operator/=(Vector4) divides W by a.Z
**Priority:** P0 (blocking)
**File:** `src/Core/Math/Vector4.h`
**Fix:** Replace `W /= a.Z;` with `W /= a.W;`.

## CORE-003 — Fix Vector4::operator-() doesn't negate W
**Priority:** P1
**File:** `src/Core/Math/Vector4.h`
**Fix:** Change 4th parameter from `-Z` to `-W`.

## CORE-010 — Fix Vector2::One initialized to Zero
**Priority:** P0 (blocking)
**File:** `src/Core/Math/Vector2.cpp:6`
**Fix:** Change `(0.0f, 0.0f)` to `(1.0f, 1.0f)`.

## CORE-004 — Implement Vector4::Cross()
**Priority:** P2
**Files:** `src/Core/Math/Vector4.h`, `Vector4.cpp`
**Description:** Returns Vector3 cross product of XYZ components (ignoring W).

## CORE-005 — Implement Matrix3x3::Inverse()
**Priority:** P1
**Files:** `src/Core/Math/Matrix3x3.h`, `Matrix3x3.cpp`
**Description:** Inverse via adjugate/determinant method.

## CORE-006 — Implement Matrix3x3 missing operators
**Priority:** P1
**File:** `src/Core/Math/Matrix3x3.h`
**Description:** `*`, `*=`, `+`, `+=`, `*(float)`, `*=(float)`, `==`, `!=`, `-` (unary)

## CORE-007 — Implement Quaternion::FromRotationMatrix(Matrix3x3)
**Priority:** P1
**File:** `src/Core/Math/Quaternion.cpp:102`
**Description:** Delegate to the Matrix4x4 version.

## CORE-008 — Implement Quaternion::MakeFromEuler
**Priority:** P1
**File:** `src/Core/Math/Quaternion.cpp`
**Description:** Implement Euler → Quaternion conversion (Yaw*Pitch*Roll).

## CORE-009 — Implement Quaternion::Euler()
**Priority:** P1
**File:** `src/Core/Math/Quaternion.cpp`
**Description:** Extract Euler angles from quaternion.

## CORE-011 — Vector2Int::Length() returns truncated int
**Priority:** P3 (cosmetic)
**File:** `src/Core/Math/Vector2Int.h`
**Description:** Add `LengthF()` returning float.

## CORE-012 — Expand BoundingBox and BoundingSphere
**Priority:** P2
**Files:** `src/Core/Math/BoundingBox.h`, `BoundingSphere.h`
**Description:** Add `Intersects`, `Contains`, `Merge`, `Transform`, `Expand`.

## CORE-013 — Support P3DZ / RZ compressed formats
**Priority:** P1
**File:** `src/P3D/P3DFile.cpp:13`
**Description:** Only raw P3D supported. RZ needs zlib deflate. P3DZ needs Ghidra reverse engineering.

## CORE-014 — Improve Log system
**Priority:** P3
**File:** `src/Core/Log.h`
**Description:** Error/Warn to stderr, add level filtering, replace scattered `std::cout`/`printf` calls.

## CORE-015 — Remove unnecessary `<iostream>` and `<fmt/format.h>` includes
**Priority:** P3 (cleanup)
**Files:** Multiple
**Description:** Replace with `Core/Log.h` throughout.
