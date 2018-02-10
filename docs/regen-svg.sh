#/bin/bash
for f in *.svg; do
    f=$(basename -s .svg $f)
    echo Converting $f
    inkscape --without-gui $f.svg --export-pdf $f.pdf
done
