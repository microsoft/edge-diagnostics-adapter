@echo off

REM Node-gyp currently does not support vs2017 build tools. We set the C++ build tools v140(vs2015).
REM If you have installed vs2017 enterprise edition:

IF "%VSVERSION%"=="" GOTO :VS2015

IF %VSVERSION%=="2017" (
    set VCTargetsPath=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\Common7\IDE\VC\VCTargets
    GOTO :COMMON
)

:VS2015
set VCTargetsPath=C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\v140

:COMMON
set GYP_MSVS_VERSION=2015
