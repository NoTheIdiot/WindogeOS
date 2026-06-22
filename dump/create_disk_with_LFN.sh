#!/bin/bash

# Script pour créer un disque FAT32 sans privilèges root

# Variables
DISK_NAME="fat32.img"
MOUNT_POINT="./montage_fat32"
DISK_SIZE="50M"  # Taille du disque (50 Mo)
VOLUME_NAME="MY_DATA"

# Couleurs pour les messages
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Fonctions d'affichage
info() { echo -e "${GREEN}[INFO]${NC} $1"; }
erreur() { echo -e "${RED}[ERREUR]${NC} $1"; }
avertissement() { echo -e "${YELLOW}[AVERTISSEMENT]${NC} $1"; }

# Vérifier si mkfs.fat est disponible
if ! command -v mkfs.fat &> /dev/null; then
    erreur "mkfs.fat n'est pas installé. Installez-le avec:"
    echo "  sudo apt install dosfstools   # Debian/Ubuntu"
    echo "  sudo yum install dosfstools   # RHEL/CentOS"
    echo "  sudo pacman -S dosfstools     # Arch"
    exit 1
fi

# Vérifier si FUSE est disponible pour le montage sans privilèges
if ! command -v fuse2fs &> /dev/null && ! command -v fat-fuse &> /dev/null; then
    avertissement "FUSE n'est pas installé. On utilisera mount avec sudo uniquement pour le montage final."
    USE_SUDO_MOUNT=true
else
    USE_SUDO_MOUNT=false
fi

# Fonction pour monter sans privilèges (si possible)
mount_fat() {
    if [ "$USE_SUDO_MOUNT" = false ] && command -v fat-fuse &> /dev/null; then
        fat-fuse "$1" "$2"
    else
        # Demander sudo uniquement pour le montage
        info "Montage nécessite sudo..."
        sudo mount -o uid=$(id -u),gid=$(id -g) "$1" "$2"
    fi
}

# Fonction pour démonter
umount_fat() {
    if mountpoint -q "$1" 2>/dev/null; then
        if [ "$USE_SUDO_MOUNT" = false ] && command -v fusermount &> /dev/null; then
            fusermount -u "$1" 2>/dev/null
        else
            sudo umount "$1" 2>/dev/null
        fi
    fi
}

# Nettoyage
cleanup() {
    umount_fat "$MOUNT_POINT"
    rm -rf "$MOUNT_POINT"
    info "Nettoyage effectué"
}

trap cleanup EXIT INT TERM

# Créer le point de montage
info "Création du point de montage..."
rm -rf "$MOUNT_POINT"
mkdir -p "$MOUNT_POINT"

# Créer le disque virtuel
info "Création du disque virtuel de $DISK_SIZE..."
dd if=/dev/zero of="$DISK_NAME" bs=1 count=0 seek="$DISK_SIZE" 2>/dev/null

# Formater en FAT32
info "Formatage en FAT32 avec le nom de volume '$VOLUME_NAME'..."
mkfs.fat -F 32 -n "$VOLUME_NAME" "$DISK_NAME"

# Monter le disque
info "Montage du disque..."
mount_fat "$DISK_NAME" "$MOUNT_POINT"

# Vérifier que le montage a réussi
if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    erreur "Échec du montage. Tentative alternative..."

    # Alternative: utiliser une boucle device
    LOOP_DEV=$(sudo losetup -f --show "$DISK_NAME")
    sudo mount -o uid=$(id -u),gid=$(id -g) "$LOOP_DEV" "$MOUNT_POINT"
    
    if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        erreur "Impossible de monter le disque"
        exit 1
    fi
fi

# Créer la structure de répertoires
info "Création de la structure de répertoires..."

# Répertoires principaux
mkdir -p "$MOUNT_POINT/Documents"
mkdir -p "$MOUNT_POINT/Images"
mkdir -p "$MOUNT_POINT/Musique"
mkdir -p "$MOUNT_POINT/Projets/Projet_A"
mkdir -p "$MOUNT_POINT/Projets/Projet_B"
mkdir -p "$MOUNT_POINT/Archives"

# Créer des fichiers avec différents types de contenu
info "Création des fichiers..."

# Fichiers texte simples
echo "Ceci est un fichier texte simple." > "$MOUNT_POINT/bonjour.txt"
echo "Contenu du fichier README" > "$MOUNT_POINT/README.txt"

# Fichier avec plusieurs lignes
cat > "$MOUNT_POINT/Documents/notes_perso.txt" << EOF
Mes notes personnelles
=====================

Tâches à faire:
- Acheter du lait
- Appeler le médecin
- Finaliser le rapport

Idées:
• Apprendre le Python
• Voyager en Italie
• Lire un livre par mois

Contacts:
- Alice: 01 23 45 67 89
- Bob: alice@example.com

Citation préférée:
"Le seul vrai sage est celui qui sait qu'il ne sait rien" - Socrate
EOF

# Fichiers de configuration
echo "# Configuration de l'application" > "$MOUNT_POINT/Documents/config.ini"
echo "lang=fr" >> "$MOUNT_POINT/Documents/config.ini"
echo "theme=sombre" >> "$MOUNT_POINT/Documents/config.ini"
echo "autosave=true" >> "$MOUNT_POINT/Documents/config.ini"

# Fichier CSV
cat > "$MOUNT_POINT/Documents/donnees.csv" << EOF
Nom,Age,Ville,Email
Alice,30,Paris,alice@example.com
Bob,25,Lyon,bob@example.com
Charlie,35,Marseille,charlie@example.com
Diana,28,Lille,diana@example.com
Éric,40,Bordeaux,eric@example.com
EOF

