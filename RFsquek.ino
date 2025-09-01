//made by Roman Kalyna 
//ask me anything on Git
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <RadioLib.h>
#include <math.h>

// ===== Product name =====
static const char* PRODUCT_NAME = "RFSqueak-MK1";

// ===== Display pins (Adafruit ST7789) =====
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  17
#define TFT_BLK -1   // -1 if hard-wired ON

// ===== Buttons =====
#define BTN_SELECT 32
#define BTN_BACK   33
#define BUTTON_ACTIVE_LOW 1   // 1 if button to GND with INPUT_PULLUP; set 0 if opposite

// ===== Potentiometer (ADC1) =====
#define POT_PIN 34

// ===== CC1101 pins =====
#define RF_CS    27   // CSn
#define RF_GDO0  26   // GDO0 (IRQ)
#define RF_GDO2  25   // GDO2 (optional)

// ===== UI behavior =====
#define MENU_ROW_H        22
#define MENU_VISIBLE_ROWS 5
#define MENU_HYSTERESIS   0.15f
#define MENU_COMMIT_MS    180
#define TFT_SPI_HZ        16000000UL

// ===== Globals =====
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// Color aliases
#define BLACK    ST77XX_BLACK
#define WHITE    ST77XX_WHITE
#define GREY     0x7BEF
#define GREEN    ST77XX_GREEN
#define BLUE     ST77XX_BLUE
#define YELLOW   0xFFE0
#define TOPBAR   0x18E3
#define DIVCOL   0x5AEB
#define RED      ST77XX_RED
#define CYAN     ST77XX_CYAN
#define ORANGE   0xFD20
#define DARKCYAN 0x03EF
#define PINK     0xF99F
#define BROWN    0xA145
#define BEIGE    0xE71C

// ----- Backlight helpers -----
#if (TFT_BLK >= 0)
void backlightBegin() { pinMode(TFT_BLK, OUTPUT); analogWrite(TFT_BLK, 255); }
void backlightWrite(uint8_t duty) { analogWrite(TFT_BLK, duty); }
#else
void backlightBegin() {}
void backlightWrite(uint8_t) {}
#endif

// ----- Button & Pot -----
struct Button {
  int pin;
  bool activeLow{true};
  bool stable{true};
  bool debounced{true};
  uint32_t lastChange{0};
  const uint16_t debounceMs{25};

  void begin() {
    if (activeLow) pinMode(pin, INPUT_PULLUP);
    else           pinMode(pin, INPUT_PULLDOWN);
    bool lvl = digitalRead(pin);
    debounced = stable = lvl;
    lastChange = millis();
  }
  // one-shot on press edge (debounced)
  bool pressed() {
    bool lvl = digitalRead(pin);
    uint32_t now = millis();
    if (lvl != debounced) { debounced = lvl; lastChange = now; }
    if ((now - lastChange) > debounceMs && stable != debounced) {
      stable = debounced;
      bool isActive = activeLow ? (stable == LOW) : (stable == HIGH);
      if (isActive) return true;
    }
    return false;
  }
  // LIVE level read (fix: don't depend on pressed() to update state)
  bool held() const {
    int lvl = digitalRead(pin);
    return activeLow ? (lvl == LOW) : (lvl == HIGH);
  }
};
Button btnSelect{BTN_SELECT, BUTTON_ACTIVE_LOW};
Button btnBack{BTN_BACK, BUTTON_ACTIVE_LOW};

// Smoothed pot
struct Pot {
  int pin; float filt{0}; bool initialized{false};
  void begin() {
    analogSetPinAttenuation(pin, ADC_11db);
    initialized = false;
  }
  float read01(float alpha=0.10f) {
    const int N = 8;
    uint32_t sum = 0;
    for (int i = 0; i < N; i++) sum += analogRead(pin);
    float v = (sum / float(N)) / 4095.0f;
    v = constrain(v, 0.0f, 1.0f);
    if (!initialized) { filt = v; initialized = true; }
    filt = (1.0f - alpha) * filt + alpha * v;
    return filt;
  }
};
Pot pot{POT_PIN};

// ----- UI Framework -----
class Page {
public:
  virtual ~Page() {}
  virtual const char* title() const = 0;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void update() {}
  virtual void draw() = 0;
};

class PageManager {
public:
  void begin(Adafruit_GFX *g) { gfx = g; }
  void push(Page *p) {
    if (current) current->onExit();
    current = p;
    if (current) current->onEnter();
    invalidate = true;
    mustDraw = true;
  }
  void tick() {
    if (!current) return;
    current->update();
    if (invalidate) {
      gfx->fillScreen(BLACK);
      invalidate = false;
      mustDraw = true;
    }
    current->draw();
  }
  void requestRedraw() { mustDraw = true; }
  Adafruit_GFX *gfx{nullptr};
  bool mustDraw{true};
private:
  Page *current{nullptr};
  bool invalidate{true};
};
PageManager UI;

void drawButtonLEDs() {
  int16_t w = UI.gfx->width();
  int y = 4; int s = 10;
  UI.gfx->fillRoundRect(w - 2*s - 8, y, s, s, 2, btnSelect.held() ? GREEN : GREY);
  UI.gfx->fillRoundRect(w - s - 4,    y, s, s, 2, btnBack.held()   ? BLUE  : GREY);
}
void drawTopBar(const char *title) {
  int16_t w = UI.gfx->width();
  UI.gfx->fillRect(0, 0, w, 22, TOPBAR);
  UI.gfx->setTextColor(WHITE, TOPBAR);
  UI.gfx->setCursor(8, 6);
  UI.gfx->setTextSize(1);
  UI.gfx->setTextWrap(false);
  UI.gfx->print(title);
  drawButtonLEDs();
}
void drawDivider(int y) { UI.gfx->drawLine(0, y, UI.gfx->width(), y, DIVCOL); }

// ----- Cross-page forward declares -----
class MenuPage;
class RadioPage;
class WaterfallPage;
class OccupancyPage;
class SettingsPage;
class HamsterPage;
class MorseRXPage;
class MorseTXPage;
extern MenuPage menuPage;
extern RadioPage radioPage;
extern WaterfallPage waterfallPage;
extern OccupancyPage occupancyPage;
extern SettingsPage settingsPage;
extern HamsterPage hamsterPage;
extern MorseRXPage morseRXPage;
extern MorseTXPage morseTXPage;

// ----- Global Settings shared by pages -----
struct Settings {
  float bandMinMHz = 433.05f;
  float bandMaxMHz = 434.79f;

  int   wfBins     = 160;
  float rssiMinDbm = -115.0f;
  float rssiMaxDbm = -45.0f;

  int dwellMinMs   = 2;
  int dwellMaxMs   = 10;

  float occStepKHz = 50.0f;
  float occThrDbmDefault = -90.0f;
};
Settings S;

// ----- CC1101 (RadioLib) -----
static const int PIN_SCK  = 18;
static const int PIN_MISO = 19;
static const int PIN_MOSI = 23;

