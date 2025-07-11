
REM C:\Tools\OpenCV_4_6\build

cl /std:c++20 /DLOG /I"C:\Tools\OpenCV_4_6\build\include" ocr.cpp ^
   /link /LIBPATH:"C:\Tools\OpenCV_4_6\build\x64\vc15\lib" opencv_world460.lib ^
   /machine:x64

