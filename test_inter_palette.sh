#!/usr/bin/env bash
# Test script for MR 2707: RTC palette on inter-frame intra blocks for screen content
# Runs the exact test matrix from the MR: preset 8, --rtc 1 --scm 1 --lp 1
# CQP: --rc 0 --cqp {23,31,43,55}  (or 27,35,43,51 per reviewer)
# CBR: --rc 2 --tbr {750,1500,2500,4000}
# Frames: 60 (reviewer) or 130 (MR) - configurable
# Output: CSV with BD-rate data

set -euo pipefail

# ===== CONFIGURATION =====
ENCODER="/home/ckristian/SVT-AV1/Bin/Release/SvtAv1EncApp"
PRESET=8
RTC=1
SCM=1
LP=1
FRAMES=60  # Change to 130 for full clip (per MR)
CQP_LIST=(23 31 43 55)  # Change to (27 35 43 51) to match reviewer
CBR_LIST=(750 1500 2500 4000)
RESULTS_DIR="/home/ckristian/svt_test_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

# ===== CLIPS TO TEST =====
# Format: "clip_name|path|width|height|fps_num|fps_denom|bit_depth|color_format"
# Add your clips here. Use y4m for auto-detection, or specify params for .yuv
CLIPS=(
    # Available clips from aom-test-data
    "screendata_1080|/tmp/screendata_1080.y4m|1920|1080|30|1|8|420"
    "screendata_640|/tmp/screendata.y4m|640|480|30|1|8|420"
    "Debugging_480x270|/home/ckristian/SVT-AV1/test/vectors/Debugging_480x270p3000_yuv420p_20frames.yuv|480|270|3000|1000|8|420"
    "wikipedia_360p|/home/ckristian/SVT-AV1/test/vectors/wikipedia_420_360p_60f.y4m|640|360|60|1|8|420"
    # Add b2_scc clips here when available:
    # "Debugging_b2|/path/to/Debugging_1080p.y4m|1920|1080|30|1|8|420"
    # "Wikipedia_b2|/path/to/Wikipedia_1080p.y4m|1920|1080|30|1|8|420"
    # "Slides1_b2|/path/to/Slides1_1080p.y4m|1920|1080|30|1|8|420"
    # "Spreadsheet_b2|/path/to/Spreadsheet_1080p.y4m|1920|1080|30|1|8|420"
    # "MobileDeviceScreenSharing_b2|/path/to/MobileDeviceScreenSharing_1080p.y4m|1920|1080|30|1|8|420"
    # "SlideShow_720p_b2|/path/to/SlideShow_720p.y4m|1280|720|30|1|8|420"
    # "SceneComposition_1_b2|/path/to/SceneComposition_1_1080p.y4m|1920|1080|30|1|8|420"
    # "Wikipedia_10bit_b2|/path/to/Wikipedia_10bit_1080p.y4m|1920|1080|30|1|10|420"
    # "ChinaSpeed_b2|/path/to/ChinaSpeed_1080p.y4m|1920|1080|30|1|8|420"
    # "KristenAndSaraScreen_b2|/path/to/KristenAndSaraScreen_1080p.y4m|1920|1080|30|1|8|420"
    # "MissionControl_b2|/path/to/MissionControl_1080p.y4m|1920|1080|30|1|8|420"
    # "Slides2_b2|/path/to/Slides2_1080p.y4m|1920|1080|30|1|8|420"
)

# ===== MAIN =====
echo "========================================="
echo "MR 2707 Test Suite - RTC Inter-Palette"
echo "========================================="
echo "Encoder: $ENCODER"
echo "Preset: $PRESET, RTC: $RTC, SCM: $SCM, LP: $LP"
echo "Frames: $FRAMES"
echo "CQP: ${CQP_LIST[*]}"
echo "CBR: ${CBR_LIST[*]}"
echo "Results dir: $RESULTS_DIR"
echo "Clips: ${#CLIPS[@]}"
echo "========================================="

# Verify encoder exists
if [[ ! -x "$ENCODER" ]]; then
    echo "ERROR: Encoder not found at $ENCODER"
    exit 1
fi

# Build baseline (FTR_RTC_INTER_PALETTE=0)
echo "Building baseline (feature OFF)..."
cd /home/ckristian/SVT-AV1
sed -i 's/#define FTR_RTC_INTER_PALETTE       1/#define FTR_RTC_INTER_PALETTE       0/' Source/API/EbDebugMacros.h
./Build/linux/build.sh -x --release --enable-lto 2>&1 | tail -5
cp "$ENCODER" "$ENCODER.baseline"
echo "Baseline built: $ENCODER.baseline"

