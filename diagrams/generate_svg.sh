#!/bin/bash

# SPDX-FileCopyrightText: 2026 ENDUROSAT EAD
# SPDX-License-Identifier: LicenseRef-EnduroSat-1.0

# PlantUML SVG Generator Script
# Generates SVG diagrams from PlantUML (.puml) files in the diagrams directory
#
# Usage:
#   ./generate_svg.sh                    # Generate all diagrams
#   ./generate_svg.sh diagram_name       # Generate specific diagram
#   ./generate_svg.sh --help             # Show help
#
# Requirements:
#   - PlantUML JAR file or plantuml command available
#   - Java runtime environment

# Note: We don't use 'set -e' here because we want to handle errors gracefully
# and continue processing other files even if one fails

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIAGRAMS_DIR="$SCRIPT_DIR"
OUTPUT_DIR="$DIAGRAMS_DIR/svg"
PLANTUML_JAR_URL="https://github.com/plantuml/plantuml/releases/latest/download/plantuml.jar"
PLANTUML_JAR="$SCRIPT_DIR/plantuml.jar"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show help
show_help() {
    cat << EOF
PlantUML SVG Generator

USAGE:
    $0 [OPTIONS] [DIAGRAM_NAME]

OPTIONS:
    --help, -h          Show this help message
    --clean, -c         Clean output directory before generating
    --download, -d      Download latest PlantUML JAR
    --verbose, -v       Verbose output

ARGUMENTS:
    DIAGRAM_NAME        Name of specific diagram to generate (without .puml extension)
                       If not specified, all diagrams will be generated

EXAMPLES:
    $0                          # Generate all diagrams
    $0 fp_arm_and_fire         # Generate only fp_arm_and_fire.puml
    $0 --clean                 # Clean and generate all diagrams
    $0 --download              # Download PlantUML and generate all diagrams

REQUIREMENTS:
    - Java runtime environment
    - PlantUML JAR file (will be downloaded automatically if not found)

OUTPUT:
    SVG files will be generated in: $OUTPUT_DIR
EOF
}

# Function to check if Java is available
check_java() {
    if ! command -v java &> /dev/null; then
        print_error "Java is not installed or not in PATH"
        print_info "Please install Java runtime environment"
        exit 1
    fi

    local java_version=$(java -version 2>&1 | head -n 1)
    print_info "Using Java: $java_version"
}

# Function to download PlantUML JAR
download_plantuml() {
    print_info "Downloading PlantUML JAR from $PLANTUML_JAR_URL"

    if command -v curl &> /dev/null; then
        curl -L -o "$PLANTUML_JAR" "$PLANTUML_JAR_URL"
    elif command -v wget &> /dev/null; then
        wget -O "$PLANTUML_JAR" "$PLANTUML_JAR_URL"
    else
        print_error "Neither curl nor wget is available for downloading PlantUML"
        print_info "Please download PlantUML JAR manually from: $PLANTUML_JAR_URL"
        print_info "Save it as: $PLANTUML_JAR"
        exit 1
    fi

    if [[ -f "$PLANTUML_JAR" ]]; then
        print_success "PlantUML JAR downloaded successfully"
    else
        print_error "Failed to download PlantUML JAR"
        exit 1
    fi
}

# Function to check PlantUML availability
check_plantuml() {
    # First try to use system plantuml command
    if command -v plantuml &> /dev/null; then
        PLANTUML_CMD="plantuml"
        print_info "Using system PlantUML command"
        return 0
    fi

    # If not available, check for JAR file
    if [[ ! -f "$PLANTUML_JAR" ]]; then
        print_warning "PlantUML JAR not found at $PLANTUML_JAR"
        read -p "Download PlantUML JAR automatically? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            download_plantuml
        else
            print_error "PlantUML is required to generate diagrams"
            print_info "You can:"
            print_info "1. Install plantuml package (e.g., 'apt install plantuml' or 'brew install plantuml')"
            print_info "2. Download PlantUML JAR manually and save as $PLANTUML_JAR"
            print_info "3. Run this script with --download option"
            exit 1
        fi
    fi

    PLANTUML_CMD="java -jar $PLANTUML_JAR"
    print_info "Using PlantUML JAR: $PLANTUML_JAR"
}

