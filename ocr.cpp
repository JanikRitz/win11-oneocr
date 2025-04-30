#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

#include <Windows.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
using namespace cv;
using namespace std;

typedef struct {
  __int32 t;
  __int32 col;
  __int32 row;
  __int32 _unk;
  __int64 step;
  __int64 data_ptr;
} Img;

typedef __int64(__cdecl *CreateOcrInitOptions_t)(__int64 *);
typedef __int64(__cdecl *GetOcrLineCount_t)(__int64, __int64 *);
typedef __int64(__cdecl *GetOcrLine_t)(__int64, __int64, __int64 *);
typedef __int64(__cdecl *GetOcrLineContent_t)(__int64, __int64 *);
typedef __int64(__cdecl *GetOcrLineBoundingBox_t)(__int64, __int64 *);
typedef __int64(__cdecl *GetOcrLineWordCount_t)(__int64, __int64 *);
typedef __int64(__cdecl *GetOcrWord_t)(__int64, __int64, __int64 *);
typedef __int64(__cdecl *GetOcrWordContent_t)(__int64, __int64 *);
typedef __int64(__cdecl *GetOcrWordBoundingBox_t)(__int64, __int64 *);
typedef __int64(__cdecl *OcrProcessOptionsSetMaxRecognitionLineCount_t)(
    __int64, __int64);
typedef __int64(__cdecl *RunOcrPipeline_t)(__int64, Img *, __int64, __int64 *);
typedef __int64(__cdecl *CreateOcrProcessOptions_t)(__int64 *);
typedef __int64(__cdecl *OcrInitOptionsSetUseModelDelayLoad_t)(__int64, char);
typedef __int64(__cdecl *CreateOcrPipeline_t)(__int64, __int64, __int64,
                                              __int64 *);

void ocr(Img img, const string &output_file, __int64 pipeline, __int64 opt) {
  HINSTANCE hDLL = LoadLibraryA("oneocr.dll");
  if (hDLL == NULL) {
    std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
    return;
  }
  // Get function pointers
  CreateOcrInitOptions_t CreateOcrInitOptions =
      (CreateOcrInitOptions_t)GetProcAddress(hDLL, "CreateOcrInitOptions");
  GetOcrLineCount_t GetOcrLineCount =
      (GetOcrLineCount_t)GetProcAddress(hDLL, "GetOcrLineCount");
  CreateOcrProcessOptions_t CreateOcrProcessOptions =
      (CreateOcrProcessOptions_t)GetProcAddress(hDLL,
                                                "CreateOcrProcessOptions");
  CreateOcrPipeline_t CreateOcrPipeline =
      (CreateOcrPipeline_t)GetProcAddress(hDLL, "CreateOcrPipeline");
  OcrInitOptionsSetUseModelDelayLoad_t OcrInitOptionsSetUseModelDelayLoad =
      (OcrInitOptionsSetUseModelDelayLoad_t)GetProcAddress(
          hDLL, "OcrInitOptionsSetUseModelDelayLoad");
  OcrProcessOptionsSetMaxRecognitionLineCount_t
      OcrProcessOptionsSetMaxRecognitionLineCount =
          (OcrProcessOptionsSetMaxRecognitionLineCount_t)GetProcAddress(
              hDLL, "OcrProcessOptionsSetMaxRecognitionLineCount");
  RunOcrPipeline_t RunOcrPipeline =
      (RunOcrPipeline_t)GetProcAddress(hDLL, "RunOcrPipeline");
  GetOcrLine_t GetOcrLine = (GetOcrLine_t)GetProcAddress(hDLL, "GetOcrLine");
  GetOcrLineContent_t GetOcrLineContent =
      (GetOcrLineContent_t)GetProcAddress(hDLL, "GetOcrLineContent");
  GetOcrLineBoundingBox_t GetOcrLineBoundingBox =
      (GetOcrLineBoundingBox_t)GetProcAddress(hDLL, "GetOcrLineBoundingBox");
  GetOcrLineWordCount_t GetOcrLineWordCount =
      (GetOcrLineWordCount_t)GetProcAddress(hDLL, "GetOcrLineWordCount");
  GetOcrWord_t GetOcrWord = (GetOcrWord_t)GetProcAddress(hDLL, "GetOcrWord");
  GetOcrWordContent_t GetOcrWordContent =
      (GetOcrWordContent_t)GetProcAddress(hDLL, "GetOcrWordContent");
  GetOcrWordBoundingBox_t GetOcrWordBoundingBox =
      (GetOcrWordBoundingBox_t)GetProcAddress(hDLL, "GetOcrWordBoundingBox");
  __int64 ctx = 0;
  __int64 instance = 0;
  __int64 res = 0;
#ifdef DEBUG
  __int16 *ibs = reinterpret_cast<__int16 *>(&img);
  for (int i = 0; i < 8; i++) {
    printf("%02x ", ibs[i]);
  }
  printf("\n");
#endif
  assert(sizeof(img) == 0x20);
  res = RunOcrPipeline(pipeline, &img, opt, &instance);
  assert(res == 0);
  printf("Running ocr pipeline...\n");
#ifdef DEBUG  
  printf("\t ctx: 0x%llx, pipeline: 0x%llx, opt: 0x%llx, instance: "
         "0x%llx\n",
         ctx, pipeline, opt, instance);
#endif
  __int64 lc;
  res = GetOcrLineCount(instance, &lc);
  assert(res == 0);
  printf("Recognize %lld lines\n", lc);

  ofstream out(output_file);
  if (!out.is_open()) {
    cerr << "Failed to open output file: " << output_file << endl;
    return;
  }

  for (__int64 lci = 0; lci < lc; lci++) {
    __int64 line = 0;
    __int64 v106 = 0;
    GetOcrLine(instance, lci, &line);
    if (!line) {
      continue;
    }
    __int64 line_content = 0;
    GetOcrLineContent(line, &line_content);
    char *lcs = reinterpret_cast<char *>(line_content);
    out << lcs << endl; // Write the recognized line to the output file
    // GetOcrLineBoundingBox(line, &v106);
    __int64 lr = 0;
    GetOcrLineWordCount(line, &lr);
    for (__int64 j = 0; j < lr; j++) {
      __int64 v105 = 0;
      __int64 v107 = 0;
      __int64 lpMultiByteStr = 0;
      GetOcrWord(line, j, &v105);
      GetOcrWordContent(v105, &lpMultiByteStr);
      GetOcrWordBoundingBox(v105, &v107);
    }
  }

  out.close();
  printf("OCR results saved to %s\n", output_file.c_str());
}

