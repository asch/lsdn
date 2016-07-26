Testy lze přidávat tak, že se sem vyrobí soubor `test_cokoliv.sh`
a pak se do `CMakeLists.txt` přidá řádek:
```
    test_in_ns(test_cokoliv)
```

Testy musí vracet 0 pokud projdou, něco jiného pokud ne.

Zatím je to spouštění dost na pikaču:
```
$ su
# make test-setup
# make test
# make test-teardown
```

Tohle bude zabalené v nějakém skriptu.

Navíc tenhle náš jediný dosavadní `test_switches` neprojde při opakovaném
spuštění `make test`, protože v sobě testuje, jestli ten ping funguje
před zavedením routovacích pravidel... a protože po testu jsou zavedená,
tak funguje, a tak test selže.

Dost možná bude potřeba pro každý jednotlivý test spouštět setup a teardown?

Jo a ještě navíc je známý problém cmake, že dependencí "make test" není "make all",
takže to nejde jen tak mýrnyks týrnyks spustit, musí být aktuální vybuilděný systém.
