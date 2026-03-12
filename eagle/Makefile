# Makefile for Eagle schematic netlist generation and CAM output
# Uses lxml library in a virtual environment for netlist generation
# Uses terriblefire78/eagle:v1 Docker container for CAM output

VENV_DIR = .venv
VENV_MARKER = $(VENV_DIR)/.installed
PYTHON = $(VENV_DIR)/bin/python
PIP = $(VENV_DIR)/bin/pip
NETLIST_SCRIPT = generate_netlist.py

# Find the .sch file
SCH_FILE = $(wildcard *.sch)
NET_FILE = $(SCH_FILE:.sch=.net)

# Variables for CAM output (Gerber generation)
BOARDPREFIX := tfpcmcia
BOARD := $(BOARDPREFIX).brd
CAMFILE := jlcpcb_4_layer_v9.cam
DRILL_CAMFILE = $(CAMFILE:.cam=_drill.cam)
GITVERSION := $(shell git rev-parse --short HEAD)
DATESTRING := $(shell date +"%Y_%m_%d")
GERBERS := $(BOARDPREFIX)_$(GITVERSION)_$(DATESTRING)
EAGLE_DOCKER := docker run --rm -v $(CURDIR):/work -w /work terriblefire78/eagle:v1 eagle

# Variables for PDF generation
PDF_FILE := $(BOARDPREFIX)_$(GITVERSION)_$(DATESTRING).pdf
EAGLE_PDF_DOCKER := docker run --rm --platform linux/amd64 -v $(CURDIR):/work -w /work terriblefire78/eagle-pdf python /app/generate_schematic_pdf.py

# Default target
.PHONY: all
all: $(NET_FILE) $(GERBERS).zip

# Create virtual environment and install dependencies
$(VENV_MARKER):
	@echo "Creating virtual environment..."
	python3 -m venv $(VENV_DIR)
	@echo "Installing dependencies lxml"
	$(PIP) install --upgrade pip
	$(PIP) install lxml 
	@touch $(VENV_MARKER)

# Generate netlist from schematic
.PHONY: netlist
netlist: $(NET_FILE)

$(NET_FILE): $(SCH_FILE) $(VENV_MARKER) $(NETLIST_SCRIPT)
	@echo "Generating netlist from $(SCH_FILE)..."
	$(PYTHON) $(NETLIST_SCRIPT) $(SCH_FILE) $(NET_FILE)
	@echo "Netlist generated: $(NET_FILE)"

$(DRILL_CAMFILE): $(CAMFILE)
	python3 extract_drill_cam.py $(CAMFILE) $(DRILL_CAMFILE)

.PHONY: pcf
pcf: $(PCF_FILE)

# CAM output (Gerber generation) targets
.PHONY: gerbers camfiles
gerbers: $(GERBERS).zip

camfiles: $(GERBERS)

$(GERBERS).zip: $(GERBERS)
	@echo "Creating Gerber archive..."
	zip -r $@ $(GERBERS)/* $(GERBERS)/*/*
	@echo "Gerber archive created: $@"

$(GERBERS): $(BOARD) $(CAMFILE)
	@echo "Generating Gerbers for $(BOARD) using $(CAMFILE)..."
	mkdir -p $(GERBERS)
	$(EAGLE_DOCKER) -X -dCAMJOB -j$(CAMFILE) -o$(GERBERS) $(BOARD)
	@echo "Gerbers generated in $(GERBERS)/"

# PDF generation target
.PHONY: pdf
pdf: $(PDF_FILE)

$(PDF_FILE): $(SCH_FILE)
	@echo "Generating PDF schematic from $(SCH_FILE)..."
	$(EAGLE_PDF_DOCKER) $(SCH_FILE) $(PDF_FILE)
	@echo "PDF schematic generated: $(PDF_FILE)"

# Assembly file generation targets
.PHONY: bom
bom: $(BOARDPREFIX)_bom.csv

.PHONY: cpl
cpl: $(BOARDPREFIX)_cpl.csv

.PHONY: assembly
assembly: bom cpl
	@echo "Generated BOM and CPL files for assembly"

$(BOARDPREFIX)_bom.csv: $(SCH_FILE) extract_bom.py
	@echo "Generating BOM from $(SCH_FILE)..."
	python3 extract_bom.py $(SCH_FILE) $@

$(BOARDPREFIX)_cpl.csv: $(BOARD) extract_cpl.py
	@echo "Generating CPL from $(BOARD)..."
	python3 extract_cpl.py $(BOARD) $@

# Clean generated files
.PHONY: clean
clean: clean-gerbers clean-pdf clean-bom clean-cpl clean-netlist

.PHONY: clean-netlist
clean-netlist:
	rm -f $(NET_FILE)
	@echo "Cleaned netlist files"

.PHONY: clean-pdf
clean-pdf:
	rm -f $(BOARDPREFIX)_*_*.pdf
	@echo "Cleaned PDF files"

.PHONY: clean-bom
clean-bom:
	rm -f $(BOARDPREFIX)_bom.csv
	@echo "Cleaned BOM files"

.PHONY: clean-cpl
clean-cpl:
	rm -f $(BOARDPREFIX)_cpl.csv
	@echo "Cleaned CPL files"

.PHONY: clean-gerbers
clean-gerbers:
	rm -rf $(BOARDPREFIX)_*_*/ 2>/dev/null || rm -rf $(BOARDPREFIX)_*_*/ 2>/dev/null || true
	rm -f $(BOARDPREFIX)_*_*.zip $(DRILL_CAMFILE)
	@echo "Cleaned Gerber files and directories"

# Clean everything including venv
.PHONY: distclean
distclean: clean clean-gerbers
	rm -rf $(VENV_DIR)
	@echo "Cleaned virtual environment"

# Show help
.PHONY: help
help:
	@echo "Eagle CAM Output Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Generate Everything"
	@echo "  netlist       - Generate netlist from schematic"
	@echo "  gerbers       - Generate Gerber files and create ZIP archive"
	@echo "  camfiles      - Generate Gerber files only (no ZIP)"
	@echo "  pdf           - Generate PDF schematic (using Docker)"
	@echo "  bom           - Generate Bill of Materials CSV from schematic"
	@echo "  cpl           - Generate Component Placement List CSV from board"
	@echo "  assembly      - Generate both BOM and CPL for assembly"
	@echo "  clean         - Remove all generated files"
	@echo "  clean-netlist - Remove netlist file"
	@echo "  clean-gerbers - Remove Gerber files and directories"
	@echo "  clean-pdf     - Remove PDF files"
	@echo "  clean-bom     - Remove BOM files"
	@echo "  clean-cpl     - Remove CPL files"
	@echo "  distclean     - Remove all generated files and virtual environment"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Input:  $(SCH_FILE), $(BOARD)"
	@echo "Output: $(NET_FILE), $(GERBERS).zip, $(PDF_FILE)"
