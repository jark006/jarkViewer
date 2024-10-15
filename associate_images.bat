@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

fltmc >nul 2>&1
if %errorlevel% neq 0 (
    echo 请以管理员权限运行此脚本。
    pause
    exit /b
)

set "currentDir=%~dp0"
set "viewer="
set "matchCount=0"
for %%f in ("%currentDir%jarkViewer*.exe") do (
    set /a matchCount+=1
    if !matchCount! equ 1 (
        set "viewer=%%~f"
    ) else (
        set "otherMatches=!otherMatches! %%f"
    )
)

if %matchCount% equ 0 (
    echo 脚本所在文件夹未找到 jarkViewer
    pause
    exit /b
) else if %matchCount% equ 1 (
    echo 当前jarkViewer路径为 %viewer%
) else (
    echo 找到多个匹配的 jarkViewer:
    echo %viewer%
	for %%m in (%otherMatches%) do echo %%m
    echo 自动选择第一个匹配项 %viewer%
)

set "formats=apng avif avifs bmp dib exr gif hdr heic heif ico icon jfif jp2 jpe jpeg jpg jxl jxr pbm pfm pgm pic png pnm ppm psd pxm ras sr svg tga tif tiff webp wp2 3fr arw cr2 cr3 crw dng kdc mrw nef orf pef raf raw rw2 sr2 srf x3f"

for %%f in (%formats%) do (
    reg add "HKCR\.%%f" /ve /d "jarkViewer.%%f" /f
    reg add "HKCR\jarkViewer.%%f" /ve /d "%%f 文件" /f
    reg add "HKCR\jarkViewer.%%f\DefaultIcon" /ve /d "\"%viewer%\",0" /f
    reg add "HKCR\jarkViewer.%%f\shell\open\command" /ve /d "\"%viewer%\" \"%%1\"" /f
)

echo 图片格式已成功关联到 %viewer%
pause