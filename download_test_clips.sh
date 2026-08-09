#!/usr/bin/env bash
# Download available test clips from aom-test-data bucket

set -euo pipefail

OUT_DIR="/home/ckristian/SVT-AV1/test/vectors"
mkdir -p "$OUT_DIR"

echo "Downloading available test clips..."

# 1. screendata 640x480
echo "  Downloading screendata.y4m..."
curl -L "https://gitlab.com/AOMediaCodec/aom-testing/-/raw/master/test-files/screendata.y4m.zst" -o /tmp/screendata.y4m.zst 2>/dev/null
zstd -d /tmp/screendata.y4m.zst -o "$OUT_DIR/screendata.y4m" 2>/dev/null
rm /tmp/screendata.y4m.zst

# 2. screendata 1920x1080
echo "  Downloading screendata.1920_1080.y4m..."
curl -L "https://storage.googleapis.com/aom-test-data/screendata.1920_1080.y4m" -o "$OUT_DIR/screendata_1920_1080.y4m" 2>/dev/null

# 3. Debugging 480x270 (from aom-test-data)
echo "  Downloading Debugging_480x270..."
curl -L "https://storage.googleapis.com/aom-test-data/Debugging_480x270p3000_yuv420p_20frames.yuv" -o "$OUT_DIR/Debugging_480x270p3000_yuv420p_20frames.yuv" 2>/dev/null

# 4. wikipedia 420 360p
echo "  Downloading wikipedia_420_360p..."
curl -L "https://storage.googleapis.com/aom-test-data/wikipedia_420_360p_60f.y4m" -o "$OUT_DIR/wikipedia_420_360p_60f.y4m" 2>/dev/null

# 5. akiyo_cif
echo "  Downloading akiyo_cif..."
curl -L "https://gitlab.com/AOMediaCodec/aom-testing/-/raw/master/test-files/akiyo_cif.y4m.zst" -o /tmp/akiyo_cif.y4m.zst 2>/dev/null
zstd -d /tmp/akiyo_cif.y4m.zst -o "$OUT_DIR/akiyo_cif.y4m" 2>/dev/null
rm /tmp/akiyo_cif.y4m.zst

echo ""
echo "Downloaded clips:"
ls -la "$OUT_DIR"/*.y4m "$OUT_DIR"/*.yuv 2>/dev/null | awk '{print "  " $9 " (" $5 " bytes)"}'

echo ""
echo "Note: b2_scc clips (Debugging, Wikipedia, Slides1, Spreadsheet, etc.)"
echo "are part of AOM Common Test Conditions and may require AOM membership."
echo "Add them to $OUT_DIR/ when available."
