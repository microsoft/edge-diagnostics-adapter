@echo off

IF DEFINED APPVEYOR (
    ECHO Building in AppVeyor
) ELSE (
    ECHO Setting VCTargetsPath
    set VCTargetsPath=\"C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\v140\"
)
set GYP_MSVS_VERSION=2015
