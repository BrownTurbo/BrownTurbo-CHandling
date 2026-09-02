Get-ChildItem -Path "." -Recurse -Include *.cpp,*.h,*.hpp | Where-Object { $_.FullName -NotMatch '[\\/](~?\.dev|deps|build|vcpkg_installed)[\\/]' } | ForEach-Object {
    Write-Host "Formatting: $($_.FullName)" -ForegroundColor Cyan
    clang-format -style=file -i $_.FullName
}
