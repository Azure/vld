REM Copying over Visual Leak Detector Dependencies
xcopy "%~dp0vld.ini" "%~3" /y
xcopy "%~dp0..\bin\%1\vld_%2.dll" "%~3" /y /d
xcopy "%~dp0..\bin\%1\vld_%2.pdb" "%~3" /y /d
xcopy "%~dp0..\setup\dbghelp\%2\symsrv.dll" "%~3" /y /d
xcopy "%~dp0..\setup\dbghelp\%2\dbghelp.dll" "%~3" /y /d
xcopy "%~dp0..\setup\dbghelp\%2\Microsoft.DTfW.DHL.manifest" "%~3" /y /d