CC1101 radio = new Module(RF_CS, RF_GDO0, RADIOLIB_NC, RF_GDO2);
bool RADIO_READY = false;
int  RADIO_INIT_CODE = 0;

// Helpers
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
uint16_t heatMapColor(float norm01) {
  norm01 = constrain(norm01, 0.0f, 1.0f);
  float x = norm01;
  uint8_t r=0, g=0, b=0;
  if (x < 0.25f) { float t = x / 0.25f; r=0; g=(uint8_t)(255*t); b=255; }
  else if (x < 0.5f) { float t = (x - 0.25f) / 0.25f; r=0; g=255; b=(uint8_t)(255*(1-t)); }
  else if (x < 0.75f) { float t = (x - 0.5f) / 0.25f; r=(uint8_t)(255*t); g=255; b=0; }
  else { float t = (x - 0.75f) / 0.25f; r=255; g=(uint8_t)(255*(1-t)); b=0; }
  return rgb565(r, g, b);
}
static inline bool responsiveWaitMs(int ms) {
  uint32_t start = millis();
  while ((int)(millis() - start) < ms) {
    if (btnBack.pressed() || btnBack.held() || btnSelect.pressed()) return false;
    delay(1);
    yield();
  }
  return true;
}

// ===== Pages =====

// Home
class HomePage : public Page {
public:
  const char* title() const override { return "Home"; }
  void update() override;  // defined after MenuPage
  void draw() override {
    if (!UI.mustDraw) return;
    UI.mustDraw = false;
    drawTopBar(title());
    drawDivider(22);
    UI.gfx->setTextColor(WHITE, BLACK);
    UI.gfx->setTextSize(2);
    UI.gfx->setCursor(10, 40); UI.gfx->print(PRODUCT_NAME);
    UI.gfx->setTextSize(1);
    UI.gfx->setCursor(10, 66); UI.gfx->print("Waterfall, Occupancy, Radio, Hamster, Morse RX/TX");
    UI.gfx->setTextColor(0x07FF);
    UI.gfx->setCursor(10, UI.gfx->height() - 14); UI.gfx->print("Press SELECT for Menu");
  }
} homePage;

// Radio
class RadioPage : public Page {
public:
  const char* title() const override { return "Radio"; }
  void onEnter() override { UI.mustDraw = true; if (RADIO_READY) radio.startReceive(); }
  void update() override;  // defined after MenuPage
  void draw() override {
    if (!UI.mustDraw) return;
    UI.mustDraw = false;
    drawTopBar(title()); drawDivider(22);
    int16_t w = UI.gfx->width(), h = UI.gfx->height();
    const int footerH = 26, contentTop = 26, contentBottom = h - footerH - 2;
    UI.gfx->fillRect(0, contentTop, w, contentBottom - contentTop, BLACK);
    UI.gfx->fillRect(0, h - footerH, w, footerH, BLACK);
    UI.gfx->setTextWrap(false); UI.gfx->setTextSize(1); UI.gfx->setTextColor(WHITE, BLACK);
    int x = 8, y = contentTop, dy = 13;
    UI.gfx->setCursor(x, y); UI.gfx->print("CC1101: "); UI.gfx->print(RADIO_READY ? "READY" : "FAILED"); y += dy;
    UI.gfx->setCursor(x, y); UI.gfx->print("Init: "); UI.gfx->print(RADIO_INIT_CODE); y += dy;
    UI.gfx->setCursor(x, y); UI.gfx->print("Freq: 433.92 MHz"); y += dy;
    UI.gfx->setCursor(x, y); UI.gfx->print("TX: "); UI.gfx->print(txCount); UI.gfx->print("  RX: "); UI.gfx->print(rxCount); y += dy;
    UI.gfx->setCursor(x, y); UI.gfx->print("RSSI: "); UI.gfx->print(lastRssi); UI.gfx->print(" dBm"); y += dy;
    UI.gfx->setCursor(x, y);
    String shown = lastRx.length() ? lastRx : String("—");
    if (shown.length() > 22) shown = shown.substring(0, 22) + "...";
    UI.gfx->print("Last: "); UI.gfx->print(shown.c_str());
    UI.gfx->setTextColor(0xC618, BLACK);
    UI.gfx->setCursor(6, h - footerH + 2); UI.gfx->print("SELECT=Send Ping    BACK=Menu");
    UI.gfx->setCursor(6, h - 12);          UI.gfx->print("Use 2nd node to see RX");
  }
private:
  int txCount{0}, rxCount{0}; float lastRssi{0}; String lastRx{""};
} ;

// Waterfall
class WaterfallPage : public Page {
public:
  const char* title() const override { return "Waterfall"; }
  void onEnter() override {
    UI.mustDraw = true;
    w = UI.gfx->width(); h = UI.gfx->height(); yTop = 24; yBottom = h - 24;
    wfH = yBottom - yTop - 16; if (wfH < 30) wfH = 30;
    fMin = S.bandMinMHz; fMax = S.bandMaxMHz;
    bins = constrain(S.wfBins, 40, MAX_BINS); if (bins > w - 20) bins = w - 20;
    binStepMHz = (fMax - fMin) / (bins - 1); minDbm = S.rssiMinDbm; maxDbm = S.rssiMaxDbm;
    writeRow = 0; curBin = 0; memset(wfBuf, 0, sizeof(wfBuf)); memset(peakHold, 0, sizeof(peakHold));
    running = true; lastStripDraw = 0;
    if (RADIO_READY) { radio.setFrequency(fMin); radio.startReceive(); }
  }
  void update() override;  // defined after MenuPage
  void draw() override {
    if (!UI.mustDraw) return; UI.mustDraw = false;
    drawTopBar(title()); drawDivider(22);
    UI.gfx->setTextSize(1); UI.gfx->setTextColor(WHITE, BLACK);
    UI.gfx->setCursor(6, yTop - 10); UI.gfx->print(fMin, 2); UI.gfx->print(" MHz");
    UI.gfx->setCursor(w - 62, yTop - 10); UI.gfx->print(fMax, 2); UI.gfx->print(" MHz");
    for (int r = 0; r < wfH; r++) drawRow(r);
    drawStrip();
    UI.gfx->setTextColor(0xC618, BLACK);
    UI.gfx->setCursor(6, h - 24); UI.gfx->print(running ? "Running" : "Paused ");
    UI.gfx->print("  SELECT=Start/Stop   BACK=Menu");
    UI.gfx->setCursor(6, h - 12); UI.gfx->print("Pot=Dwell "); UI.gfx->print(dwellMs); UI.gfx->print("ms");
  }
private:
  int16_t w{240}, h{135}; int yTop{24}, yBottom{111}, wfH{80};
  float fMin{433.05f}, fMax{434.79f}, binStepMHz{0.01f}; int bins{120}, dwellMs{4}, curBin{0};
  float minDbm{-120.0f}, maxDbm{-40.0f};
  static const int MAX_BINS = 220, MAX_H = 100;
  uint8_t wfBuf[MAX_H][MAX_BINS]; uint8_t peakHold[MAX_BINS];
  int writeRow{0}; bool running{true}; uint32_t lastStripDraw{0};
  void drawRow(int bufRow) {
    int linesFromBottom = (writeRow - 1 - bufRow); while (linesFromBottom < 0) linesFromBottom += wfH;
    int y = yBottom - 16 - linesFromBottom; if (y < yTop || y >= yBottom - 16) return;
    for (int i = 0; i < bins; i++) { uint8_t v = wfBuf[bufRow][i]; uint16_t c = heatMapColor(v / 255.0f); UI.gfx->drawPixel(10 + i, y, c); }
  }
  void drawStrip() {
    int stripY = yBottom - 14; UI.gfx->fillRect(0, stripY, w, 14, BLACK); if (bins <= 0) return;
    int curRow = (writeRow - 1 + wfH) % wfH;
    for (int i = 0; i < bins; i++) {
      uint8_t v = wfBuf[curRow][i]; uint16_t c = heatMapColor(v / 255.0f); int x = 10 + i;
      UI.gfx->drawPixel(x, stripY + 6, c);
      uint8_t p = peakHold[i]; uint16_t pc = (p > 200) ? RED : ((p > 120) ? ORANGE : YELLOW);
      UI.gfx->drawPixel(x, stripY + 2, pc);
    }
  }
};