# Fichier JSON
cat > "$MOUNT_POINT/Documents/parametres.json" << EOF
{
  "utilisateur": {
    "nom": "$USER",
    "date_creation": "$(date '+%Y-%m-%d')"
  },
  "preferences": {
    "langue": "français",
    "theme": "clair",
    "notifications": true
  },
  "applications": ["editor", "browser", "terminal"]
}
EOF

# Fichier HTML
cat > "$MOUNT_POINT/page_web.html" << EOF
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Mon Disque FAT32</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 40px;
            background-color: #f0f0f0;
        }
        .container {
            background-color: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 0 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #2c3e50;
        }
        .file-list {
            background-color: #ecf0f1;
            padding: 15px;
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Disque FAT32 de Test</h1>
        <p>Créé le $(date '+%d/%m/%Y à %H:%M')</p>
        
        <div class="file-list">
            <h2>Contenu du disque:</h2>
            <ul>
                <li>Fichiers texte (.txt, .md)</li>
                <li>Fichiers de données (.csv, .json)</li>
                <li>Fichiers web (.html)</li>
                <li>Documentation</li>
            </ul>
        </div>
        
        <p>Espace total: $DISK_SIZE</p>
    </div>
</body>
</html>
EOF

# Fichier Markdown
cat > "$MOUNT_POINT/Documentation.md" << EOF
# Documentation du Disque FAT32

## Structure des dossiers

\`\`\`
/
├── Documents/          # Documents textes
├── Images/            # Images (pour plus tard)
├── Musique/           # Fichiers audio
├── Projets/           # Dossiers de projets
│   ├── Projet_A/
│   └── Projet_B/
├── Archives/          # Archives et sauvegardes
└── Divers fichiers à la racine
\`\`\`

## Caractéristiques

- **Format**: FAT32
- **Taille**: $DISK_SIZE
- **Créé par**: $USER
- **Date**: $(date)

## Utilisation

Ce disque peut être utilisé sur:
- Windows (XP, 7, 10, 11)
- macOS
- Linux
- Systèmes embarqués
- Caméras et appareils photo

## Limitations FAT32

- Taille max fichier: 4 Go
- Taille max partition: 8 To (mais limité par l'OS)
- Pas de permissions UNIX natives
- Pas de liens symboliques
EOF

# Fichiers dans les sous-dossiers
echo "Document du Projet A" > "$MOUNT_POINT/Projets/Projet_A/rapport.txt"
echo "Code source" > "$MOUNT_POINT/Projets/Projet_A/main.py"

echo "Document du Projet B" > "$MOUNT_POINT/Projets/Projet_B/notes.txt"
cat > "$MOUNT_POINT/Projets/Projet_B/todo.txt" << EOF
TODO Projet B:
- [ ] Phase 1: Analyse
- [ ] Phase 2: Développement
- [ ] Phase 3: Tests
- [ ] Phase 4: Déploiement
EOF

# Créer des fichiers avec des noms variés
echo "Fichier avec ACCENTS: éèàçù" > "$MOUNT_POINT/accents_éèàç.txt"
echo "Fichier avec ESPACES ET MAJUSCULES" > "$MOUNT_POINT/FICHIER IMPORTANT.TXT"
echo "underscore_file" > "$MOUNT_POINT/fichier_avec_underscores.txt"
echo "mixedCase.File" > "$MOUNT_POINT/mixedCase.txt"
echo "123-numerique.txt" > "$MOUNT_POINT/123-fichier-numerique.txt"

# Fichier binaire petit
echo -n -e '\x7F\x45\x4C\x46\x01\x01\x01\x00' > "$MOUNT_POINT/header_elf.bin"

# Fichier plus gros (rempli de données)
info "Création d'un fichier de données plus volumineux..."
dd if=/dev/urandom of="$MOUNT_POINT/Archives/data_random.bin" bs=1024 count=5 2>/dev/null

# Fichier de log avec date
echo "=== LOG DU $(date '+%Y-%m-%d %H:%M:%S') ===" > "$MOUNT_POINT/log_creation.txt"
echo "Utilisateur: $USER" >> "$MOUNT_POINT/log_creation.txt"
echo "Script: $0" >> "$MOUNT_POINT/log_creation.txt"
echo "Taille disque: $DISK_SIZE" >> "$MOUNT_POINT/log_creation.txt"

# Démontage
info "Démontage du disque..."
umount_fat "$MOUNT_POINT"

# Vérifier la propriété du fichier
chown $USER:$USER "$DISK_NAME" 2>/dev/null || true

# Afficher les informations finales
echo ""
info "=========================================="
info "DISQUE FAT32 CRÉÉ AVEC SUCCÈS !"
info "=========================================="
echo ""
info "📁 Fichier créé: $(pwd)/$DISK_NAME"
info "🏷️  Nom du volume: $VOLUME_NAME"
info "💾 Taille: $DISK_SIZE"
info "👤 Propriétaire: $USER"
echo ""
info "📊 Contenu créé:"
echo "   - $(find . -maxdepth 1 -name "*.img" -type f | wc -l) fichier image"
echo "   - Structure de dossiers complète"
echo "   - Fichiers textes, HTML, JSON, CSV, binaires"
echo ""
info "🔧 Pour monter manuellement:"
echo "   mkdir -p ./mon_disque"
echo "   sudo mount -o uid=$(id -u),gid=$(id -g) $DISK_NAME ./mon_disque"
echo ""
info "🔍 Pour vérifier le contenu sans monter:"
echo "   mtype -i $DISK_NAME ::/ 2>/dev/null || echo 'Installez mtools pour cette fonction'"
echo ""
info "🧹 Le point de montage temporaire a été nettoyé."
echo ""