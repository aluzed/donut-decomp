# RENDER-008b — Octree / kd-tree depuis les chunks `Tree`

- **Status:** TODO
- **Priority:** P2
- **Module:** Render
- **Depends on:** P3D-007, RENDER-008a
- **Files:** `src/P3D/P3DChunk.h`, `src/Level.cpp`, `src/Render/CompositeModel.cpp`

## Contexte
Le culling par frustum naïf (RENDER-008a) teste chaque drawable linéairement.
Les fichiers SH&R portent des chunks `Tree` / `TreeNode` (hiérarchie spatiale
pré-calculée) qui permettent des requêtes O(log n). P3D-007 doit d'abord parser
ces chunks (`P3DChunk.h`) ; ce ticket les exploite pour bâtir un octree/kd côté
moteur.

## Approche
1. **Prérequis P3D-007** : disposer des nœuds `Tree`/`TreeNode` parsés (bounds +
   références d'enfants/feuilles). Vérifier la structure exposée dans
   `P3D.generated.h`.
2. Construire au chargement du niveau (`Level.cpp`) un index spatial : soit
   reprendre directement la hiérarchie `Tree` du fichier, soit la convertir en
   octree maison à partir des AABB monde des drawables (RENDER-008a fournit
   l'AABB par drawable).
3. Implémenter une requête `QueryFrustum(frustum) -> liste de drawables` :
   descente récursive, élague les sous-arbres dont l'AABB est hors frustum.
4. Brancher la requête en amont de la boucle de rendu pour ne soumettre que les
   drawables visibles (remplace le balayage linéaire de RENDER-008a).

## Critères d'acceptation
- [ ] Un index spatial est construit au chargement (octree/kd ou hiérarchie
      `Tree`).
- [ ] La requête frustum retourne le même ensemble visible que le test linéaire
      de RENDER-008a (parité), mais sans tester chaque drawable.
- [ ] Gain mesurable du temps CPU de culling sur une grande scène.