// Occupancy
class OccupancyPage : public Page {
public:
  const char* title() const override { return "Occupancy"; }
  void onEnter() override {
    UI.mustDraw = true;
    w = UI.gfx->width(); h = UI.gfx->height();
    statusTop = 24; statusH = 12; chartTop = statusTop + statusH + 2; chartLeft = 6;
    chartW = w - 12; chartH = h - chartTop - 26;
    fMin = S.bandMinMHz; fMax = S.bandMaxMHz; chStepKHz = S.occStepKHz;
    channels = (int)round((fMax - fMin) * 1000.0f / chStepKHz) + 1; channels = constrain(channels, 8, MAX_CH);
    memset(highCounts, 0, sizeof(highCounts)); memset(totalCounts, 0, sizeof(totalCounts));
    scanIdx = 0; running = true; thrDbm = S.occThrDbmDefault; lastDraw = 0;
    if (RADIO_READY) { radio.setFrequency(fMin); radio.startReceive(); }
  }
  void update() override;  // defined after MenuPage
  void draw() override {
    if (!UI.mustDraw) return; UI.mustDraw = false;
    drawTopBar(title()); drawDivider(22);
    UI.gfx->fillRect(0, statusTop, UI.gfx->width(), statusH, BLACK);
    UI.gfx->setTextSize(1); UI.gfx->setTextColor(WHITE, BLACK);
    UI.gfx->setCursor(6, statusTop + 2);
    UI.gfx->print("Thr "); UI.gfx->print(thrDbm, 0); UI.gfx->print(" dBm   ");
    UI.gfx->print(running ? "Running" : "Paused");
    int maxIdx = -1; float maxOcc = -1.0f;
    for (int i = 0; i < channels; i++) { float o = occupancy(i); if (o > maxOcc) { maxOcc = o; maxIdx = i; } }
    char rightMsg[24] = {0};
    if (maxIdx >= 0 && maxOcc > 0.0f) { float fMHz = chFreq(maxIdx); int pct = (int)round(maxOcc * 100.0f); snprintf(rightMsg, sizeof(rightMsg), "Max %5.2f %3d%%", fMHz, pct); }
    else { snprintf(rightMsg, sizeof(rightMsg), "Max --.--   0%%"); }
    int msgW = 6 * (int)strlen(rightMsg); int xRight = UI.gfx->width() - msgW - 4; if (xRight < 120) xRight = 120;
    UI.gfx->setCursor(xRight, statusTop + 2); UI.gfx->print(rightMsg);
    UI.gfx->fillRect(chartLeft, chartTop, chartW, chartH, BLACK);
    if (channels > 0) {
      float barWf = (float)chartW / channels;
      for (int i = 0; i < channels; i++) {
        float occ = occupancy(i); int barH = (int)round(occ * (chartH - 2));
        int x0 = chartLeft + (int)floor(i * barWf); int x1 = chartLeft + (int)floor((i + 1) * barWf) - 1; if (x1 < x0) x1 = x0;
        int y0 = chartTop + chartH - barH; uint16_t c = (occ > 0.6f) ? RED : (occ > 0.3f ? ORANGE : (occ > 0.1f ? YELLOW : DARKCYAN));
        UI.gfx->fillRect(x0, y0, x1 - x0 + 1, barH, c);
      }
    }
    UI.gfx->setTextColor(0xC618, BLACK);
    UI.gfx->setCursor(6, UI.gfx->height() - 24); UI.gfx->print("Pot=Threshold   SELECT=Run/Pause");
    UI.gfx->setCursor(6, UI.gfx->height() - 12); UI.gfx->print("Hold=Reset   BACK=Menu");
  }
private:
  static const int MAX_CH = 64;
  float fMin{433.05f}, fMax{434.79f}, chStepKHz{50.0f};
  int channels{0}, scanIdx{0}; uint32_t highCounts[MAX_CH]; uint32_t totalCounts[MAX_CH];
  float thrDbm{-90.0f}; int16_t w{240}, h{135}; int statusTop{24}, statusH{12};
  int chartLeft{6}, chartTop{40}, chartW{228}, chartH{80};
  bool running{true}; uint32_t lastDraw{0};
  float chFreq(int idx) const { float stepMHz = chStepKHz / 1000.0f; float f = fMin + idx * stepMHz; if (f > fMax) f = fMax; return f; }
  float occupancy(int idx) const { uint32_t t = totalCounts[idx]; if (t == 0) return 0.0f; return (float)highCounts[idx] / (float)t; }
};

