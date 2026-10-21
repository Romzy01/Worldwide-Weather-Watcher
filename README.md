# Worldwide Weather Watcher (3W)

Station météo embarquée développée en C sur **STM32 Nucleo-L476RG**, dans le cadre d'un projet de systèmes embarqués au CESI.

Le système mesure les conditions environnementales, horodate les relevés et les enregistre sur une carte **microSD** au format `.LOG`. Un terminal série permet de suivre le fonctionnement de la station et de configurer ses paramètres.

## Matériel utilisé

| Composant | Fonction |
| --- | --- |
| STM32 Nucleo-L476RG | Microcontrôleur principal |
| Grove Base Shield | Connexion des modules Grove |
| BME280 (I²C) | Température, humidité et pression atmosphérique |
| Capteur de luminosité Grove (analogique) | Mesure de luminosité |
| RTC DS1307 (I²C) | Date et heure des relevés |
| GPS Grove Air530Z (UART) | Coordonnées géographiques (acquisition à valider) |
| Module microSD (SPI) | Enregistrement des relevés |
| Grove Dual Button | Commandes utilisateur |
| LED RGB Grove | Indication du mode de fonctionnement |

## Modes de fonctionnement

- **STANDARD** : acquisition périodique des mesures et enregistrement sur microSD.
- **ÉCONOMIE** : acquisition moins fréquente et utilisation réduite du GPS.
- **MAINTENANCE** : mesures affichées sans écriture sur la microSD ; la carte est démontée pour permettre son retrait.
- **CONFIGURATION** : commandes envoyées par le terminal série pour consulter ou modifier les paramètres.

## Enregistrement des données

Les relevés sont conservés sur la **microSD**, dans des fichiers `.LOG` dont le nom suit le format `AAMMJJNN.LOG` (par exemple `26102100.LOG`). Une nouvelle **ligne** est ajoutée à chaque relevé ; le système archive le fichier lorsqu'il atteint la taille maximale configurée.

Exemple de contenu :

```text
DATE HEURE;PRESSION_HPA;TEMP_AIR_C;HYGR_PCT;LUMIN;LATITUDE;LONGITUDE
2026-10-21 09:06:07;969.2;22.8;39.4;402;NA;NA
2026-10-21 09:06:17;969.2;22.9;39.0;360;NA;NA
```

`NA` signifie qu'une valeur n'est pas disponible, par exemple lorsque le GPS n'a pas encore obtenu de position valide.

Par défaut, le paramètre `LOG_INTERVAL` vaut **10 minutes**. La configuration actuelle active `DEMO_ACCELEREE` : **une minute configurée équivaut à une seconde**, ce qui permet de démontrer un relevé toutes les 10 secondes environ en mode STANDARD.

> **Retrait de la carte microSD :** passer d'abord en mode MAINTENANCE et attendre la confirmation de démontage affichée sur le terminal série.

## Installer et lancer le projet

Le guide détaillé d'installation, compilation, téléversement et lecture du moniteur série est disponible dans **[LANCEMENT_3W.md](LANCEMENT_3W.md)**.

Pour récupérer directement la version intégrée :

```bash
git clone -b integration-capteurs https://github.com/Romzy01/Worldwide-Weather-Watcher.git
cd Worldwide-Weather-Watcher
```

Sur Linux avec PlatformIO installé :

```bash
# Compiler
~/.platformio/penv/bin/pio run

# Téléverser sur la carte STM32
~/.platformio/penv/bin/pio run --target upload

# Ouvrir le moniteur série (adapter le port USB si nécessaire)
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200
```

## Organisation du code

| Chemin | Rôle |
| --- | --- |
| `src/main.c` | Initialisation et boucle principale |
| `src/modes.c` | Gestion des modes |
| `src/mesures.c` | Acquisition des mesures |
| `src/journal_sd.c` et `src/sd_spi.c` | Enregistrement sur microSD et communication SPI |
| `src/config.c` | Configuration de la station |
| `src/gps.c` | Communication et traitement GPS |
| `src/boutons.c` | Gestion des boutons |
| `src/led_etat.c` | Gestion de la LED RGB |
| `include/` | Fichiers d'en-tête |
| `lib/` | Bibliothèques et pilotes |
| `platformio.ini` | Configuration de compilation PlatformIO |

## État des tests

Les tests réalisés sur la station ont permis de vérifier :

- les mesures de température, humidité, pression et luminosité ;
- la lecture de l'heure par le RTC ;
- l'écriture **et la relecture** de fichiers `.LOG` sur la microSD ;
- la sélection des modes à l'aide des boutons et de la LED RGB ;
- les intervalles de mesure en modes STANDARD et ÉCONOMIE ;
- l'affichage et la modification de paramètres en mode CONFIGURATION.

**À vérifier ultérieurement :** obtention d'une position GPS valide en conditions de réception satellite. L'initialisation du module a été observée, mais les coordonnées étaient `NA` lors des essais en intérieur.

## Travail collaboratif

La branche **`integration-capteurs`** contient la version intégrée testée. Les branches `test-*` conservent les étapes de développement et de validation des composants.