# Function to create output directory
create_output_dir() {
    if [[ ! -d "$OUTPUT_DIR" ]]; then
        mkdir -p "$OUTPUT_DIR"
        print_info "Created output directory: $OUTPUT_DIR"
    fi
}

# Function to clean output directory
clean_output_dir() {
    if [[ -d "$OUTPUT_DIR" ]]; then
        print_info "Cleaning output directory: $OUTPUT_DIR"
        rm -rf "$OUTPUT_DIR"/*.svg 2>/dev/null || true
    fi
}

# Function to generate SVG from PlantUML file
generate_svg() {
    local puml_file="$1"
    local base_name=$(basename "$puml_file" .puml)
    local svg_file="$OUTPUT_DIR/${base_name}.svg"

    print_info "Generating SVG for: $puml_file"

    # Execute PlantUML command and capture exit code
    local exit_code=0
    if [[ $VERBOSE == true ]]; then
        $PLANTUML_CMD -tsvg -o "$OUTPUT_DIR" "$puml_file" || exit_code=$?
    else
        $PLANTUML_CMD -tsvg -o "$OUTPUT_DIR" "$puml_file" > /dev/null 2>&1 || exit_code=$?
    fi

    # Check if SVG file was created successfully
    if [[ -f "$svg_file" && $exit_code -eq 0 ]]; then
        print_success "Generated: $svg_file"
        return 0
    else
        print_error "Failed to generate: $svg_file (PlantUML exit code: $exit_code)"
        return 1
    fi
}

# Function to find and process PlantUML files
process_diagrams() {
    local target_diagram="$1"
    local puml_files=()
    local generated_count=0
    local failed_count=0

    # Find PlantUML files
    if [[ -n "$target_diagram" ]]; then
        local target_file="$DIAGRAMS_DIR/${target_diagram}.puml"
        if [[ -f "$target_file" ]]; then
            puml_files=("$target_file")
        else
            print_error "Diagram not found: $target_file"
            exit 1
        fi
    else
        while IFS= read -r -d '' file; do
            puml_files+=("$file")
        done < <(find "$DIAGRAMS_DIR" -maxdepth 1 -name "*.puml" -print0)
    fi

    if [[ ${#puml_files[@]} -eq 0 ]]; then
        print_warning "No PlantUML files found in $DIAGRAMS_DIR"
        exit 0
    fi

    print_info "Found ${#puml_files[@]} PlantUML file(s) to process"

    # Debug: List all files to be processed
    if [[ $VERBOSE == true ]]; then
        print_info "Files to process:"
        for file in "${puml_files[@]}"; do
            print_info "  - $file"
        done
    fi

    # Process each file
    for puml_file in "${puml_files[@]}"; do
        if generate_svg "$puml_file"; then
            ((generated_count++))
        else
            ((failed_count++))
        fi
    done

    # Summary
    echo
    print_info "Generation complete:"
    print_success "  Successfully generated: $generated_count"
    if [[ $failed_count -gt 0 ]]; then
        print_error "  Failed: $failed_count"
    fi
    print_info "  Output directory: $OUTPUT_DIR"
}

# Main function
main() {
    local target_diagram=""
    local clean_output=false
    local download_jar=false
    VERBOSE=false

    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --help|-h)
                show_help
                exit 0
                ;;
            --clean|-c)
                clean_output=true
                shift
                ;;
            --download|-d)
                download_jar=true
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            -*)
                print_error "Unknown option: $1"
                print_info "Use --help for usage information"
                exit 1
                ;;
            *)
                if [[ -z "$target_diagram" ]]; then
                    target_diagram="$1"
                else
                    print_error "Multiple diagram names specified"
                    print_info "Use --help for usage information"
                    exit 1
                fi
                shift
                ;;
        esac
    done

    print_info "PlantUML SVG Generator"
    print_info "Working directory: $DIAGRAMS_DIR"

    # Check prerequisites
    check_java

    # Download PlantUML if requested
    if [[ $download_jar == true ]]; then
        download_plantuml
    fi

    check_plantuml
    create_output_dir

    # Clean output directory if requested
    if [[ $clean_output == true ]]; then
        clean_output_dir
    fi

    # Process diagrams
    process_diagrams "$target_diagram"
}

# Run main function with all arguments
main "$@"
