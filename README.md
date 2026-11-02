# Meghir Stress — compresseur inspiré du Distressor (VST3 + AU)

Projet JUCE / CMake pour Mac (Apple Silicon + Intel), usage personnel.
Ce n'est pas une émulation de circuit : c'est un **modèle comportemental**. Les constantes à ajuster
sont toutes regroupées en haut de `Source/StrainCore.h` (section `CALIBRATION`).

## Contenu

```
CMakeLists.txt
Source/
  StrainCore.h        cœur DSP, sans dépendance JUCE (testable seul)
  PluginProcessor.*   paramètres, suréchantillonnage 2x, liaison avec le cœur
  PluginEditor.*      interface dessinée en code (potards, boutons à LED, vumètre)
Tests/
  test_core.cpp       test autonome du DSP
```

## 1. Tester le DSP seul (sans JUCE)

```bash
g++ -std=c++17 -O2 -Wall -Wextra Tests/test_core.cpp -o test_core && ./test_core
```

## 2. Compiler le plugin

Prérequis : Xcode (ou `xcode-select --install`), et CMake (`brew install cmake`).
Le premier lancement télécharge JUCE depuis GitHub (internet requis).

```bash
cd Strain
cmake -B build -G Xcode
cmake --build build --config Release
```

Avec `COPY_PLUGIN_AFTER_BUILD`, les plugins sont copiés automatiquement dans :
- `~/Library/Audio/Plug-Ins/VST3/Meghir Stress.vst3`
- `~/Library/Audio/Plug-Ins/Components/Meghir Stress.component`

Si le tag JUCE `8.0.6` n'existe pas, remplace-le dans `CMakeLists.txt` par le dernier tag 8.0.x.

## 3. Dans Ableton Live

1. Préférences → Plug-ins → active « Utiliser les dossiers système VST3 » et/ou les Audio Units.
2. Relance un scan (ou redémarre Live). Meghir Stress apparaît dans Plug-ins → Dynamics / Meghir Stress.
3. Validation AU (optionnel) : `auval -v aufx Mstr Pers`

## Paramètres

| Contrôle | Rôle |
|---|---|
| Input (−6 à +30 dB) | gain avant détecteur : règle le niveau de compression (le seuil est fixe) |
| Attack / Release (1–10) | temps de réponse, échelle logarithmique |
| Output (−12 à +24 dB) | gain de sortie |
| Ratio | 1:1, 2:1, 3:1, 4:1, 6:1, 10:1, 20:1, NUKE |
| Dist 2 / Dist 3 | saturation à harmoniques paires / impaires |
| Det HP | passe-haut sur le détecteur (basses moins sensibles) |
| Opto | attaque plus lente, relâchement à queue longue |
| Link | détection stéréo liée |

## Latence

Le suréchantillonnage ajoute quelques échantillons de latence, déclarés à l'hôte (Ableton la compense).

## À calibrer / vérifier

- Plages d'attack et de release (`kAttackMinMs` etc.), seuil, coude, force des distorsions.
- Les noms et les fonctions des boutons sont à comparer au manuel officiel de la machine.
- Jamais compilé avec JUCE côté développement : si une erreur apparaît, renvoie le message tel quel.
