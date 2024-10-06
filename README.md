# jarkViewer

![](SocialPreview.png)

### 操作简单：

鼠标在窗口两侧边缘滚轮切换图片，在窗口中间滚轮放大缩小。

双击切换全屏，鼠标中键显示图像EXIF等详细信息，右键退出。

### 静态图像支持：
jpg jp2 jpe jpeg ico dib bmp exr png apng pbm pgm ppm pxm pnm tif tiff ras hdr pic icon gif jxl psd tga svg webp sr avifs heic heif avif jfif jxr

### 动态图像支持：
gif webp png apng jxl

### RAW格式支持：
crw pef sr2 cr2 cr3 nef arw 3fr srf orf rw2 dng raf raw kdc x3f mrw

---

## 编译前操作

1. 解压 `jarkViewer/lib/lib.7z` 所有 `*.lib` 静态库
2. 解压 `jarkViewer/libexiv2/libexiv2.7z` 所有 `*.lib` 静态库
3. 解压 `jarkViewer/libopencv/libopencv.7z` 所有 `*.lib` 静态库
4. 解压 `jarkViewer/libpng/libpng.7z` 所有 `*.lib` 静态库

或者手动安装第三方库
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
vcpkg install opencv4[core,ade,contrib,default-features,dnn,eigen,ffmpeg,freetype,gdcm,gstreamer,halide,ipp,jasper,jpeg,lapack,nonfree,openexr,opengl,openjpeg,openmp,openvino,ovis,png,python,qt,quirc,sfm,tbb,tiff,vtk,vulkan,webp,world]:x64-windows-static
```

---

# 若运行软件提示缺少 ``*.dll``

请下载安装 `VC++ 2015-2022` 运行库：

[Microsoft Visual C++ 2015-2022 Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe) 

---

# 预览

![](preview.jpg)
