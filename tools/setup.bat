@echo off

set CHECK_PATH=.\tools\msys\bin

IF NOT EXIST %CHECK_PATH% (
    .\tools\7z\7za.exe x -y -r -aoa -o./tools ./tools/w800/toolchain/msys.7z
    echo - setup msys done !
) else (
    echo - found msys !, path: %CHECK_PATH%
)
