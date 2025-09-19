#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cmath>

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

// Structure to hold OCR line data with bounding box
typedef struct {
  string content;
  float x, y, width, height;
  float center_x, center_y;
} OcrLineData;

// Structure to hold OCR word data with bounding box
typedef struct {
  string content;
  float x, y, width, height;
  float center_x, center_y;
} OcrWordData;

// Simple XML escape helper
static string escapeXml(const string &s) {
  string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default: out.push_back(c);
    }
  }
  return out;
}

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

// Function to calculate distance between two lines
double calculateDistance(const OcrLineData& line1, const OcrLineData& line2) {
  double dist = sqrt(pow(line1.center_x - line2.center_x, 2) + pow(line1.center_y - line2.center_y, 2));
#ifdef LOG
  printf("Distance between '%s' (%.1f,%.1f) and '%s' (%.1f,%.1f): %.2f\n", 
         line1.content.substr(0, 20).c_str(), line1.center_x, line1.center_y,
         line2.content.substr(0, 20).c_str(), line2.center_x, line2.center_y, dist);
#endif
  return dist;
}

// Function to group lines by proximity (for speech bubbles)
vector<vector<OcrLineData>> groupLinesByProximity(vector<OcrLineData>& lines, int imageHeight, double maxDistancePercent = 0.1, double maxDistanceAbsoluteMinimum = 100) {
  double maxDistance = max(imageHeight * maxDistancePercent, maxDistanceAbsoluteMinimum);
  vector<vector<OcrLineData>> groups;
  vector<bool> used(lines.size(), false);
  
  #ifdef LOG
  printf("\n=== Grouping %zu lines with maxDistance=%.2f (%.1f%% of image height %d) ===\n", 
         lines.size(), maxDistance, maxDistancePercent * 100, imageHeight);
  #endif
  
  for (size_t i = 0; i < lines.size(); i++) {
    if (used[i]) continue;

    vector<OcrLineData> currentGroup;
    currentGroup.push_back(lines[i]);
    used[i] = true;

    #ifdef LOG
    printf("\nStarting new group %zu with line: '%s' at (%.1f,%.1f)\n",
           groups.size() + 1, lines[i].content.substr(0, 30).c_str(),
           lines[i].center_x, lines[i].center_y);
    #endif

    // Find all lines close to the current group
    bool foundNew = true;
    while (foundNew) {
      foundNew = false;
      for (size_t j = 0; j < lines.size(); j++) {
        if (used[j]) continue;

        // Calculate the vertical span of the current group (distance between highest and lowest line)
        float minY = currentGroup[0].center_y;
        float maxY = currentGroup[0].center_y;
        for (const auto& groupLine : currentGroup) {
          minY = min(minY, groupLine.center_y);
          maxY = max(maxY, groupLine.center_y);
        }
        float verticalSpan = maxY - minY;
        float verticalCompensation = 0.5f * verticalSpan;

        // Check if this line is close to any line in the current group
        for (const auto& groupLine : currentGroup) {
          float dx = lines[j].center_x - groupLine.center_x;
          float dy = lines[j].center_y - groupLine.center_y;
          float dist = sqrt(dx * dx + dy * dy);
          float compensatedMaxDistance = maxDistance;
          // If the candidate is vertically offset, allow more distance
          if (fabs(dy) > 0) {
            compensatedMaxDistance += verticalCompensation;
          }
          #ifdef LOG
          printf("    Checking '%s' vs group line '%s': dist=%.2f, compensatedMax=%.2f (dy=%.2f, verticalComp=%.2f, span=%.2f)\n",
                 lines[j].content.substr(0, 20).c_str(), groupLine.content.substr(0, 20).c_str(),
                 dist, compensatedMaxDistance, dy, verticalCompensation, verticalSpan);
          #endif
          if (dist <= compensatedMaxDistance) {
            #ifdef LOG
            printf("  -> Adding '%s' to group (distance: %.2f <= %.2f)\n",
                   lines[j].content.substr(0, 30).c_str(), dist, compensatedMaxDistance);
            #endif
            currentGroup.push_back(lines[j]);
            used[j] = true;
            foundNew = true;
            break;
          }
        }
        if (foundNew) break;
      }
    }

    #ifdef LOG
    printf("  Group %zu completed with %zu lines\n", groups.size() + 1, currentGroup.size());
    #endif

    // Sort lines in the group by vertical position (top to bottom)
    sort(currentGroup.begin(), currentGroup.end(),
         [](const OcrLineData& a, const OcrLineData& b) {
           return a.center_y < b.center_y;
         });

    groups.push_back(currentGroup);
  }
  
  #ifdef LOG
  printf("\n=== Created %zu groups total ===\n", groups.size());
  #endif
  
  // Sort groups by their topmost line's position (left to right, then top to bottom)
  sort(groups.begin(), groups.end(), 
       [](const vector<OcrLineData>& a, const vector<OcrLineData>& b) {
         if (abs(a[0].center_y - b[0].center_y) < 50) { // Same row
           return a[0].center_x < b[0].center_x; // Left to right
         }
         return a[0].center_y < b[0].center_y; // Top to bottom
       });
  
  return groups;
}

