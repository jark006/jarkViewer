# 🌟 jarkViewer 

![Version](https://img.shields.io/github/v/release/jark006/jarkViewer) ![Stars](https://img.shields.io/github/stars/jark006/jarkViewer) ![License](https://img.shields.io/github/license/jark006/jarkViewer) ![Issues](https://img.shields.io/github/issues/jark006/jarkViewer)  
*一个高效便捷的多格式图片查看工具*

---

![Preview](SocialPreview.png)

## ✨ 操作方式

1. **切换图片**：窗口左右边缘滚轮 / 左键 / 右键 / PgUp / PgDown  
1. **放大缩小**：窗口中间滚轮 / 上下方向键  
1. **旋转图片**：窗口左上角或右上角滚轮 / 单击 / Q 和 E 键  
1. **平移图片**：鼠标拖动 / WASD 键  
1. **查看图像信息**：点击滚轮 / 按 `I` 键  
1. **复制信息**：按空格 / `C` 键  
1. **切换全屏**：双击窗口 / `F11` 键  
1. **快捷退出**：右键单击 / 按 `ESC` 键

---

## ⚙️ 其他功能  

1. ✅ 自动记忆上次窗口位置/尺寸  
2. 📖 支持读取AI生成图像（如 Stable-Diffusion、Flux）的提示词等信息【前提是图片中包含了提示词信息，不是所有的文生图图片都包含提示词信息的】

---

## 📂 支持的图像格式

- **静态图像**：`png avif avifs bmp bpg dib exr gif hdr heic heif ico icon jfif jp2 jpe jpeg jpg jxl jxr pbm pfm pgm pic png pnm ppm psd pxm ras sr svg tga tif tiff webp wp2
` 等  
- **动态图像**：`gif webp png apng jxl`  
- **RAW格式**：`crw pef sr2 cr2 cr3 nef arw 3fr srf orf rw2 dng raf raw kdc x3f mrw` 等  

---

## 🛠️ 编译前的准备

1. 解压 `jarkViewer/lib/lib.7z` 所有 `*.lib` 静态库
2. 解压 `jarkViewer/libexiv2/libexiv2.7z` 所有 `*.lib` 静态库
3. 解压 `jarkViewer/libopencv/libopencv.7z` 所有 `*.lib` 静态库
4. 解压 `jarkViewer/libpng/libpng.7z` 所有 `*.lib` 静态库

或者开启vcpkg支持，然后手动安装第三方库
```sh
vcpkg install giflib:x64-windows-static
vcpkg install x265:x64-windows-static
vcpkg install zlib:x64-windows-static
vcpkg install libyuv:x64-windows-static
vcpkg install exiv2[core,bmff,png,xmp]:x64-windows-static
vcpkg install libavif[core,aom,dav1d]:x64-windows-static
vcpkg install libjxl[core,tools]:x64-windows-static
vcpkg install libheif[core,hevc]:x64-windows-static
vcpkg install libraw[core,dng-lossy,openmp]:x64-windows-static
vcpkg install opencv4[core,ade,contrib,default-features,eigen,ffmpeg,freetype,gdcm,gstreamer,halide,ipp,jasper,jpeg,lapack,nonfree,openexr,opengl,openjpeg,openmp,openvino,ovis,png,python,qt,quirc,sfm,tbb,tiff,vtk,vulkan,webp,world]:x64-windows-static
```

---

## 🔧 DLL 缺失解决方案

请下载并安装 [Microsoft Visual C++ 2015-2022 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)。

---

## 🖱️ 小白用户使用教程  

### 方法一：通过右键菜单使用 jarkViewer 打开图像  

1. **找到目标图像文件**：在文件管理器中定位需要查看的图片。  
2. **右键单击文件**：在弹出的菜单中，选择 **"打开方式"**。  
3. **选择 jarkViewer**：如果列表中没有显示 jarkViewer：  
   - 点击 **"选择其他应用"** 或 **"更多应用"**。  
   - 点击 **"查找其他应用程序"**，并在弹出的窗口中找到 jarkViewer 的下载目录。  
   - 选中 jarkViewer 程序文件（如 `jarkViewer.exe`），点击打开即可，之后你便可以在打开方式的列表中看到`jarkViewer`了。  
4. **设为默认程序**（可选）：勾选 **"始终使用此应用打开该类型文件"**，然后点击 jarkViewer 图标打开文件。  

### 方法二：将 jarkViewer 设置为默认图像浏览器  

1. **打开系统设置**：按下 `Win + I` 打开 Windows 设置。  
2. **进入默认应用设置**：依次点击 **"应用" > "默认应用"**。  
3. **设置图片类型关联**：在 **"按文件类型选择默认应用"** 中，搜索常用的图片格式（如 `.png`、`.jpg`、`.gif`），点击当前默认的应用程序。  
4. **选择 jarkViewer**：如果列表中没有显示 jarkViewer：  
   - 点击 **"选择其他应用"** 或 **"更多应用"**。  
   - 点击 **"查找其他应用程序"**，并在弹出的窗口中找到 jarkViewer 的下载目录。  
   - 选中 jarkViewer 程序文件（如 `jarkViewer.exe`），点击打开即可，之后你便可以在打开方式的列表中看到`jarkViewer`了。  。    
5. 完成后，双击任何关联格式的图片文件，都会自动使用 jarkViewer 打开。

---

## 🖼️ 软件预览

![软件截图](preview.png)

---

## 📜 License

本项目采用 MIT 许可证开放源代码。了解更多内容，请查看 [LICENSE 文件](https://github.com/jark006/jarkViewer/blob/main/LICENSE)。
