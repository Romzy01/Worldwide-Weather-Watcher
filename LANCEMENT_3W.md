# Lancement du projet Worldwide Weather Watcher (3W)

Ce guide permet à un camarade de récupérer, compiler et tester le projet **3W** sur une **Nucleo-L476RG** avec **VS Code + PlatformIO**.

> **Environnement de référence : Linux Mint** (directement ou dans VirtualBox). Les commandes de port `/dev/ttyACM*` sont propres à Linux.
>
> **Branche à utiliser : `integration-capteurs`** (le programme complet n'est pas sur `main`).

## 1. Prérequis (à faire une seule fois)

- Installer **Visual Studio Code** : https://code.visualstudio.com/
- Dans VS Code, aller dans **Extensions** (`Ctrl+Shift+X`), chercher **PlatformIO IDE** et cliquer sur **Installer**.
- Attendre la fin de l'initialisation de PlatformIO.
- Vérifier que Git est installé :

```bash
git --version
```

Si Git est absent sous Linux Mint :

```bash
sudo apt update
sudo apt install git
```

## 2. Télécharger le programme depuis GitHub (une seule fois)

Ouvrir un terminal Linux, puis saisir :

```bash
cd ~/Documents
git clone -b integration-capteurs https://github.com/Romzy01/Worldwide-Weather-Watcher.git
cd Worldwide-Weather-Watcher
```

Si le projet est **déjà téléchargé**, ne pas refaire `git clone` : ouvrir son dossier directement.

Dans VS Code : **File / Fichier > Open Folder / Ouvrir un dossier** > choisir `Worldwide-Weather-Watcher`.

## 3. Compiler le programme

Dans le terminal de VS Code, **à la racine du projet** (là où se trouve `platformio.ini`) :

```bash
~/.platformio/penv/bin/pio run
```

Résultat attendu : `SUCCESS`.

Il est aussi possible de cliquer sur le bouton **✓ Build** de PlatformIO.

## 4. Téléverser le programme sur la STM32

Brancher la **Nucleo-L476RG** au PC avec un câble USB compatible données. Si Linux tourne dans **VirtualBox**, rattacher le périphérique **STMicroelectronics ST-LINK** à la machine virtuelle via **Périphériques > USB**.

Dans le terminal, toujours à la racine du projet :

```bash
~/.platformio/penv/bin/pio run --target upload
```

Résultat attendu : `SUCCESS`.

> Le téléversement remplace le programme présent sur la STM32. Il ne supprime pas les fichiers de la microSD.

## 5. Ouvrir le moniteur série

Déterminer le port série sous Linux :

```bash
ls /dev/ttyACM*
```

Si le résultat contient `/dev/ttyACM0` :

```bash
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200
```

Si le résultat contient `/dev/ttyACM1` :

```bash
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM1 -b 115200
```

Appuyer brièvement sur **RESET** de la Nucleo pour voir les messages de démarrage :

```text
=== WORLDWIDE WEATHER WATCHER ===
  RTC DS1307 ............ OK
  BME280 ................ OK
  Capteur de lumiere .... ADC PA4
  GPS Air530Z ........... UART4 9600 bauds (sous interruption)

=== Mode STANDARD ===
```

Les mesures s'affichent ensuite dans le terminal. Pour **quitter le moniteur série**, appuyer sur `Ctrl+C`.

## 6. Pour les prochains lancements

Une fois le projet récupéré et les outils installés, il suffit de :

```bash
cd ~/Documents/Worldwide-Weather-Watcher
~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run --target upload
```

Puis ouvrir le moniteur sur **le port réellement détecté** (voir étape 5).

## 7. Problèmes fréquents

**`pio: commande introuvable`** — Utiliser le chemin complet `~/.platformio/penv/bin/pio` indiqué dans ce guide, ou vérifier l'installation de PlatformIO.

**`could not open port /dev/ttyACM0`** — Vérifier le nouveau port avec `ls /dev/ttyACM*`. Après une reconnexion, il peut devenir `/dev/ttyACM1`.

**Aucun périphérique `/dev/ttyACM*`** — Contrôler le câble USB, l'alimentation et, dans VirtualBox, le rattachement du ST-LINK à la machine virtuelle. `lsusb` peut confirmer la présence du ST-LINK.

**Le port est déjà utilisé** — Fermer un autre moniteur série avec `Ctrl+C` avant de le rouvrir.

**`git clone` dit que le dossier existe déjà** — Le projet est déjà téléchargé ; entrer dans ce dossier plutôt que le cloner à nouveau.

**Pas de mesures GPS (`GPS=NA`)** — Cela signifie qu'aucune position valide n'est disponible ; la réception GPS doit être testée dans un environnement permettant la réception des satellites.

## 8. Matériel nécessaire

Pour tester l'ensemble des fonctions, utiliser une **Nucleo-L476RG** et le câblage correspondant au montage 3W : BME280 (I²C), RTC DS1307 (I²C), capteur de luminosité (A2), GPS Air530Z (A0), boutons Grove (D2), LED RGB (D6) et module microSD raccordé au brochage SPI prévu dans le code.

**Attention :** ne pas supposer que tous les modules tolèrent 5 V ; vérifier les tensions d'alimentation et les niveaux logiques des composants avant branchement.

## Dépôt

https://github.com/Romzy01/Worldwide-Weather-Watcher/tree/integration-capteurs

hoisir la version du programme pour la soutenance

Le projet Worldwide Weather Watcher (3W) possède deux versions, disponibles sur GitHub.

Branche GitHub

Fonctionnement

integration-capteurs

Version normale : utilisation du GPS réel uniquement.

demo-gps-soutenance

Version de démonstration : GPS réel prioritaire, coordonnées simulées si aucune position GPS n'est disponible.

1. Lancer la version normale

Pour utiliser la station météo avec uniquement les données GPS réelles :

git switch integration-capteurs

Compiler et téléverser le programme sur la STM32 :

~/.platformio/penv/bin/pio run --target upload

Ouvrir le moniteur série :

~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200

En l'absence de réception GPS, le programme affichera GPS=NA.