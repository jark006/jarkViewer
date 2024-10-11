@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

set "viewer=%~dp0jarkViewer.exe"
set "formats=apng avif avifs bmp dib exr gif hdr heic heif ico icon jfif jp2 jpe jpeg jpg jxl jxr pbm pfm pgm pic png pnm ppm psd pxm ras sr svg tga tif tiff webp wp2 3fr arw cr2 cr3 crw dng kdc mrw nef orf pef raf raw rw2 sr2 srf x3f"

REM 检查 jarkViewer.exe 是否存在
if not exist "%viewer%" (
    echo jarkViewer.exe 未找到，请确保此脚本与其在同一目录中。
    pause
    exit /b 1
)

for %%f in (%formats%) do (
    reg add "HKCR\.%%f" /ve /d "jarkViewer.%%f" /f
    reg add "HKCR\jarkViewer.%%f" /ve /d "%%f 文件" /f
    reg add "HKCR\jarkViewer.%%f\DefaultIcon" /ve /d "\"%viewer%\",0" /f
    reg add "HKCR\jarkViewer.%%f\shell\open\command" /ve /d "\"%viewer%\" \"%%1\"" /f
)

echo 图片格式已成功关联到 jarkViewer.exe
pause