void process_image(const string &file_name, __int64 pipeline, __int64 opt) {
  Mat img = imread(file_name, IMREAD_UNCHANGED);
  if (img.empty()) {
    cout << "Can't read image: " << file_name << endl;
    return;
  }

  Mat img_rgba;
  if (img.channels() == 3) {
    cvtColor(img, img_rgba, COLOR_BGR2BGRA);
  } else if (img.channels() == 4) {
    img_rgba = img;
  } else {
    cout << "Image type not supported: " << file_name << endl;
    return;
  }

  int rows = img_rgba.rows;
  int cols = img_rgba.cols;
  size_t step = img_rgba.step;

  Img ig = {.t = 3,
            .col = cols,
            .row = rows,
            ._unk = 0,
            .step = (__int64)step,
            .data_ptr = (__int64)reinterpret_cast<char *>(img_rgba.data)};

  string output_file = filesystem::path(file_name).replace_extension(".txt").string();
  ocr(ig, output_file, pipeline, opt);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: ocr.exe <image_path_or_folder>\n");
    return 0;
  }

  string input_path = argv[1];
  vector<string> image_files;

  if (filesystem::is_directory(input_path)) {
    for (const auto &entry : filesystem::directory_iterator(input_path)) {
      if (entry.is_regular_file() && 
          (entry.path().extension() == ".png" || entry.path().extension() == ".jpg")) {
        image_files.push_back(entry.path().string());
      }
    }
  } else if (filesystem::is_regular_file(input_path)) {
    image_files.push_back(input_path);
  } else {
    cout << "Invalid path: " << input_path << endl;
    return -1;
  }

  HINSTANCE hDLL = LoadLibraryA("oneocr.dll");
  if (hDLL == NULL) {
    cerr << "Failed to load DLL: " << GetLastError() << endl;
    return -1;
  }

  CreateOcrInitOptions_t CreateOcrInitOptions =
      (CreateOcrInitOptions_t)GetProcAddress(hDLL, "CreateOcrInitOptions");
  CreateOcrPipeline_t CreateOcrPipeline =
      (CreateOcrPipeline_t)GetProcAddress(hDLL, "CreateOcrPipeline");
  CreateOcrProcessOptions_t CreateOcrProcessOptions =
      (CreateOcrProcessOptions_t)GetProcAddress(hDLL, "CreateOcrProcessOptions");
  OcrInitOptionsSetUseModelDelayLoad_t OcrInitOptionsSetUseModelDelayLoad =
      (OcrInitOptionsSetUseModelDelayLoad_t)GetProcAddress(hDLL, "OcrInitOptionsSetUseModelDelayLoad");
  OcrProcessOptionsSetMaxRecognitionLineCount_t OcrProcessOptionsSetMaxRecognitionLineCount =
      (OcrProcessOptionsSetMaxRecognitionLineCount_t)GetProcAddress(hDLL, "OcrProcessOptionsSetMaxRecognitionLineCount");

  __int64 ctx = 0, pipeline = 0, opt = 0;
  __int64 res = CreateOcrInitOptions(&ctx);
  assert(res == 0);
  res = OcrInitOptionsSetUseModelDelayLoad(ctx, 0);
  assert(res == 0);

  const char *key = {"kj)TGtrK>f]b[Piow.gU+nC@s\"\"\"\"\"\"4"};
  res = CreateOcrPipeline((__int64)"oneocr.onemodel", (__int64)key, ctx, &pipeline);
  assert(res == 0);
  printf("OCR model loaded...\n");

  res = CreateOcrProcessOptions(&opt);
  assert(res == 0);
  res = OcrProcessOptionsSetMaxRecognitionLineCount(opt, 1000);
  assert(res == 0);

  for (const auto &file_name : image_files) {
    process_image(file_name, pipeline, opt);
  }

  return 0;
}
