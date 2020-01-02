nrfjprog.exe -f nrf52 --erasepage 0-0x60000
nrfjprog.exe -f nrf52 --sectorerase --program ..\..\ble_bin\s132_nrf52_3.0.0_softdevice.hex
nrfjprog.exe -f nrf52 --sectorerase --program ..\..\ble_bin\nrf52832_xxaa.hex
nrfjprog.exe -f nrf52 --reset

pause