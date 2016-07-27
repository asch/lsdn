Testy lze přidávat tak, že se sem vyrobí soubor `test_cokoliv.sh`
a pak se do `CMakeLists.txt` přidá řádek:
```
    test_in_ns(cokoliv)
```

Testy musí vracet 0 pokud projdou, něco jiného pokud ne.

Všechny testy spustíme jednoduše `make test`

Runner pro ně je `./test/harness.sh <jménotestu> argumenty`,
který před testem udělá setup, spustí test v namespace a
pak provede teardown.

Je známý problém cmake, že dependencí "make test" není "make all",
takže to nejde jen tak mýrnyks týrnyks spustit, musí být aktuální vybuilděný systém.
