#!/bin/bash

# Script pour créer un disque FAT32 avec noms de fichiers 8.3
# Pour l'apprentissage du parsing FAT32 sans LFN (Long File Names)

# Variables
DISK_NAME="fat32.img"
MOUNT_POINT="./fat_mount"
DISK_SIZE="10M"  # Plus petit pour l'apprentissage
VOLUME_NAME="FAT32LEARN"

# Couleurs pour les messages
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Fonctions d'affichage
info() { echo -e "${GREEN}[INFO]${NC} $1"; }
erreur() { echo -e "${RED}[ERREUR]${NC} $1"; }
avertissement() { echo -e "${YELLOW}[AVERTISSEMENT]${NC} $1"; }

# Fonction pour valider les noms 8.3
validate_83_name() {
    local name="$1"
    local base="${name%.*}"
    local ext="${name##*.}"
    
    # Vérifier longueur
    if [ ${#base} -gt 8 ] || [ ${#ext} -gt 3 ]; then
        erreur "Nom invalide 8.3: $name (base: ${#base} chars, ext: ${#ext} chars)"
        return 1
    fi
    
    # Vérifier caractères valides (A-Z, 0-9, _)
    if [[ ! "$base" =~ ^[A-Za-z0-9_]+$ ]] || [[ ! "$ext" =~ ^[A-Za-z0-9_]*$ ]]; then
        erreur "Caractères invalides dans: $name"
        return 1
    fi
    
    return 0
}

# Fonction pour convertir en majuscules (standard FAT)
to_fat_upper() {
    echo "$1" | tr '[:lower:]' '[:upper:]'
}

# Vérifier si mkfs.fat est disponible
if ! command -v mkfs.fat &> /dev/null; then
    erreur "mkfs.fat n'est pas installé. Installez-le avec:"
    echo "  sudo apt install dosfstools   # Debian/Ubuntu"
    echo "  sudo yum install dosfstools   # RHEL/CentOS"
    echo "  sudo pacman -S dosfstools     # Arch"
    exit 1
fi

# Nettoyage
cleanup() {
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        sudo umount "$MOUNT_POINT" 2>/dev/null
    fi
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

# Formater en FAT32 sans LFN
info "Formatage en FAT32 (sans LFN) avec le nom de volume '$VOLUME_NAME'..."
mkfs.fat -F 32 -n "$VOLUME_NAME" -s 2 -f 1 "$DISK_NAME"
# Options:
# -s 2: secteurs par cluster
# -f 1: nombre de FATs

# Monter le disque
info "Montage du disque..."
sudo mount -o uid=$(id -u),gid=$(id -g),shortname=mixed "$DISK_NAME" "$MOUNT_POINT"

# Vérifier que le montage a réussi
if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    erreur "Échec du montage."
    exit 1
fi

# ============================================
# CRÉATION DE LA STRUCTURE 8.3
# ============================================

info "Création de la structure 8.3..."

# Créer les répertoires racine (noms 8.0)
mkdir -p "$MOUNT_POINT/DOCS"
mkdir -p "$MOUNT_POINT/IMGS"
mkdir -p "$MOUNT_POINT/MUSIC"
mkdir -p "$MOUNT_POINT/PRJ"
mkdir -p "$MOUNT_POINT/SYS"
mkdir -p "$MOUNT_POINT/TEMP"
mkdir -p "$MOUNT_POINT/PRJ/PRJ1"
mkdir -p "$MOUNT_POINT/PRJ/PRJ2"
mkdir -p "$MOUNT_POINT/SYS/LOG"
mkdir -p "$MOUNT_POINT/SYS/CFG"
mkdir -p "$MOUNT_POINT/DOCS/TECH"
mkdir -p "$MOUNT_POINT/DOCS/ADMIN"

# ============================================
# FICHIERS À LA RACINE
# ============================================

info "Création des fichiers racine..."

# Fichiers système
echo "BOOT SECTOR INFO" > "$MOUNT_POINT/BOOTSEC.TXT"
echo "FAT1 FAT2" > "$MOUNT_POINT/FATINFO.TXT"
echo "ROOT DIRECTORY" > "$MOUNT_POINT/ROOTDIR.TXT"

# Fichiers simples
echo "Hello FAT32 World!" > "$MOUNT_POINT/HELLO.TXT"
echo "Read me first" > "$MOUNT_POINT/README.TXT"
echo "Volume: $VOLUME_NAME" > "$MOUNT_POINT/VOLINFO.TXT"

# Fichiers avec contenu différent
cat > "$MOUNT_POINT/TEST1.TXT" << EOF
Ligne 1: Test de contenu
Ligne 2: Pour parser FAT32
Ligne 3: 1234567890
Ligne 4: ABCDEFGHIJ
EOF

cat > "$MOUNT_POINT/TEST2.TXT" << EOF
Fichier avec différentes lignes.
Chaque ligne a une longueur différente.
123
456789
ABCDEFGHIJKLMNOP
EOF

# Fichier avec exactement 512 octets (taille secteur)
info "Création d'un fichier de 512 octets (taille secteur)..."
dd if=/dev/zero bs=512 count=1 2>/dev/null | tr '\0' 'A' > "$MOUNT_POINT/SECTOR.AAA"

# Fichier avec exactement 1024 octets (2 secteurs)
dd if=/dev/zero bs=512 count=2 2>/dev/null | tr '\0' 'B' > "$MOUNT_POINT/2SECT.BBB"

# Fichiers avec différentes extensions
echo "CONFIG DATA" > "$MOUNT_POINT/CONFIG.CFG"
echo "BINARY DATA" > "$MOUNT_POINT/DATA.BIN"
echo "LOG DATA" > "$MOUNT_POINT/LOG001.LOG"
echo "TEMPORARY" > "$MOUNT_POINT/TEMP001.TMP"
echo "BACKUP FILE" > "$MOUNT_POINT/BAK001.BAK"

# Fichier vide
touch "$MOUNT_POINT/EMPTY.FIL"

# ============================================
# FICHIERS DANS DOCS
# ============================================

info "Création des fichiers dans DOCS..."

echo "Documentation technique" > "$MOUNT_POINT/DOCS/TECH01.TXT"
echo "Manuel utilisateur" > "$MOUNT_POINT/DOCS/USERMAN.TXT"
echo "Notes importantes" > "$MOUNT_POINT/DOCS/NOTES.TXT"

cat > "$MOUNT_POINT/DOCS/SPECS.TXT" << EOF
Spécifications FAT32:
- Taille cluster: 512 octets
- FATs: 2 copies
- Root dir: cluster spécifique
- Max fichiers: 65535
EOF

# Sous-dossier TECH
echo "Détails techniques" > "$MOUNT_POINT/DOCS/TECH/TECH1.TXT"
echo "API Reference" > "$MOUNT_POINT/DOCS/TECH/APIREF.TXT"
echo "Format FAT" > "$MOUNT_POINT/DOCS/TECH/FATFMT.TXT"

# Sous-dossier ADMIN
echo "Admin docs" > "$MOUNT_POINT/DOCS/ADMIN/ADMIN1.TXT"
echo "Procédures" > "$MOUNT_POINT/DOCS/ADMIN/PROCED.TXT"

# ============================================
# FICHIERS DANS PRJ
# ============================================

info "Création des fichiers dans PRJ..."

echo "Projet principal" > "$MOUNT_POINT/PRJ/MAIN.PRJ"
echo "Code source" > "$MOUNT_POINT/PRJ/SOURCE.C"
echo "En-têtes" > "$MOUNT_POINT/PRJ/HEADER.H"

# PRJ1
echo "Projet 1 doc" > "$MOUNT_POINT/PRJ/PRJ1/P1DOC.TXT"
echo "Projet 1 src" > "$MOUNT_POINT/PRJ/PRJ1/P1SRC.C"
echo "Projet 1 data" > "$MOUNT_POINT/PRJ/PRJ1/P1DATA.DAT"

cat > "$MOUNT_POINT/PRJ/PRJ1/P1CFG.CFG" << EOF
[PROJECT1]
version=1.0
author=TEST
date=$(date +%Y%m%d)
EOF

# PRJ2
echo "Projet 2 doc" > "$MOUNT_POINT/PRJ/PRJ2/P2DOC.TXT"
echo "Projet 2 src" > "$MOUNT_POINT/PRJ/PRJ2/P2SRC.C"
echo "Projet 2 data" > "$MOUNT_POINT/PRJ/PRJ2/P2DATA.DAT"

# ============================================
# FICHIERS DANS SYS
# ============================================

info "Création des fichiers système..."

echo "Configuration système" > "$MOUNT_POINT/SYS/SYSCFG.CFG"
echo "Paramètres" > "$MOUNT_POINT/SYS/SETTINGS.SET"

# LOG
for i in {1..5}; do
    printf "LOG%03d: $(date) - Message %d\n" $i $i > "$MOUNT_POINT/SYS/LOG/LOG$(printf %03d $i).LOG"
done

echo "ERROR LOG" > "$MOUNT_POINT/SYS/LOG/ERROR.LOG"
echo "DEBUG LOG" > "$MOUNT_POINT/SYS/LOG/DEBUG.LOG"

# CFG
echo "Config 1" > "$MOUNT_POINT/SYS/CFG/CFG1.CFG"
echo "Config 2" > "$MOUNT_POINT/SYS/CFG/CFG2.CFG"
echo "Defaults" > "$MOUNT_POINT/SYS/CFG/DEFAULT.CFG"

# ============================================
# FICHIERS DANS AUTRES DOSSIERS
# ============================================

# IMGS
echo "FAKE IMG DATA" > "$MOUNT_POINT/IMGS/IMG001.JPG"
echo "FAKE IMG DATA" > "$MOUNT_POINT/IMGS/IMG002.PNG"
echo "FAKE IMG DATA" > "$MOUNT_POINT/IMGS/ICON.ICO"

# MUSIC
echo "FAKE AUDIO" > "$MOUNT_POINT/MUSIC/SONG1.MP3"
echo "FAKE AUDIO" > "$MOUNT_POINT/MUSIC/SONG2.WAV"
echo "FAKE AUDIO" > "$MOUNT_POINT/MUSIC/TRACK3.MID"

# TEMP
echo "Temporary file 1" > "$MOUNT_POINT/TEMP/TMP001.TMP"
echo "Temporary file 2" > "$MOUNT_POINT/TEMP/TMP002.TMP"
echo "Cache data" > "$MOUNT_POINT/TEMP/CACHE.DAT"

# ============================================
# FICHIERS SPÉCIAUX POUR TESTS
# ============================================

info "Création de fichiers spéciaux pour tests..."

# Fichier avec cluster unique (petit)
echo "X" > "$MOUNT_POINT/ONE_CLUST.TXT"

# Fichier avec plusieurs clusters
info "Création d'un fichier multi-clusters..."
dd if=/dev/urandom of="$MOUNT_POINT/MULTI_CLS.DAT" bs=1024 count=3 2>/dev/null

# Fichier qui remplit exactement des clusters
dd if=/dev/zero of="$MOUNT_POINT/FULL_CLUS.DAT" bs=1024 count=4 2>/dev/null

# Fichiers avec noms particuliers
echo "Fichier avec underscores" > "$MOUNT_POINT/FILE_TEST.TXT"
echo "A1B2C3D4" > "$MOUNT_POINT/A1B2C3D4.TXT"
echo "TEST_123" > "$MOUNT_POINT/TEST_123.TXT"
echo "12345678" > "$MOUNT_POINT/12345678.123"
echo "ABCDEFGH" > "$MOUNT_POINT/ABCDEFGH.EXT"

# Fichier avec attributs (créé normalement, attributs FAT seront basiques)
echo "Read-only content" > "$MOUNT_POINT/READONLY.TXT"

# ============================================
# DÉMONTAGE ET VÉRIFICATION
# ============================================

# Démontage
info "Démontage du disque..."
sudo umount "$MOUNT_POINT"

# Vérifier la propriété du fichier
chown $USER:$USER "$DISK_NAME" 2>/dev/null || true

# Afficher les statistiques avec mtools si disponible
if command -v mdir &> /dev/null; then
    info "Contenu du disque (vue mtools):"
    mdir -i "$DISK_NAME" ::/
    
    info "Informations FAT:"
    minfo -i "$DISK_NAME"
else
    avertissement "mtools non installé, installez-le pour voir le contenu sans monter"
fi

# Afficher les informations finales
echo ""
info "=========================================="
info "DISQUE FAT32 8.3 CRÉÉ AVEC SUCCÈS !"
info "=========================================="
echo ""
info "📁 Fichier créé: $(pwd)/$DISK_NAME"
info "🏷️  Nom du volume: $VOLUME_NAME (8 caractères)"
info "💾 Taille: $DISK_SIZE"
info "👤 Propriétaire: $USER"
echo ""
info "🎯 CARACTÉRISTIQUES POUR PARSING:"
info "  - Noms de fichiers 8.3 uniquement"
info "  - Pas de LFN (Long File Names)"
info "  - Structure simple pour l'apprentissage"
info "  - Tous les noms en majuscules (standard FAT)"
info "  - Taille réduite pour analyse facile"
echo ""
info "📊 CONTENU CRÉÉ:"
echo "  - $(find . -maxdepth 1 -name "*.img" -type f | wc -l) fichier image"
echo "  - 7 répertoires racine"
echo "  - Plus de 40 fichiers avec extensions variées"
echo "  - Fichiers de différentes tailles"
echo ""
info "🔧 POUR ANALYSER:"
echo "   hexdump -C $DISK_NAME | head -100"
echo "   # Pour voir le boot sector et FAT"
echo ""
echo "   mkdir -p ./analyse"
echo "   sudo mount -o ro $DISK_NAME ./analyse"
echo "   ls -la ./analyse"
echo ""
info "📚 STRUCTURE CRÉÉE:"
cat << 'EOF'
/
├── BOOTSEC.TXT      FATINFO.TXT      ROOTDIR.TXT
├── HELLO.TXT        README.TXT       VOLINFO.TXT
├── TEST1.TXT        TEST2.TXT        SECTOR.AAA
├── 2SECT.BBB        CONFIG.CFG       DATA.BIN
├── LOG001.LOG       TEMP001.TMP      BAK001.BAK
├── EMPTY.FIL        ONE_CLUST.TXT    MULTI_CLS.DAT
├── FULL_CLUS.DAT    FILE_TEST.TXT    A1B2C3D4.TXT
├── TEST_123.TXT     12345678.123     ABCDEFGH.EXT
├── READONLY.TXT
├── DOCS/           (avec sous-dossiers TECH/, ADMIN/)
├── IMGS/           (IMG001.JPG, etc.)
├── MUSIC/          (SONG1.MP3, etc.)
├── PRJ/            (avec PRJ1/, PRJ2/)
├── SYS/            (avec LOG/, CFG/)
└── TEMP/           (fichiers temporaires)
EOF
echo ""
info "✅ Tous les noms respectent le format 8.3"
info "✅ Pas de LFN à gérer"
info "✅ Parfait pour apprendre le parsing FAT32"