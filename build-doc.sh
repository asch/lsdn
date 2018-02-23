#!/bin/bash
set -eu


mkdir -p doxygen doxygen-full

cp Doxyfile.template doxygen-full/Doxyfile
cat >> doxygen-full/Doxyfile <<EOF
INPUT = netmodel lsdn lsctl
OUTPUT_DIRECTORY = doxygen-full
EOF

cp Doxyfile.template doxygen/Doxyfile
cat >> doxygen/Doxyfile <<EOF
INPUT = netmodel
OUTPUT_DIRECTORY = doxygen
EXCLUDE_PATTERNS = */netmodel/private/*
EOF

doxygen doxygen-full/Doxyfile
doxygen doxygen/Doxyfile

if [ "${1:-}" != "doxygen-only" ]; then
    sphinx-build -b html docs/ docs/html
    sphinx-build -b latex docs/ docs/pdf
    make -C docs/pdf
fi