# Build MR (FTR_RTC_INTER_PALETTE=1)
echo "Building MR (feature ON)..."
sed -i 's/#define FTR_RTC_INTER_PALETTE       0/#define FTR_RTC_INTER_PALETTE       1/' Source/API/EbDebugMacros.h
./Build/linux/build.sh -x --release --enable-lto 2>&1 | tail -5
echo "MR built: $ENCODER"

# Function to run single encode
run_encode() {
    local encoder="$1"
    local clip_name="$2"
    local input_file="$3"
    local width="$4"
    local height="$5"
    local fps_num="$6"
    local fps_denom="$7"
    local bit_depth="$8"
    local color_fmt="$9"
    local rc_mode="${10}"
    local qp_or_tbr="${11}"
    local output_file="${12}"
    local stat_file="${13}"

    local cmd=("$encoder" -i "$input_file" -w "$width" -h "$height" -n "$FRAMES"
        --preset "$PRESET" --rtc "$RTC" --scm "$SCM" --lp "$LP"
        --input-depth "$bit_depth" --color-format "$color_fmt"
        --fps-num "$fps_num" --fps-denom "$fps_denom"
        --rc "$rc_mode" -b "$output_file" --enable-stat-report 1 --stat-file "$stat_file"
        --progress 0)

    if [[ "$rc_mode" == "0" ]]; then
        cmd+=(--cqp "$qp_or_tbr" --aq-mode 0)
    else
        cmd+=(--tbr "$qp_or_tbr")
    fi

    echo "  Running: ${cmd[*]}"
    "${cmd[@]}" 2>&1 | tail -3
}

# Function to parse PSNR from stat file
parse_psnr() {
    local stat_file="$1"
    if [[ -f "$stat_file" ]]; then
        # SVT-AV1 stat file format: frame, psnr_y, psnr_u, psnr_v, ssim
        awk -F',' 'NR>1 {sum+=$2; count++} END {if(count>0) print sum/count; else print "N/A"}' "$stat_file"
    else
        echo "N/A"
    fi
}

# Main test loop
for clip_entry in "${CLIPS[@]}"; do
    IFS='|' read -r clip_name input_file width height fps_num fps_denom bit_depth color_fmt <<< "$clip_entry"

    if [[ ! -f "$input_file" ]]; then
        echo "SKIP: $clip_name - file not found: $input_file"
        continue
    fi

    echo ""
    echo "===== Testing clip: $clip_name ($width x $height) ====="
    clip_dir="$RESULTS_DIR/$clip_name"
    mkdir -p "$clip_dir"

    # Run CQP tests
    for cqp in "${CQP_LIST[@]}"; do
        echo "  CQP $cqp..."
        run_encode "$ENCODER.baseline" "$clip_name" "$input_file" "$width" "$height" "$fps_num" "$fps_denom" "$bit_depth" "$color_fmt" 0 "$cqp" "$clip_dir/mr_cqp${cqp}.ivf" "$clip_dir/mr_cqp${cqp}.csv"
        run_encode "$ENCODER" "$clip_name" "$input_file" "$width" "$height" "$fps_num" "$fps_denom" "$bit_depth" "$color_fmt" 0 "$cqp" "$clip_dir/base_cqp${cqp}.ivf" "$clip_dir/base_cqp${cqp}.csv"
    done

    # Run CBR tests
    for tbr in "${CBR_LIST[@]}"; do
        echo "  CBR ${tbr}kbps..."
        run_encode "$ENCODER.baseline" "$clip_name" "$input_file" "$width" "$height" "$fps_num" "$fps_denom" "$bit_depth" "$color_fmt" 2 "$tbr" "$clip_dir/mr_cbr${tbr}.ivf" "$clip_dir/mr_cbr${tbr}.csv"
        run_encode "$ENCODER" "$clip_name" "$input_file" "$width" "$height" "$fps_num" "$fps_denom" "$bit_depth" "$color_fmt" 2 "$tbr" "$clip_dir/base_cbr${tbr}.ivf" "$clip_dir/base_cbr${tbr}.csv"
    done
done

echo ""
echo "========================================="
echo "All encodes complete. Computing BD-rates..."
echo "========================================="

# Generate summary CSV
SUMMARY_CSV="$RESULTS_DIR/summary.csv"
echo "clip,config,type,value,base_psnr,mr_psnr,psnr_delta,base_bitrate,mr_bitrate,bitrate_delta" > "$SUMMARY_CSV"

# TODO: Add BD-rate computation using bjdelta or similar
# For now, output the stat files for manual analysis

echo "Results in: $RESULTS_DIR"
echo "Manual BD-rate analysis needed - use bjdelta tool on stat files"
