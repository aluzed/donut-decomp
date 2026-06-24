# RENDER-013 — Cache binaire / précompilation des shaders

- **Status:** DONE
- **Priority:** P3
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Render/OpenGL/ShaderProgram.cpp`, `src/Render/OpenGL/ShaderProgram.h:18,41`, `src/Game.cpp:175-185`

## Contexte
Chaque lancement recompile tous les shaders GLSL depuis la source : `Game.cpp:175-185`
lit `skin/mesh/postprocess.{vert,frag}` et appelle `new GL::ShaderProgram(src, src)`,
qui invoque `createSubShader` + `glCompileShader`/`glLinkProgram`
(`ShaderProgram.cpp`). Le coût est payé à chaque démarrage et ré-augmentera avec
l'arrivée des passes FX (bloom/blur/LUT/shadow).

## Approche
1. Ajouter dans `GL::ShaderProgram` un chemin via `glGetProgramBinary` /
   `glProgramBinary` (GL 4.1+ / `GL_ARB_get_program_binary`). Au build, après link,
   `Save()` le binaire (format + blob) dans un cache disque (p.ex.
   `cache/shaders/<hash>.bin`), clé = hash(source vert+frag).
2. Au constructeur (`ShaderProgram.cpp`), tenter de charger le binaire ; si absent,
   format invalide, ou `GL_LINK_STATUS` faux après `glProgramBinary`, **fallback**
   sur la compilation source actuelle puis ré-écriture du cache.
3. Invalider le cache si le hash source change (inclure les sources dans le hash) ;
   prévoir une option de désactivation pour le dev.
4. Centraliser le chargement de shaders dans un helper du `ResourceManager` pour
   éviter la duplication des `File::ReadAll(...) + make_unique` de `Game.cpp:175-185`.

## Critères d'acceptation
- [ ] Au 2e lancement (cache chaud), aucun `glCompileShader` n'est appelé pour les
      shaders inchangés.
- [ ] Modifier un `.vert`/`.frag` invalide le cache et recompile.
- [ ] Fallback transparent si `glProgramBinary` échoue (driver/format) — le jeu
      démarre quand même.
