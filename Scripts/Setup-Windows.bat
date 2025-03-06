@echo off

pushd ..
if not defined HYDRA_ENGINE ( setx HYDRA_ENGINE "%CD%" ) 
ThirdParty\Premake\Windows\premake5.exe --file=premake.lua vs2022
popd

pause