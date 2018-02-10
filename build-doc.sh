#!/bin/bash
doxygen doxygen/Doxyfile
sphinx-build -b html docs/ docs/html
sphinx-build -b latex docs/ docs/pdf
make -C docs/pdf