void ocr(Img img, const string &output_file, __int64 pipeline, __int64 opt) {
  HINSTANCE hDLL = LoadLibraryA("oneocr.dll");
  if (hDLL == NULL) {
    std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
    return;
  }
  
  // Store image height for maxDistance calculation
  int imageHeight = img.row;
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
#ifdef LOG
  printf("Running ocr pipeline...\n");
  printf("\t ctx: 0x%llx, pipeline: 0x%llx, opt: 0x%llx, instance: "
         "0x%llx\n",
         ctx, pipeline, opt, instance);
#endif
  __int64 lc;
  res = GetOcrLineCount(instance, &lc);
  assert(res == 0);
  #ifdef LOG
  printf("Recognize %lld lines\n", lc);
  #endif

  // Collect all line data first
  vector<OcrLineData> allLines;
  // For XML export: keep words per line
  vector<vector<OcrWordData>> allWordsPerLine;

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
    
    // Get bounding box for this line
    __int64 bbox_ptr = 0;
    GetOcrLineBoundingBox(line, &bbox_ptr);
    
    OcrLineData lineData;
    lineData.content = string(lcs);
    
    if (bbox_ptr) {
      // Assuming bounding box is stored as [x, y, width, height]
      float* bbox = reinterpret_cast<float*>(bbox_ptr);
      lineData.width = bbox[0];
      lineData.height = bbox[1];
      lineData.x = bbox[2];
      lineData.y = bbox[3];
      lineData.center_x = lineData.x + lineData.width / 2;
      lineData.center_y = lineData.y + lineData.height / 2;
      #ifdef LOG
      printf("Line %lld: Got bounding box from API: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n", 
             lci, lineData.x, lineData.y, lineData.width, lineData.height);
      #endif
    } else {
      // Fallback: use line index as approximate position
      lineData.x = 0;
      lineData.y = (int)lci * 20; // Approximate line height
      lineData.width = 100;
      lineData.height = 20;
      lineData.center_x = 50;
      lineData.center_y = lineData.y + 10;
      #ifdef LOG
      printf("Line %lld: No bounding box available, using fallback: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n", 
             lci, lineData.x, lineData.y, lineData.width, lineData.height);
      #endif
    }
    
    allLines.push_back(lineData);
  // Prepare container for this line's words
  vector<OcrWordData> wordsThisLine;
    
    __int64 lr = 0;
    GetOcrLineWordCount(line, &lr);
    for (__int64 j = 0; j < lr; j++) {
      __int64 v105 = 0;
      __int64 v107 = 0;
      __int64 lpMultiByteStr = 0;
      GetOcrWord(line, j, &v105);
      GetOcrWordContent(v105, &lpMultiByteStr);
      GetOcrWordBoundingBox(v105, &v107);
      if (lpMultiByteStr) {
        char *wcs = reinterpret_cast<char *>(lpMultiByteStr);
        OcrWordData wd;
        wd.content = string(wcs);
        if (v107) {
          float* wb = reinterpret_cast<float*>(v107);
          wd.width = wb[0];
          wd.height = wb[1];
          wd.x = wb[2];
          wd.y = wb[3];
          wd.center_x = wd.x + wd.width / 2;
          wd.center_y = wd.y + wd.height / 2;
        } else {
          wd.x = 0; wd.y = 0; wd.width = 0; wd.height = 0; wd.center_x = 0; wd.center_y = 0;
        }
        wordsThisLine.push_back(wd);
      }
    }
    allWordsPerLine.push_back(wordsThisLine);
  }

  // Log all recognized lines with their bounding boxes before grouping
  #ifdef LOG
  printf("\n=== All recognized text lines with bounding boxes ===\n");
  for (size_t i = 0; i < allLines.size(); i++) {
    printf("Line %zu: '%s'\n", i, allLines[i].content.c_str());
    printf("  Bounding box: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n", 
           allLines[i].x, allLines[i].y, allLines[i].width, allLines[i].height);
    printf("  Center: (%.1f, %.1f)\n", allLines[i].center_x, allLines[i].center_y);
    printf("\n");
  }
  #endif

  // Group lines by proximity
  vector<vector<OcrLineData>> groupedLines = groupLinesByProximity(allLines, imageHeight);
  
  // Write grouped results to output file: lines in a group on one line, groups separated by newlines, all lowercase
  for (size_t groupIdx = 0; groupIdx < groupedLines.size(); groupIdx++) {
    if (groupIdx > 0) {
      out << "\n\n"; // Newline between groups (speech bubbles)
    }
    for (size_t lineIdx = 0; lineIdx < groupedLines[groupIdx].size(); lineIdx++) {
      string lineContent = groupedLines[groupIdx][lineIdx].content;
      // Convert to lowercase
      // transform(lineContent.begin(), lineContent.end(), lineContent.begin(), ::tolower);
      out << lineContent;
      // Add space between lines in a group, except after the last line
      if (lineIdx < groupedLines[groupIdx].size() - 1) {
        out << " ";
      }
    }
  }
  out.close();
  printf("OCR results saved to %s\n", output_file.c_str());

  // Write XML export next to the .txt file
  try {
    string xml_file = filesystem::path(output_file).replace_extension(".xml").string();
    ofstream xout(xml_file);
    if (xout.is_open()) {
      xout << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
      xout << "<ocrExport source=\"" << escapeXml(output_file) << "\">\n";

      // Write lines with bounding boxes and their words
      for (size_t i = 0; i < allLines.size(); i++) {
        const auto &ln = allLines[i];
        xout << "  <line id=\"" << i << "\" x=\"" << ln.x << "\" y=\"" << ln.y << "\" width=\"" << ln.width << "\" height=\"" << ln.height << "\">\n";
        xout << "    <text>" << escapeXml(ln.content) << "</text>\n";
        // words (if available)
        if (i < allWordsPerLine.size()) {
          const auto &words = allWordsPerLine[i];
          for (size_t w = 0; w < words.size(); w++) {
            const auto &wd = words[w];
            xout << "    <word id=\"" << w << "\" x=\"" << wd.x << "\" y=\"" << wd.y << "\" width=\"" << wd.width << "\" height=\"" << wd.height << "\">";
            xout << escapeXml(wd.content) << "</word>\n";
          }
        }
        xout << "  </line>\n";
      }

      xout << "</ocrExport>\n";
      xout.close();
      printf("XML export saved to %s\n", xml_file.c_str());
    } else {
      printf("Failed to open XML output: %s\n", xml_file.c_str());
    }
  } catch (const std::exception &e) {
    printf("Exception while writing XML: %s\n", e.what());
  }
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
