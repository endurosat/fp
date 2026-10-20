# PlantUML Diagrams

This directory contains PlantUML diagrams for the EnduroSat Function Protocol (FP) service implementation.

## Available Diagrams

- `fp_regular_dispatch.puml` - Regular FP client/server dispatch flow
- `fp_arm_and_fire.puml` - ARM-and-FIRE FP client/server scenario

## Generating SVG Diagrams

Use the provided bash script to generate SVG diagrams from the PlantUML files:

### Quick Start

```bash
# Generate all diagrams
./generate_svg.sh

# Generate specific diagram
./generate_svg.sh fp_arm_and_fire

# Clean and regenerate all diagrams
./generate_svg.sh --clean
```

### Prerequisites

- Java runtime environment
- PlantUML: a `plantuml` command, or `plantuml.jar` in this directory. If neither is
  found, the script asks before downloading the JAR; `--download` downloads the latest
  JAR without asking.

### Script Options

```bash
./generate_svg.sh [OPTIONS] [DIAGRAM_NAME]

OPTIONS:
    --help, -h          Show help message
    --clean, -c         Clean output directory before generating
    --download, -d      Download latest PlantUML JAR
    --verbose, -v       Verbose output

EXAMPLES:
    ./generate_svg.sh                    # Generate all diagrams
    ./generate_svg.sh fp_arm_and_fire   # Generate only fp_arm_and_fire.puml
    ./generate_svg.sh --clean           # Clean and generate all diagrams
    ./generate_svg.sh --download        # Download PlantUML and generate all
```

### Output

SVG files are generated in the `svg/` subdirectory:
- `svg/fp_regular_dispatch.svg`
- `svg/fp_arm_and_fire.svg`

## Manual Generation

If you prefer to use PlantUML directly:

```bash
# Using PlantUML JAR
java -jar plantuml.jar -tsvg *.puml

# Using system PlantUML command
plantuml -tsvg *.puml
```

## Viewing Diagrams

SVG files can be viewed in:
- Web browsers
- Image viewers that support SVG
- Text editors with SVG preview
- Documentation systems (Confluence, GitHub, etc.)

## Adding New Diagrams

1. Create a new `.puml` file in this directory
2. Use PlantUML syntax to define your diagram
3. Run `./generate_svg.sh` to generate the SVG version

## PlantUML Resources

- [PlantUML Official Site](https://plantuml.com/)
- [PlantUML Language Reference](https://plantuml.com/guide)
- [Sequence Diagram Syntax](https://plantuml.com/sequence-diagram)
