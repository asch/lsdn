#!/bin/bash
set -eu


mkdir -p doxygen doxygen-full

cp Doxyfile.template doxygen-full/Doxyfile
echo 'INPUT = netmodel lsdn lsctl' >> doxygen-full/Doxyfile
echo 'OUTPUT_DIRECTORY = doxygen-full' >> doxygen-full/Doxyfile

cp Doxyfile.template doxygen/Doxyfile
echo 'INPUT = netmodel/include' >> doxygen/Doxyfile
echo 'OUTPUT_DIRECTORY = doxygen' >> doxygen/Doxyfile

doxygen doxygen-full/Doxyfile
doxygen doxygen/Doxyfile

if [ "${1:-}" != "doxygen-only" ]; then
    sphinx-build -b html docs/ docs/html
    sphinx-build -b latex docs/ docs/pdf
    make -C docs/pdf
fi
