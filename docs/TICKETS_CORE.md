# Tickets Core

## CORE-001 — Fix Vector4::One initialisé à Zero
**Priorité :** P0 (bloquant)
**Fichier :** `src/Core/Math/Vector4.cpp:6`
**Description :** `Vector4::One` est initialisé à `(0,0,0,0)` au lieu de `(1,1,1,1)`.
**Impact :** Quaternion::Identity et toute normalisation peuvent être cassées.
**Correctif :** Changer `0.0f, 0.0f, 0.0f, 0.0f` par `1.0f, 1.0f, 1.0f, 1.0f`.

---

## CORE-002 — Fix Vector4::operator/=(Vector4) divise W par a.Z
**Priorité :** P0 (bloquant)
**Fichier :** `src/Core/Math/Vector4.h` (dans le corps de l'opérateur)
**Description :** L'opérateur `W /= a.Z` doit être `W /= a.W`.
**Correctif :** Remplacer `W /= a.Z;` par `W /= a.W;`.

---

## CORE-003 — Fix Vector4::operator-() ne nie pas W
**Priorité :** P1
**Fichier :** `src/Core/Math/Vector4.h`
**Description :** L'opérateur unaire de négation retourne `(-X, -Y, -Z, -Z)` au lieu de `(-X, -Y, -Z, -W)`.
**Correctif :** Changer le 4ème paramètre.

---

## CORE-004 — Implémenter Vector4::Cross()
**Priorité :** P2
**Fichier :** `src/Core/Math/Vector4.h`, `Vector4.cpp`
**Description :** `Cross()` est déclaré mais non implémenté. Pour un Vector4, le cross product n'a pas de sens standard ; clarifier si c'est un cross 3D (ignorer W) ou si c'est une erreur de déclaration.

---

## CORE-005 — Implémenter Matrix3x3::Inverse()
**Priorité :** P1
**Fichier :** `src/Core/Math/Matrix3x3.h`, `Matrix3x3.cpp`
**Description :** `Inverse()` est déclaré inline dans le header mais jamais défini. Ajouter implémentation (cofacteurs 3x3).
**Note :** `Inverse()` est aussi manquant pour `Matrix4x4` qui a `Determinant()` mais pas d'appel a `Inverse()` (inversion inline différente ? vérifier).

---

## CORE-006 — Implémenter Matrix3x3 opérateurs manquants (+, *, ==, !=)
**Priorité :** P1
**Fichier :** `src/Core/Math/Matrix3x3.cpp`
**Description :** Les opérateurs `+`, `+=`, `*`, `*=`, `==`, `!=`, `-` (unaire) sont déclarés dans le header mais manquent dans le .cpp.
**Correctif :** Ajouter les définitions.

---

## CORE-007 — Implémenter Quaternion::FromRotationMatrix(Matrix3x3)
**Priorité :** P1
**Fichier :** `src/Core/Math/Quaternion.cpp:115`
**Description :** La surcharge Matrix3x3 est un stub qui retourne `*this` avec un `// TODO`.
**Correctif :** Porter l'algorithme existant de `FromRotationMatrix(Matrix4x4)`.

---

## CORE-008 — Implémenter Quaternion::MakeFromEuler
**Priorité :** P1
**Fichier :** `src/Core/Math/Quaternion.cpp`
**Description :** Retourne toujours `Quaternion(0,0,0,1)` (identity). 
**Correctif :** Implémenter la conversion Euler -> Quaternion (Yaw*Pitch*Roll).

---

## CORE-009 — Implémenter Quaternion::Euler()
**Priorité :** P1
**Fichier :** `src/Core/Math/Quaternion.cpp`
**Description :** `Euler()` est déclaré dans le header mais jamais défini. 
**Correctif :** Implémenter l'extraction des angles d'Euler depuis le quaternion.

---

## CORE-010 — Fix Vector2::One initialisé à Zero
**Priorité :** P0 (bloquant)
**Fichier :** `src/Core/Math/Vector2.cpp:6`
**Description :** Même bug que CORE-001. `Vector2::One` est `(0.0f, 0.0f)`.
**Correctif :** Changer par `(1.0f, 1.0f)`.

---

## CORE-011 — Vector2Int::Length() retourne float -> int (perte de précision)
**Priorité :** P3 (cosmétique)
**Fichier :** `src/Core/Math/Vector2Int.h`
**Description :** `Length()` utilise `Math::Sqrt` (float) puis cast en int.
**Correctif :** Ajouter une surcharge `LengthF()` ou documenter le comportement.

---

## CORE-012 — Étoffer BoundingBox et BoundingSphere
**Priorité :** P2
**Fichier :** `src/Core/Math/BoundingBox.h`, `BoundingSphere.h`
**Description :** Classes minimales sans tests d'intersection.
**Correctif :** Ajouter `Intersects(BoundingBox)`, `Intersects(BoundingSphere)`, `Contains(Vector3)`, `Merge(BoundingBox)`, `Expand(float)`.

---

## CORE-013 — Support du format compressé P3DZ / RZ
**Priorité :** P1
**Fichier :** `src/P3D/P3DFile.cpp:13`
**Description :** Seul le format P3D brut est supporté. Les formats compressés P3DZ et RZ sont ignorés avec un `assert`.
**Correctif :** 
- P3DZ : compression propriétaire Radical (à reverser via Ghidra sur `SIMPSONS.EXE`)
- RZ : wrapper zlib deflate — chercher les bytes `0x5A52` puis décompresser

---

## CORE-014 — Améliorer le système de Log
**Priorité :** P3
**Fichier :** `src/Core/Log.h`
**Description :** Les 4 niveaux (Debug, Info, Warn, Error) écrivent tous dans stdout sans filtrage. Ajouter :
- Filtrage par niveau (compile-time ou runtime)
- Sortie fichier optionnelle
- Horodatage
- Remplacer les `std::cout` et `printf` dispersés dans le code par Log::*

---

## CORE-015 — Supprimer les includes inutiles de `<iostream>` et `<fmt/format.h>`
**Priorité :** P3 (nettoyage)
**Fichiers :** Plusieurs (Shader.cpp:108, RSDFile.cpp, Level.cpp, etc.)
**Description :** Beaucoup de fichiers utilisent `std::cout` ou `fmt::print` au lieu du système `Log`. Après CORE-014, les remplacer.
