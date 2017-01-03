@echo off
set root=%~dp0
pushd %root%
python excel_converter.py %root%test_excel lua %root%../server/.config s .cfg
::python excel_converter.py %root%test_excel lua %root%client c .lua
popd
pause