// Settings
class SettingsPage : public Page {
public:
  const char* title() const override { return "Settings"; }
  void onEnter() override { UI.mustDraw = true; selectMode = true; sel = 0; }
  void update() override;  // defined after MenuPage
  void draw() override {
    if (!UI.mustDraw) return; UI.mustDraw = false;
    drawTopBar(title()); drawDivider(22);
    UI.gfx->fillRect(0, 24, UI.gfx->width(), 12, BLACK);
    UI.gfx->setTextSize(1); UI.gfx->setTextColor(WHITE, BLACK); UI.gfx->setCursor(8, 26);
    UI.gfx->print(selectMode ? "Select field" : "Edit value");
    int16_t w = UI.gfx->width(), h = UI.gfx->height();
    int listTop = 38, rowH = MENU_ROW_H;
    int visibleRows = min(MENU_VISIBLE_ROWS, (int)((h - listTop - 26) / rowH));
    int topIndex = constrain(sel - visibleRows / 2, 0, max(0, itemCount() - visibleRows));
    UI.gfx->fillRect(0, listTop, w, rowH * visibleRows, BLACK);
    for (int i = 0; i < visibleRows; i++) {
      int itemIndex = topIndex + i; if (itemIndex < 0 || itemIndex >= itemCount()) continue;
      int y = listTop + i * rowH; bool isSelected = (itemIndex == sel);
      uint16_t fg = isSelected ? BLACK : WHITE; uint16_t bg = isSelected ? (selectMode ? YELLOW : CYAN) : BLACK;
      if (isSelected) UI.gfx->fillRoundRect(6, y + 2, w - 12, rowH - 4, 6, bg);
      UI.gfx->setTextColor(fg, isSelected ? bg : BLACK); UI.gfx->setTextSize(1); UI.gfx->setCursor(14, y + 4);
      printItemLine(itemIndex);
    }
    UI.gfx->setTextColor(0xC618, BLACK); UI.gfx->setCursor(8, h - 24);
    if (selectMode) UI.gfx->print("Pot=Choose   SELECT=Edit   BACK=Menu");
    else UI.gfx->print("Pot=Adjust   SELECT=Done   BACK=Cancel");
    UI.gfx->setCursor(8, h - 12); UI.gfx->print("Values apply when pages reopen");
  }
private:
  int sel{0}; bool selectMode{true};
  enum Field { BandMin=0, BandMax, WfBins, WfDwellMin, WfDwellMax, RssiMin, RssiMax, OccStepKHz, OccThrDefault, ResetDefaults };
  int itemCount() const { return 10; }
  void printItemLine(int idx) {
    switch (idx) {
      case BandMin:       UI.gfx->print("Band Min: "); UI.gfx->print(S.bandMinMHz, 2); UI.gfx->print(" MHz"); break;
      case BandMax:       UI.gfx->print("Band Max: "); UI.gfx->print(S.bandMaxMHz, 2); UI.gfx->print(" MHz"); break;
      case WfBins:        UI.gfx->print("WF Bins: "); UI.gfx->print(S.wfBins); break;
      case WfDwellMin:    UI.gfx->print("WF Dwell Min: "); UI.gfx->print(S.dwellMinMs); UI.gfx->print(" ms"); break;
      case WfDwellMax:    UI.gfx->print("WF Dwell Max: "); UI.gfx->print(S.dwellMaxMs); UI.gfx->print(" ms"); break;
      case RssiMin:       UI.gfx->print("Color Min: "); UI.gfx->print((int)S.rssiMinDbm); UI.gfx->print(" dBm"); break;
      case RssiMax:       UI.gfx->print("Color Max: "); UI.gfx->print((int)S.rssiMaxDbm); UI.gfx->print(" dBm"); break;
      case OccStepKHz:    UI.gfx->print("Occ Step: "); UI.gfx->print(S.occStepKHz, 0); UI.gfx->print(" kHz"); break;
      case OccThrDefault: UI.gfx->print("Occ Thr def: "); UI.gfx->print((int)S.occThrDbmDefault); UI.gfx->print(" dBm"); break;
      case ResetDefaults: UI.gfx->print("Reset to defaults"); break;
    }
  }
  void applyEdit(int idx, float pot01) {
    switch (idx) {
      case BandMin: { float min=430.0f,max=436.0f; S.bandMinMHz = roundf((min+(max-min)*pot01)*100.0f)/100.0f;
        if (S.bandMinMHz > S.bandMaxMHz - 0.02f) S.bandMinMHz = S.bandMaxMHz - 0.02f; } break;
      case BandMax: { float min=430.0f,max=436.0f; S.bandMaxMHz = roundf((min+(max-min)*pot01)*100.0f)/100.0f;
        if (S.bandMaxMHz < S.bandMinMHz + 0.02f) S.bandMaxMHz = S.bandMinMHz + 0.02f; } break;
      case WfBins: { int minB=40,maxB=220; S.wfBins = minB + (int)round((maxB-minB)*pot01); } break;
      case WfDwellMin: { int minV=1,maxV=20; S.dwellMinMs = minV + (int)round((maxV-minV)*pot01);
        if (S.dwellMinMs >= S.dwellMaxMs) S.dwellMinMs = S.dwellMaxMs - 1; } break;
      case WfDwellMax: { int minV=2,maxV=50; S.dwellMaxMs = minV + (int)round((maxV-minV)*pot01);
        if (S.dwellMaxMs <= S.dwellMinMs) S.dwellMaxMs = S.dwellMinMs + 1; } break;
      case RssiMin: { float min=-130.0f,max=-60.0f; S.rssiMinDbm = min + (max-min)*pot01;
        if (S.rssiMinDbm > S.rssiMaxDbm - 5.0f) S.rssiMinDbm = S.rssiMaxDbm - 5.0f; } break;
      case RssiMax: { float min=-100.0f,max=-20.0f; S.rssiMaxDbm = min + (max-min)*pot01;
        if (S.rssiMaxDbm < S.rssiMinDbm + 5.0f) S.rssiMaxDbm = S.rssiMinDbm + 5.0f; } break;
      case OccStepKHz: { float min=10.0f,max=200.0f; S.occStepKHz = roundf((min+(max-min)*pot01)/5.0f)*5.0f; } break;
      case OccThrDefault: { float min=-110.0f,max=-60.0f; S.occThrDbmDefault = min + (max-min)*pot01; } break;
      case ResetDefaults: { /* handled on press */ } break;
    }
    UI.requestRedraw();
  }
};

// Hamster
class HamsterPage : public Page {
public:
  const char* title() const override { return "Hamster"; }
  void onEnter() override {
    UI.mustDraw = true; running = true; lastFrame = 0; baseDrawn = false;
    for (int i=0;i<STAR_MAX;i++) {
      starX[i] = 20 + (int)random(0, UI.gfx->width()-40);
      starY[i] = 30 + (int)random(0, UI.gfx->height()-60);
      starPh[i] = random(0, 1000) / 1000.0f; wasBright[i] = false;
    }
  }
  void update() override;  // defined after MenuPage
  void draw() override {
    if (!baseDrawn || UI.mustDraw) {
      UI.mustDraw = false; drawTopBar(title()); drawDivider(22);
      UI.gfx->fillRect(0, 24, UI.gfx->width(), UI.gfx->height()-24, BLACK);
      drawHamsterStatic(); for (int i=0;i<STAR_MAX;i++) wasBright[i] = false; baseDrawn = true;
      UI.gfx->setTextColor(0xC618, BLACK);
      UI.gfx->setCursor(6, UI.gfx->height() - 24); UI.gfx->print(running ? "Shining" : "Paused ");
      UI.gfx->print("  SELECT=Start/Stop   BACK=Menu");
      UI.gfx->setCursor(6, UI.gfx->height() - 12); UI.gfx->print("Pot=Speed");
    }
    for (int i=0;i<STAR_MAX;i++) {
      if (wasBright[i]) drawStarCross(starX[i], starY[i], BLACK);
      float b = 0.5f + 0.5f * sinf((starPh[i] + starTime)*6.2831f);
      bool nowBright = (b > 0.85f); uint16_t c = nowBright ? YELLOW : (b > 0.6f ? ORANGE : WHITE);
      UI.gfx->drawPixel(starX[i], starY[i], c); if (nowBright) drawStarCross(starX[i], starY[i], c); wasBright[i] = nowBright;
    }
  }
private:
  static const int STAR_MAX = 18; int starX[STAR_MAX], starY[STAR_MAX]; float starPh[STAR_MAX];
  bool wasBright[STAR_MAX]; float starTime{0.0f}; bool running{true}; bool baseDrawn{false}; uint32_t lastFrame{0};
  void drawHamsterStatic() {
    const int cx = UI.gfx->width()/2, cy = 78;
    UI.gfx->fillRoundRect(cx-38, cy-28, 76, 66, 20, ORANGE);
    UI.gfx->fillRoundRect(cx-28, cy-10, 56, 40, 16, BEIGE);
    UI.gfx->fillCircle(cx-24, cy-28, 6, ORANGE); UI.gfx->fillCircle(cx+24, cy-28, 6, ORANGE);
    UI.gfx->fillCircle(cx-24, cy-28, 3, PINK); UI.gfx->fillCircle(cx+24, cy-28, 3, PINK);
    UI.gfx->fillCircle(cx-12, cy-6, 3, BLACK); UI.gfx->fillCircle(cx+12, cy-6, 3, BLACK);
    UI.gfx->fillCircle(cx-13, cy-7, 1, WHITE); UI.gfx->fillCircle(cx+11, cy-7, 1, WHITE);
    UI.gfx->fillCircle(cx, cy, 2, BROWN); UI.gfx->drawLine(cx-3, cy+6, cx+3, cy+6, BLACK); UI.gfx->drawLine(cx, cy+6, cx, cy+10, BLACK);
    UI.gfx->fillRect(cx-22, cy+28, 8, 4, PINK); UI.gfx->fillRect(cx+14, cy+28, 8, 4, PINK);
  }
  inline void drawStarCross(int x, int y, uint16_t c) { UI.gfx->drawPixel(x-1, y, c); UI.gfx->drawPixel(x+1, y, c); UI.gfx->drawPixel(x, y-1, c); UI.gfx->drawPixel(x, y+1, c); }
};

