#!/bin/bash
doxygen doxygen/Doxyfile
sphinx-build -b html docs/ docs/html
sphinx-build -D ispdf=yes -b latex docs/ docs/pdf
make -C docs/pdf
