To add a test, just create one or more files in `parts` directory. Tests are modular, thus you can
specify only one part of the test and combine it with another part already contained in `parts`.
The mandatory functions which have to be defined are `prepare()`, `connect()` and `test()`.

The complete test is specified in `CMakeLists.txt`, where you add line `test_parts()`, where you
specify all parts to be used in the test. E.g. `test_parts(vxlan_mcast basic ping)`.

Tests should be run under emulation because of stability and need of root permissions. For this
reason there is `run-qemu` script which will run all the tests. The documentation is part of the
comments inside the script. Read it.
