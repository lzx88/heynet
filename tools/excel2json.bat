@echo off
set root=%~dp0
pushd %root%
python excel_converter.py %root%../test_excel json %root%../server/.config s .json
::python excel_converter.py %root%../test_excel json %root%client c .json
popd
pause
