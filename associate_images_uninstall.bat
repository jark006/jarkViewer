@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

fltmc >nul 2>&1
if %errorlevel% neq 0 (
    echo 请以管理员权限运行此脚本。
    pause
    exit /b
)

set "formats=apng avif avifs bmp bpg dib exr gif hdr heic heif ico icon jfif jp2 jpe jpeg jpg jxl jxr pbm pfm pgm pic png pnm ppm psd pxm ras sr svg tga tif tiff webp wp2 3fr arw cr2 cr3 crw dng kdc mrw nef orf pef raf raw rw2 sr2 srf x3f"

for %%f in (%formats%) do (
    reg delete "HKCR\jarkViewer.%%f" /f
    reg delete "HKCR\.%%f" /ve /f
)

echo 图片格式已成功解除与 jarkViewer 的关联
pause