// Host-test stand-in for the ESP-IDF generated sdkconfig.h (no Kconfig on the host).
//
// It is empty on purpose: every CONFIG_* symbol is then undefined, i.e. every optional
// feature of main/feature_config.h is off and the code under test is the upstream
// code. A test target that exercises an optional feature defines the CONFIG_FEATURE_*
// / CONFIG_FORK_* symbols it needs on its own compile line (see CMakeLists.txt).
