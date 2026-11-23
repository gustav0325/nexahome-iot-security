# Carrega ESP-IDF v5.3.2 e QEMU (Espressif) no PATH
export IDF_PYTHON_CHECK_CONSTRAINTS=no
. /home/claude/tools/esp-idf/export.sh >/dev/null 2>&1
export PATH=/home/claude/tools/qemu/bin:$PATH
