// ============================================================================
//  print_designer.h — full vector print-design editor (release 1.4.0, §3/§4)
//  WYSIWYG canvas, draggable/resizable/rotatable items, 20 built-in templates,
//  per-section design binding, .aztpl export/import, undo/redo, appointment
//  counters. File-backed data model so the EXE stays a single static binary.
// ============================================================================
#pragma once
#include <string>
#include <vector>

// --------------------------------------------------------------- item types --
enum PrintItemType {
    PIT_LABEL = 0,     // free text / static label
    PIT_FIELD,         // data-bound text (field name in `field`)
    PIT_HLINE,         // horizontal line
    PIT_VLINE,         // vertical line
    PIT_RECT,          // rectangle box
    PIT_FRAME,         // full-page border (special hit-test, §3.10)
    PIT_IMAGE,         // custom image (path or embedded base64)
    PIT_LOGO,          // clinic logo (image)
    PIT_QR,            // QR / barcode (encodes the receipt number)
    PIT_PHOTO,         // patient personal photo placeholder
    PIT_APPTNO         // appointment-number counter (§3.13)
};

// Each PrintItem lives in millimetre space (paper coordinates).
struct PrintItem {
    int          id;
    int          type;          // PrintItemType
    double       x, y, w, h;    // mm
    double       rot;           // degrees
    bool         locked;
    bool         is_frame;      // true for PIT_FRAME (hit-test special)
    int          z;             // z-index (paint order)

    // text
    std::wstring text;          // label content / prefix display
    std::wstring field;         // data binding key (nationalCode, firstName, …)
    std::wstring prefix, suffix;
    std::wstring fmt;           // number/date format hint
    std::wstring fontName;
    double       fontPt;
    bool         bold, italic;
    int          align;         // 0=right 1=center 2=left 3=justify (RTL)
    double       lineSpacing;

    // appearance
    unsigned int textColor;     // 0x00RRGGBB
    unsigned int fillColor;
    bool         fillTransparent;
    unsigned int borderColor;
    double       borderWidth;   // px
    double       corner;        // mm
    double       padding;       // mm
    double       opacity;       // 0..1
    int          visibility;    // 0=always 1=when_field_not_empty

    // image
    std::wstring imgPath;       // for PIT_IMAGE / PIT_LOGO

    // appointment counter (§3.13)
    int          startValue;
    int          step;

    PrintItem();
};

struct PrintDesign {
    int          id;            // 0 = unsaved
    std::wstring name;
    std::wstring kind;          // "builtin" | "user"
    std::wstring paper;         // "A4","A5","A6","B5","Letter","R80","R58","A3","custom"
    double       paperW, paperH;// mm (for custom / resolved)
    int          orientation;   // 0=portrait 1=landscape
    std::vector<PrintItem> items;
    PrintDesign();
};

// Resolve a paper preset name to mm dimensions (portrait). Returns false if
// the name is "custom" (caller keeps paperW/paperH).
bool Paper_Dims(const std::wstring& name, double& wmm, double& hmm);

// ---------------------------------------------------------- JSON (in-house) --
//  Minimal, self-contained serializer/parser for PrintDesign. Not a general
//  JSON library — only what the designer needs. Magic header "AZTEMPLATE/1".
std::string  Design_ToJson(const PrintDesign& d);
bool         Design_FromJson(const std::string& json, PrintDesign& out,
                             std::wstring& err);

// ----------------------------------------------------------- design store ----
void Designs_Init();                       // seed 20 built-ins on first run
int  Designs_All(std::vector<PrintDesign>& out);
int  Designs_Builtins(std::vector<PrintDesign>& out);
int  Designs_User(std::vector<PrintDesign>& out);
int  Designs_Insert(const PrintDesign& d); // returns new id
bool Designs_Update(const PrintDesign& d);
bool Designs_Delete(int id);
bool Designs_Get(int id, PrintDesign& out);

// section <-> design binding
bool SectionDesign_Set(int sectionId, int designId);
int  SectionDesign_Get(int sectionId);     // designId or 0
// resolve the active design payload for a section (falls back to T01).
bool SectionDesign_Resolve(int sectionId, PrintDesign& out);

// ---------------------------------------------------- appointment counters ---
//  Atomically advance the counter for (section, today-jalali) and return the
//  new value. step/start come from the design's APPTNO item (defaults 1/1).
int  ApptCounter_Next(int sectionId, int startValue, int step);
//  Peek the current counter value without advancing (for preview).
int  ApptCounter_Peek(int sectionId);

// -------------------------------------------------------------- UI entries ---
//  Management → "دیزاین چاپگر": opens the section picker then the editor.
void PrintDesigner_Open(HWND hMain);
//  Management → "بازگردانی دیزاین چاپ": import an .aztpl and apply to sections.
void RestoreDesign_Open(HWND hMain);
