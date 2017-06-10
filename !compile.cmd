@echo off

SET EMSDKPATH=C:\bin\emsdk

CALL %EMSDKPATH%\emsdk_env.bat 

SET SOURCE=
SET SOURCE=%SOURCE% emubase\Disasm.cpp
SET SOURCE=%SOURCE% emubase\Board.cpp
SET SOURCE=%SOURCE% emubase\Processor.cpp
SET SOURCE=%SOURCE% emubase\Floppy.cpp
SET SOURCE=%SOURCE% Emulator.cpp

SET EMBED=--embed-file monit10.rom
SET EMBED=%EMBED% --embed-file basic10_1.rom --embed-file basic10_2.rom --embed-file basic10_3.rom

@echo on

@rem emcc %SOURCE% -s WASM=1 -o emul.html  2>emcc.log
@rem emcc %SOURCE% -s WASM=1 -s SAFE_HEAP=1 -o emul.html --shell-file shell_minimal.html
emcc %SOURCE% -s WASM=1 -O2 -s NO_EXIT_RUNTIME=1 -o emul.html --shell-file shell_minimal.html %EMBED%

