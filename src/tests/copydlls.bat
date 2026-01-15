REM Copying over Visual Leak Detector Dependencies
REM
REM Batch file parameter modifiers explained:
REM   %~dp0  = Drive letter and path of this batch file's directory (with trailing \)
REM   %1, %2, %3 = Command-line arguments 1, 2, and 3
REM
REM Example: If this batch file is at D:\r_arm\vld\src\tests\copydlls.bat
REM          then %~dp0 = D:\r_arm\vld\src\tests\
REM
xcopy "%~dp0\..\..\vld.ini" "%~dp0\..\bin\" /y /d
copy "%~dp0\..\bin\%1\vld_%2.dll" "%~3\vld_%2.dll" /y
copy "%~dp0\..\bin\%1\vld_%2.pdb" "%~3\vld_%2.pdb" /y
copy "%~dp0\..\..\setup\dbghelp\%2\dbghelp.dll" "%~3\dbghelp.dll" /y
if exist "%~dp0\..\..\setup\dbghelp\%2\Microsoft.DTfW.DHL.manifest" copy "%~dp0\..\..\setup\dbghelp\%2\Microsoft.DTfW.DHL.manifest" "%~3\Microsoft.DTfW.DHL.manifest" /y