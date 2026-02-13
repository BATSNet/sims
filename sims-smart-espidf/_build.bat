@echo off
call "C:\Users\pp\esp\v5.5-rc1\esp-idf\export.bat" > nul 2>&1
cd /d "C:\java\workspace\sims-bw\sims-smart-espidf"
idf.py build > "C:\java\workspace\sims-bw\sims-smart-espidf\_build_output.txt" 2>&1
echo BUILD_EXIT_CODE=%errorlevel% >> "C:\java\workspace\sims-bw\sims-smart-espidf\_build_output.txt"
