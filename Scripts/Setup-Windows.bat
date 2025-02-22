@echo off

pushd ..
ThirdParty\Premake\Windows\premake5.exe --file=premake.lua vs2022
popd
pause