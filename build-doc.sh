#!/bin/bash
doxygen doxygen/Doxyfile

# needed for html_static_path in sphinx configuration, otherwise warning is generated
mkdir -p docs/_static

sphinx-build -b html docs/ docs/html
sphinx-build -D ispdf=yes -b latex docs/ docs/pdf
make -C docs/pdf
