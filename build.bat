
REM Q:\dev-project\libs\opencv\build


cl /std:c++20 /I"Q:\dev-project\libs\opencv\build\include" ocr.cpp /link /LIBPATH:"Q:\dev-project\libs\opencv\build\x64\vc15\lib" opencv_world460.lib