// ===== Morse helpers and pages =====
static void morseKey(bool on) {
  static bool keyed = false;
  if (!RADIO_READY) return;
  if (on && !keyed) { radio.setOOK(true); (void)radio.transmitDirect(); keyed = true; }
  else if (!on && keyed) { (void)radio.standby(); keyed = false; }
}

// Shared Morse map
struct MC { const char* code; char ch; };
static const MC MORSE_MAP[54] = {
  {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},  {".", 'E'},
  {"..-.", 'F'}, {"--.", 'G'},  {"....", 'H'}, {"..", 'I'},   {".---", 'J'},
  {"-.-", 'K'},  {".-..", 'L'}, {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},
  {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'},  {"...", 'S'},  {"-", 'T'},
  {"..-", 'U'},  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'}, {"-.--", 'Y'},
  {"--..", 'Z'},
  {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'},
  {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'},
  {".-.-.-", '.'}, {"--..--", ','}, {"..--..", '?'}, {"-..-.", '/'}, {"-....-", '-'},
  {"-...-", '='}, {".-.-.", '+'}, {"-.--.", '('}, {"-.--.-", ')'}, {".-..-.", '"'},
  {"---...", ':'}, {"-.-.-.", ';'}, {"-.-.--", '!'}, {".--.-.", '@'}, {"..--.-", '_'},
  {".-...", '&'}, {"...-..-", '$'}
};

// Morse RX page
class MorseRXPage : public Page {
public:
  const char* title() const override { return "Morse RX"; }
  void onEnter() override {
    UI.mustDraw = true;
    running = true;
    thrDbm = -90.0f; thrTargetDbm = thrDbm;
    tuMs = 80.0f; lastSampleMs = millis(); segMs = 0; lastLevel = false; unstable = 0;
    sym = ""; out = ""; sym.reserve(8); out.reserve(320);
    lastEdgeMs = millis();
    if (RADIO_READY) { radio.setFrequency(433.92f); radio.setRxBandwidth(58.0); radio.startReceive(); }
  }
  void update() override;  // after MenuPage
  void draw() override {
    if (!UI.mustDraw) return; UI.mustDraw = false;
    drawTopBar(title()); drawDivider(22);
    UI.gfx->fillRect(0, 24, UI.gfx->width(), 12, BLACK);
    UI.gfx->setTextSize(1); UI.gfx->setTextColor(WHITE, BLACK); UI.gfx->setCursor(6, 26);
    UI.gfx->print(running ? "Running" : "Paused");
    UI.gfx->print("  Thr "); UI.gfx->print((int)round(thrDbm)); UI.gfx->print(" dBm");
    UI.gfx->print("  WPM "); int wpm = (int)round(1200.0f / max(20.0f, tuMs)); UI.gfx->print(wpm);
    int16_t w = UI.gfx->width(), h = UI.gfx->height(); int areaTop = 38, areaH = h - areaTop - 26;
    UI.gfx->fillRect(0, areaTop, w, areaH, BLACK);
    UI.gfx->setTextColor(WHITE, BLACK); UI.gfx->setTextSize(1);
    const int CPL = 38; String buf = out; if (buf.length() > 3*CPL) buf = buf.substring(buf.length() - 3*CPL);
    int lines = (buf.length() + CPL - 1) / CPL; int y = areaTop + 2;
    for (int i = max(0, lines - 3); i < lines; i++) {
      int s = i * CPL; int e = min((i + 1) * CPL, (int)buf.length());
      UI.gfx->setCursor(6, y); for (int k = s; k < e; k++) UI.gfx->print(buf[k]); y += 12;
    }
    UI.gfx->setTextColor(0xC618, BLACK); UI.gfx->setCursor(6, h - 24);
    UI.gfx->print("Pot=Thr  SELECT=Run/Pause");
    UI.gfx->setCursor(6, h - 12); UI.gfx->print("BACK=Menu");
  }
private:
  // adaptive dit time
  float tuMs{80.0f};
  // RX state
  bool running{true};
  float thrDbm{-90.0f}, thrTargetDbm{-90.0f};
  uint32_t lastSampleMs{0}, lastEdgeMs{0};
  uint16_t segMs{0};
  bool lastLevel{false};
  uint8_t unstable{0};
  // buffers
  String sym, out;

  // slow-hand assist (absolute mins)
  uint16_t dahThreshMs() const { uint16_t a=(uint16_t)round(2.2f*tuMs); return (a<320)?320:a; }
  uint16_t letterGapMinMs() const { uint16_t a=(uint16_t)round(2.4f*tuMs); return (a<320)?320:a; }
  uint16_t wordGapMinMs() const { uint16_t a=(uint16_t)round(6.0f*tuMs); return (a<800)?800:a; }

  void appendChar(char c) { if (out.length()>320) out.remove(0, out.length()-280); out += c; }
  void decodeSymbol() {
    if (sym.length()==0) return; char c='?';
    for (size_t i=0;i<sizeof(MORSE_MAP)/sizeof(MORSE_MAP[0]);i++)
      if (strcmp(MORSE_MAP[i].code, sym.c_str())==0) { c=MORSE_MAP[i].ch; break; }
    appendChar(c); sym="";
  }
  void handleHigh(uint16_t onMs) {
    if (onMs >= dahThreshMs()) { sym += '-'; tuMs = 0.90f*tuMs + 0.10f*(onMs/3.0f); }
    else { sym += '.'; tuMs = 0.90f*tuMs + 0.10f*onMs; }
    tuMs = constrain(tuMs, 50.0f, 350.0f);
    if (sym.length()>8) sym="";
    UI.requestRedraw();
  }
  void handleLow(uint16_t offMs) {
    if (offMs >= wordGapMinMs()) { decodeSymbol(); if (out.length()==0 || out[out.length()-1] != ' ') appendChar(' '); UI.requestRedraw(); }
    else if (offMs >= letterGapMinMs()) { decodeSymbol(); UI.requestRedraw(); }
  }
  void finalizeIdle() {
    uint32_t now = millis();
    if (sym.length()>0 && (now - lastEdgeMs) > (uint32_t)letterGapMinMs()) { decodeSymbol(); UI.requestRedraw(); }
  }
};

// Morse TX page (simple: hold SELECT to key; BACK to menu)
class MorseTXPage : public Page {
public:
  const char* title() const override { return "Morse TX"; }
  void onEnter() override {
    UI.mustDraw = true;
    keying = false;
    sym = ""; out = ""; sym.reserve(8); out.reserve(320);
    txClosedAfterEdge = true;
    txLastEdgeMs = millis();
    txDitMsStable = 180; txDitMsCandidate = txDitMsStable; lastDitCandTs = 0;
    if (RADIO_READY) { radio.setFrequency(433.92f); radio.setRxBandwidth(58.0); }
  }
  void onExit() override {
    if (keying) { morseKey(false); keying = false; }
    (void)radio.standby();
  }
  void update() override;  // defined after MenuPage
  void draw() override {
    if (!UI.mustDraw) return; UI.mustDraw = false;
    drawTopBar(title()); drawDivider(22);
    UI.gfx->fillRect(0, 24, UI.gfx->width(), 12, BLACK);
    UI.gfx->setTextSize(1); UI.gfx->setTextColor(WHITE, BLACK); UI.gfx->setCursor(6, 26);
    int estWpm = (int)round(1200.0f / (float)txDitMsStable);
    UI.gfx->print("TX  Dit "); UI.gfx->print(txDitMsStable); UI.gfx->print("ms  ~"); UI.gfx->print(estWpm); UI.gfx->print(" WPM  ");
    UI.gfx->print(keying ? "[KEY DOWN]" : "[key up]");
    int16_t w = UI.gfx->width(), h = UI.gfx->height(); int areaTop = 38, areaH = h - areaTop - 26;
    UI.gfx->fillRect(0, areaTop, w, areaH, BLACK);
    UI.gfx->setTextColor(WHITE, BLACK); UI.gfx->setTextSize(1);
    const int CPL = 38; String buf = out; if (buf.length() > 3*CPL) buf = buf.substring(buf.length() - 3*CPL);
    int lines = (buf.length() + CPL - 1) / CPL; int y = areaTop + 2;
    for (int i = max(0, lines - 3); i < lines; i++) {
      int s = i * CPL; int e = min((i + 1) * CPL, (int)buf.length());
      UI.gfx->setCursor(6, y); for (int k = s; k < e; k++) UI.gfx->print(buf[k]); y += 12;
    }
    // live symbol preview
    UI.gfx->setCursor(6, y + 2); UI.gfx->setTextColor(0xC618, BLACK); UI.gfx->print(">> "); UI.gfx->print(sym);
    UI.gfx->setCursor(6, h - 24); UI.gfx->setTextColor(0xC618, BLACK); UI.gfx->print("Hold SELECT=Key (OOK)");
    UI.gfx->setCursor(6, h - 12); UI.gfx->print("Pot=Dit (speed)   BACK=Menu");
  }
private:
  bool keying{false};
  bool txClosedAfterEdge{true};
  uint32_t txSegStartMs{0};
  uint32_t txLastEdgeMs{0};
  uint16_t txDitMsStable{180}, txDitMsCandidate{180};
  uint32_t lastDitCandTs{0};
  String sym, out;

  // Easier thresholds
  uint16_t txDahThreshMs() const { uint16_t p = (uint16_t)(2.0f * (float)txDitMsStable); return (p < 240) ? 240 : p; }
  uint16_t txLetterGapMinMs() const { uint16_t g = (uint16_t)(2.0f * (float)txDitMsStable); return (g < 280) ? 280 : g; }
  uint16_t txWordGapMinMs() const { uint16_t g = (uint16_t)(5.5f * (float)txDitMsStable); return (g < 750) ? 750 : g; }

  void appendChar(char c) { if (out.length()>320) out.remove(0, out.length()-280); out += c; }
  void decodeSymbol() {
    if (sym.length()==0) return; char c='?';
    for (size_t i=0;i<sizeof(MORSE_MAP)/sizeof(MORSE_MAP[0]);i++)
      if (strcmp(MORSE_MAP[i].code, sym.c_str())==0) { c=MORSE_MAP[i].ch; break; }
    appendChar(c); sym="";
  }
};

// ===== Menu =====
class MenuPage : public Page {
public:
  MenuPage(PageManager &mgr) : manager(mgr) {
    items[0] = "Radio"; items[1] = "Waterfall"; items[2] = "Occupancy";
    items[3] = "Settings"; items[4] = "Hamster"; items[5] = "Morse RX"; items[6] = "Morse TX"; itemCount = 7;
  }
  const char* title() const override { return "Menu"; }
  void onEnter() override {
    float v = pot.read01(); stableSel = discreteCandidate(v, -1);
    pendingSel = stableSel; pendingSince = millis(); lastTopIndex = -1; UI.mustDraw = true;
  }
  void update() override {
    if (btnBack.pressed()) { manager.push(&homePage); return; }
    float v = pot.read01(); int cand = discreteCandidate(v, stableSel); uint32_t now = millis();
    if (cand != pendingSel) { pendingSel = cand; pendingSince = now; }
    else if (pendingSel != stableSel && (now - pendingSince) >= MENU_COMMIT_MS) { stableSel = pendingSel; UI.mustDraw = true; }
    if (btnSelect.pressed()) {
      const char *name = items[stableSel];
      if      (strcmp(name, "Radio")     == 0) manager.push(&radioPage);
      else if (strcmp(name, "Waterfall") == 0) manager.push(&waterfallPage);
      else if (strcmp(name, "Occupancy") == 0) manager.push(&occupancyPage);
      else if (strcmp(name, "Settings")  == 0) manager.push(&settingsPage);
      else if (strcmp(name, "Hamster")   == 0) manager.push(&hamsterPage);
      else if (strcmp(name, "Morse RX")  == 0) manager.push(&morseRXPage);
      else if (strcmp(name, "Morse TX")  == 0) manager.push(&morseTXPage);
    }
  }
  void draw() override {
    int16_t w = UI.gfx->width(), h = UI.gfx->height();
    const int listTop = 26, rowH = MENU_ROW_H;
    const int visibleRows = min(MENU_VISIBLE_ROWS, (int)((h - listTop - 16) / rowH));
    drawTopBar(title()); drawDivider(22);
    if (!(UI.mustDraw || stableSel != lastSel)) return; UI.mustDraw = false;
    int topIndex = constrain(stableSel - visibleRows / 2, 0, max(0, itemCount - visibleRows));
    if (topIndex != lastTopIndex) { lastTopIndex = topIndex; }
    UI.gfx->fillRect(0, listTop, w, rowH * visibleRows, BLACK);
    for (int i = 0; i < visibleRows; i++) {
      int itemIndex = topIndex + i; if (itemIndex < 0 || itemIndex >= itemCount) continue;
      int y = listTop + i * rowH; bool isSelected = (itemIndex == stableSel);
      uint16_t fg = isSelected ? BLACK : WHITE; uint16_t bg = isSelected ? YELLOW : BLACK;
      if (isSelected) UI.gfx->fillRoundRect(6, y + 2, w - 12, rowH - 4, 6, bg);
      UI.gfx->setTextColor(fg, isSelected ? bg : BLACK); UI.gfx->setTextSize(2); UI.gfx->setCursor(14, y + 4);
      UI.gfx->print(items[itemIndex]);
    }
    UI.gfx->setTextSize(1); UI.gfx->setTextColor(0xC618, BLACK); UI.gfx->setCursor(8, h - 12);
    UI.gfx->print("Pot=Scroll  SELECT=Open  BACK=Home");
    lastSel = stableSel;
  }
private:
  PageManager &manager;
  static const int MAX_ITEMS = 12;
  const char *items[MAX_ITEMS]; int itemCount{0};
  int stableSel{0}, pendingSel{0}; uint32_t pendingSince{0}; int lastSel{-1}, lastTopIndex{-1};
  int discreteCandidate(float v01, int prev) const {
    if (itemCount <= 1) return 0; v01 = constrain(v01, 0.0f, 1.0f); float step = 1.0f / itemCount;
    if (prev < 0) { int idx = (int)floor(v01 / step + 1e-4f); return constrain(idx, 0, itemCount - 1); }
    float band = MENU_HYSTERESIS * step; if (band > 0.49f * step) band = 0.49f * step;
    float prevStart = prev * step, prevEnd = (prev + 1) * step; float downThresh = prevStart + band, upThresh = prevEnd - band;
    if (v01 < downThresh && prev > 0) return prev - 1; if (v01 > upThresh && prev < itemCount-1) return prev + 1; return prev;
  }
};

// ----- Instantiate pages -----
WaterfallPage waterfallPage;
OccupancyPage occupancyPage;
RadioPage radioPage;
SettingsPage settingsPage;
HamsterPage hamsterPage;
MorseRXPage morseRXPage;
MorseTXPage morseTXPage;
MenuPage menuPage(UI);

// ===== Implement methods AFTER MenuPage =====

void HomePage::update() {
  if (btnSelect.pressed()) { UI.push(&menuPage); }
}

void RadioPage::update() {
  if (btnBack.pressed()) { UI.push(&menuPage); return; }
  if (btnSelect.pressed() && RADIO_READY) {
    String msg = "Ping " + String(++txCount);
    (void)radio.transmit(msg);
    radio.startReceive();
    UI.requestRedraw();
  }
  if (RADIO_READY && radio.available()) {
    String str; int st = radio.readData(str);
    if (st == RADIOLIB_ERR_NONE) { lastRx = str; lastRssi = radio.getRSSI(); rxCount++; }
    else { lastRx = String("RX err ") + st; }
    radio.startReceive();
    UI.requestRedraw();
  }
}

void WaterfallPage::update() {
  if (btnBack.pressed() || btnBack.held()) { UI.push(&menuPage); return; }
  if (!RADIO_READY) return;
  if (btnSelect.pressed()) { running = !running; UI.requestRedraw(); }
  int dmin = max(1, S.dwellMinMs), dmax = max(dmin+1, S.dwellMaxMs);
  dwellMs = dmin + (int)round((dmax - dmin) * pot.read01());
  if (!running) return;
  const int budgetMs = 28; uint32_t t0 = millis();
  while ((int)(millis() - t0) < budgetMs) {
    if (btnBack.pressed() || btnBack.held()) { UI.push(&menuPage); return; }
    float f = fMin + curBin * binStepMHz; radio.setFrequency(f); radio.startReceive();
    if (!responsiveWaitMs(dwellMs)) {
      if (btnBack.held() || btnBack.pressed()) { UI.push(&menuPage); return; }
      if (btnSelect.pressed()) { running = false; UI.requestRedraw(); return; }
    }
    float rssi = radio.getRSSI();
    float norm = (rssi - minDbm) / (maxDbm - minDbm);
    uint8_t v = (uint8_t)constrain((int)(norm * 255.0f), 0, 255);
    wfBuf[writeRow][curBin] = v;
    if (v > peakHold[curBin]) peakHold[curBin] = v; else if (peakHold[curBin] > 0) peakHold[curBin] -= 1;
    curBin++;
    if (curBin >= bins) { curBin = 0; writeRow++; if (writeRow >= wfH) writeRow = 0; UI.requestRedraw(); break; }
  }
  static uint32_t last = 0; uint32_t now = millis(); if (now - last > 100) { last = now; UI.requestRedraw(); }
}

void OccupancyPage::update() {
  if (btnBack.pressed()) { UI.push(&menuPage); return; }
  if (btnSelect.pressed()) { running = !running; UI.requestRedraw(); }
  static uint32_t heldStart = 0; static bool didResetHold = false; uint32_t now = millis();
  if (btnSelect.held()) { if (heldStart == 0) { heldStart = now; didResetHold = false; }
    if (!didResetHold && now - heldStart > 800) { memset(highCounts, 0, sizeof(highCounts)); memset(totalCounts, 0, sizeof(totalCounts)); didResetHold = true; UI.requestRedraw(); } }
  else { heldStart = 0; didResetHold = false; }
  static float lastThrTarget = thrDbm;
  float v = pot.read01(0.06f);
  float tgt = -110.0f + 50.0f * v;
  if (fabsf(tgt - lastThrTarget) >= 1.0f) lastThrTarget = tgt;
  thrDbm = 0.8f * thrDbm + 0.2f * lastThrTarget;

  if (!RADIO_READY || !running) return;
  const int budgetMs = 24; uint32_t t0 = millis();
  while ((int)(millis() - t0) < budgetMs) {
    if (btnBack.pressed() || btnBack.held()) { UI.push(&menuPage); return; }
    float f = chFreq(scanIdx); radio.setFrequency(f); radio.startReceive();
    if (!responsiveWaitMs(3)) return;
    float rssi = radio.getRSSI(); totalCounts[scanIdx]++; if (rssi >= thrDbm) highCounts[scanIdx]++; scanIdx = (scanIdx + 1) % channels;
  }
  if (now - lastDraw > 200) { lastDraw = now; UI.requestRedraw(); }
}

void SettingsPage::update() {
  if (selectMode) {
    if (btnBack.pressed()) { UI.push(&menuPage); return; }
    int cnt = itemCount(); int idx = (int)floor(pot.read01() * cnt + 1e-6f); if (idx >= cnt) idx = cnt - 1;
    if (idx != sel) { sel = idx; UI.requestRedraw(); }
    if (btnSelect.pressed()) { selectMode = false; UI.requestRedraw(); }
  } else {
    float v = pot.read01();
    if (sel == ResetDefaults) {
      if (btnSelect.pressed()) { S = Settings(); selectMode = true; UI.requestRedraw(); }
      if (btnBack.pressed()) { selectMode = true; UI.requestRedraw(); }
    } else {
      applyEdit(sel, v);
      if (btnSelect.pressed() || btnBack.pressed()) { selectMode = true; UI.requestRedraw(); }
    }
  }
}

void HamsterPage::update() {
  if (btnBack.pressed()) { UI.push(&menuPage); return; }
  if (btnSelect.pressed()) { running = !running; UI.mustDraw = true; }
  uint32_t now = millis(); if (lastFrame == 0) lastFrame = now; if (now - lastFrame < 33) return;
  float dt = (now - lastFrame) / 1000.0f;
  if (running && dt > 0.0f) { float speed = 0.2f + 2.8f * pot.read01(); starTime += dt * speed * 0.7f; lastFrame = now; draw(); }
  else { lastFrame = now; }
}

// Morse RX update
void MorseRXPage::update() {
  if (btnBack.pressed()) { UI.push(&menuPage); return; }
  if (btnSelect.pressed()) { running = !running; UI.requestRedraw(); }

  float p = pot.read01(0.06f);
  float target = -110.0f + 50.0f * p;
  if (fabsf(target - thrTargetDbm) >= 1.0f) thrTargetDbm = target;
  thrDbm = 0.8f * thrDbm + 0.2f * thrTargetDbm;

  if (!RADIO_READY || !running) { finalizeIdle(); return; }

  uint32_t now = millis();
  const int budgetMs = 22, step = 4;
  uint32_t t0 = now;
  while ((int)(millis() - t0) < budgetMs) {
    if ((uint32_t)(millis() - lastSampleMs) < (uint32_t)step) break;
    lastSampleMs += step;

    float rssi = radio.getRSSI();
    bool level = (rssi >= thrDbm);

    if (level == lastLevel) { segMs += step; unstable = 0; }
    else {
      unstable++;
      if (unstable >= 2) {
        bool prev = lastLevel; uint16_t dur = segMs;
        segMs = step; lastLevel = level; unstable = 0; lastEdgeMs = millis();
        if (prev) handleHigh(dur); else handleLow(dur);
      }
    }
    yield();
  }
  finalizeIdle();
}

// Morse TX update (live-read button + reliable gap commit)
void MorseTXPage::update() {
  if (btnBack.pressed()) { UI.push(&menuPage); return; }

  uint32_t now = millis();

  // Pot -> dit time (120..280 ms), commit after 150 ms stable
  {
    float p = pot.read01(0.06f);
    uint16_t cand = 120 + (uint16_t)roundf(p * 160.0f);
    if (cand != txDitMsCandidate) { txDitMsCandidate = cand; lastDitCandTs = now; }
    if ((now - lastDitCandTs) > 150 && txDitMsStable != txDitMsCandidate) { txDitMsStable = txDitMsCandidate; UI.requestRedraw(); }
  }

  // Live SELECT state (uses fixed held() which reads the pin directly)
  bool selDown = btnSelect.held();

  // Start of press: commit previous letter/word if gap long enough
  if (!keying && selDown) {
    uint32_t gap = now - txLastEdgeMs;
    if (!txClosedAfterEdge) {
      if (gap >= txWordGapMinMs()) {
        decodeSymbol();
        if (out.length() == 0 || out[out.length()-1] != ' ') out += ' ';
        txClosedAfterEdge = true;
      } else if (gap >= txLetterGapMinMs()) {
        decodeSymbol();
        txClosedAfterEdge = true;
      }
    }
    keying = true;
    morseKey(true);
    txSegStartMs = now;
    UI.requestRedraw();
    return;
  }

  // Release: classify element
  if (keying && !selDown) {
    keying = false;
    morseKey(false);
    uint16_t pressMs = (uint16_t)(now - txSegStartMs);
    if (pressMs >= txDahThreshMs()) sym += '-'; else sym += '.';
    if (sym.length() > 8) sym = "";
    txClosedAfterEdge = false;
    txLastEdgeMs = now;
    UI.requestRedraw();
  }

  // Passive commit during idle (fallback)
  if (!keying && !txClosedAfterEdge) {
    uint32_t offMs = now - txLastEdgeMs;
    if (offMs >= txWordGapMinMs()) {
      decodeSymbol();
      if (out.length() == 0 || out[out.length()-1] != ' ') out += ' ';
      txClosedAfterEdge = true;
      UI.requestRedraw();
    } else if (offMs >= txLetterGapMinMs()) {
      decodeSymbol();
      txClosedAfterEdge = true;
      UI.requestRedraw();
    }
  }

  // Flush letter if idle too long with partial symbol
  if (!keying && sym.length() > 0) {
    uint32_t idleMs = now - txLastEdgeMs;
    if (idleMs > txLetterGapMinMs()) { decodeSymbol(); UI.requestRedraw(); }
  }
}

// ----- Boot / Setup / Loop -----
void drawBoot() {
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE, BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 34); tft.print(PRODUCT_NAME);
  tft.setTextSize(1);
  tft.setCursor(10, 60); tft.print("Spectrum + Occupancy + Morse RX/TX");
  tft.setCursor(10, 76); tft.print("ST7789 135x240 ROT=3");
}

void setup() {
  Serial.begin(115200);
  delay(50);
  backlightBegin();
  // Shared SPI (VSPI)
  SPI.begin(18, 19, 23);
  pinMode(RF_CS, OUTPUT); digitalWrite(RF_CS, HIGH);
  // Display init
  tft.init(135, 240); tft.setRotation(3); tft.setSPISpeed(TFT_SPI_HZ);
  // Inputs
  btnSelect.begin(); btnBack.begin(); pot.begin();
  // RNG
  randomSeed(analogRead(POT_PIN) ^ millis());
  // UI
  UI.begin(&tft);
  drawBoot();
  UI.mustDraw = true;
  UI.push(&homePage);
  // Radio init (433.92 MHz baseline)
  Serial.println(F("Init CC1101..."));
  RADIO_INIT_CODE = radio.begin(433.92);
  RADIO_READY = (RADIO_INIT_CODE == RADIOLIB_ERR_NONE);
  if (RADIO_READY) {
    radio.setBitRate(4.8);
    radio.setFrequencyDeviation(10.0);
    radio.setRxBandwidth(58.0);
    radio.setOutputPower(10);
    radio.startReceive();
    Serial.println(F("CC1101 READY"));
  } else {
    Serial.print(F("CC1101 init failed, code ")); Serial.println(RADIO_INIT_CODE);
  }
}

void loop() {
  UI.tick();
  delay(8);
}
