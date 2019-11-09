
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include <ESP8266WiFi.h>  // Include the Wi-Fi library
#include <ESP8266HTTPClient.h>
#include <FS.h>
#include <i2s.h>
#include <i2s_reg.h>
#include <JPEGDecoder.h>
#include "wavspiffs.h"

const char* ssid     = "MY_HOUSE_WIFI";         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "TRY_HARDER!";     // The password of the Wi-Fi network

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
HTTPClient http;

//*******************

// This is the file name used to store the calibration data
// You can change this to create new calibration files.
// The SPIFFS file name must start with "/".
#define CALIBRATION_FILE "/TouchCalData1"

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false

// Keypad start position, key sizes and spacing
#define KEY_X 40 // Centre of key
#define KEY_Y 96
#define KEY_W 62 // Width and height
#define KEY_H 30
#define KEY_SPACING_X 18 // X and Y gap
#define KEY_SPACING_Y 20
#define KEY_TEXTSIZE 1   // Font size multiplier

// Using two fonts since numbers are nice when bold
#define LABEL1_FONT &FreeSansOblique12pt7b // Key label font 1
#define LABEL2_FONT &FreeSansBold12pt7b    // Key label font 2

// Numeric display box size and location
#define DISP_X 1
#define DISP_Y 10
#define DISP_W 438
#define DISP_H 50
#define DISP_TSIZE 3
#define DISP_TCOLOR TFT_CYAN

// Number length, buffer for storing it and character index
#define NUM_LEN 12
char numberBuffer[NUM_LEN + 1] = "";
uint8_t numberIndex = 0;

// We have a status line for messages
#define STATUS_X 120 // Centred on this
#define STATUS_Y 65

// Create 15 keys for the keypad
char keyLabel[15] = "play music";
uint16_t keyColor = TFT_RED;


TFT_eSPI_Button key;

void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin()) {
    Serial.println("Formating file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      fs::File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    fs::File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}

void drawPlayButton()
{
  // Draw the keys
//  tft.setFreeFont(LABEL1_FONT);
  

  key.initButton(&tft, KEY_X + (KEY_W + KEY_SPACING_X),
                        KEY_Y + (KEY_H + KEY_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEY_W, KEY_H, TFT_WHITE, keyColor, TFT_WHITE,
                        keyLabel, KEY_TEXTSIZE);
  key.drawButton();
}



//******************

#define TEXT_HEIGHT 8 // Height of text to be printed and scrolled
#define BOT_FIXED_AREA 0  // Number of lines in bottom fixed area (lines counted from bottom of screen)
#define TOP_FIXED_AREA 0  // Number of lines in top fixed area (lines counted from top of screen)

uint16_t yStart = TOP_FIXED_AREA;
uint16_t yArea = 320 - TOP_FIXED_AREA - BOT_FIXED_AREA;
uint16_t yDraw = 320 - BOT_FIXED_AREA - TEXT_HEIGHT;
byte     pos[42];
uint16_t xPos = 0;

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println('\n');
  
//  WiFi.begin(ssid, password);             // Connect to the network
//  Serial.print("Connecting to ");
//  Serial.print(ssid); Serial.println(" ...");
//
//  int i = 0;
////  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
////    delay(1000);
////    Serial.print(++i); Serial.print(' ');
////  }
//
//  Serial.println('\n');
//  Serial.println("Connection established!");  
//  Serial.print("IP address:\t");
//  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer

  SPIFFS.begin();                           // Start the SPI Flash Files System
  
  randomSeed(analogRead(A0));
  tft.init();
  Serial.print("setup init\n");
  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);
  Serial.print("setup black\n");
  setupScrollArea(TOP_FIXED_AREA, BOT_FIXED_AREA);
  wav_setup();
  Serial.print("setup complete\n");
  String str = "";
  fs::Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    str += dir.fileName();
    str += " / ";
    str += dir.fileSize();
    str += "\r\n";
  }
  Serial.print(str);
  draw_image();
  startPlaying("/game_over.wav");
  touch_calibrate();
}

void loop(void) {
    Serial.print("loop\n");
    
    phack_scroll();
    delay(1000);
    phack_print();
    draw_image();
    tft.fillScreen(ILI9341_BLACK);
    drawPlayButton();
    int i = 0;
    while (i < 50 ){
      uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
      // Pressed will be set true is there is a valid touch on the screen
      boolean pressed = tft.getTouch(&t_x, &t_y);
      
      // Check if any key coordinate boxes contain the touch coordinates
      if (pressed && key.contains(t_x, t_y)) {
            key.press(true);  // tell the button it is pressed
      } else {
            key.press(false);  // tell the button it is NOT pressed
          }
      
      if (key.justReleased()) key.drawButton();     // draw normal
          if (key.justPressed()) {
            key.drawButton(true);  // draw invert
            startPlaying("/game_over.wav");
            
      
            delay(10); // UI debouncing
          }
    i++;
    }
    setupScrollArea(TOP_FIXED_AREA, BOT_FIXED_AREA);

}
 

void draw_image(void){
  tft.setRotation(2);  // portrait
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 10, 1);
  int randNumber = random(3);
  switch (randNumber) {
  case 1:
    drawJpeg("/27.jpeg", 0 , 10);
    break;
  case 2:
    drawJpeg("/noqtr.jpeg", 0 , 10);
    break;
  default:
    drawJpeg("/phack.jpeg", 0 , 10);
    break;
  }
  delay(2000);
}


void download_image(void){
  String file_name = "phack.jpeg";
  String url= "http://example.com";
  fs::File f = SPIFFS.open(file_name, "w");
    if (f) {
      http.begin(url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          http.writeToStream(&f);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      f.close();
    }
    http.end();
}

void phack_print(void){
  tft.init();
  tft.fillScreen(TFT_BLACK);
  
  // Set "cursor" at top left corner of display (0,0) and select font 2
  // (cursor will move to next line automatically during printing with 'tft.println'
  //  or stay on the line is there is room for the text with tft.print)
  tft.setCursor(50, 80);
  Serial.printf("cursor Y location %d\n",tft.getCursorY());
  // Set the font colour to be white with a black background, set text size multiplier to 1
  tft.setRotation(2);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);  tft.setTextSize(4);
  // We can now plot text on screen using the "print" class
  tft.println("");
  tft.println(" Pacific");
  tft.setTextColor(TFT_WHITE,TFT_BLACK);  tft.setTextSize(4);
  tft.println(" Hackers");
  delay(5000);
    
}

void phack_scroll(void){
  // First fill the screen with random streaks of characters
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(1);
  for (int j = 0; j < 600; j += TEXT_HEIGHT) {
    for (int i = 0; i < 40; i++) {
      if (pos[i] > 20) pos[i] -= 3; // Rapid fade initially brightness values
      if (pos[i] > 0) pos[i] -= 1; // Slow fade later
      if ((random(20) == 1) && (j<400)) pos[i] = 63; // ~1 in 20 probability of a new character
      tft.setTextColor(pos[i] << 5, ILI9341_BLACK); // Set the green character brightness
      if (pos[i] == 63) tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK); // Draw white character
      xPos += tft.drawChar(random(32, 128), xPos, yDraw, 1); // Draw the character
    }
    yDraw = scroll_slow(TEXT_HEIGHT, 14); // Scroll, 14ms per pixel line
    xPos = 0;
    
}

  tft.setRotation(2);
  tft.setTextColor(63 << 5, ILI9341_BLACK);
  tft.drawCentreString("Pacific Hackers",120,60,4);
  tft.setRotation(0);

  // Now scroll smoothly forever
  for (int i = 1; i < 4; i++) {
    yield(); 
    yDraw = scroll_slow(310,5); 
  }// Scroll 320 lines, 5ms per line  
}

void setupScrollArea(uint16_t TFA, uint16_t BFA) {
  tft.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
  tft.writedata(TFA >> 8);
  tft.writedata(TFA);
  tft.writedata((320 - TFA - BFA) >> 8);
  tft.writedata(320 - TFA - BFA);
  tft.writedata(BFA >> 8);
  tft.writedata(BFA);
}

int scroll_slow(int lines, int wait) {
  int yTemp = yStart;
  for (int i = 0; i < lines; i++) {
    yStart++;
    if (yStart == 320 - BOT_FIXED_AREA) yStart = TOP_FIXED_AREA;
    scrollAddress(yStart);
    delay(wait);
  }
  return  yTemp;
}

void scrollAddress(uint16_t VSP) {
  tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling start address
  tft.writedata(VSP >> 8);
  tft.writedata(VSP);
}




//*****************


// Non-blocking I2S write for left and right 16-bit PCM
bool ICACHE_FLASH_ATTR i2s_write_lr_nb(int16_t left, int16_t right){
  int sample = right & 0xFFFF;
  sample = sample << 16;
  sample |= left & 0xFFFF;
  return i2s_write_sample_nb(sample);
}

struct I2S_status_s {
  wavFILE_t wf;
  int16_t buffer[512];
  int bufferlen;
  int buffer_index;
  int playing;
} I2S_WAV;

void wav_stopPlaying()
{
  i2s_end();
  I2S_WAV.playing = false;
  wavClose(&I2S_WAV.wf);
}

bool wav_playing()
{
  return I2S_WAV.playing;
}

void wav_setup()
{
  Serial.println(F("wav_setup"));
  I2S_WAV.bufferlen = -1;
  I2S_WAV.buffer_index = 0;
  I2S_WAV.playing = false;
}

void wav_loop()
{
  bool i2s_full = false;
  int rc;

  while (I2S_WAV.playing && !i2s_full) {
    while (I2S_WAV.buffer_index < I2S_WAV.bufferlen) {
      int16_t pcm = I2S_WAV.buffer[I2S_WAV.buffer_index];
      if (i2s_write_lr_nb(pcm, pcm)) {
        I2S_WAV.buffer_index++;
      }
      else {
        i2s_full = true;
        break;
      }
      if ((I2S_WAV.buffer_index & 0x3F) == 0) yield();
    }
    if (i2s_full) break;

    rc = wavRead(&I2S_WAV.wf, I2S_WAV.buffer, sizeof(I2S_WAV.buffer));
    if (rc > 0) {
      Serial.printf("wavRead %d\r\n", rc);
      I2S_WAV.bufferlen = rc / sizeof(I2S_WAV.buffer[0]);
      I2S_WAV.buffer_index = 0;
    }
    else {
      Serial.println(F("Stop playing"));
      wav_stopPlaying();
      break;
    }
  }
}

void wav_startPlayingFile(const char *wavfilename)
{
  wavProperties_t wProps;
  int rc;

  Serial.printf("wav_starPlayingFile(%s)\r\n", wavfilename);
  i2s_begin();
  rc = wavOpen(wavfilename, &I2S_WAV.wf, &wProps);
  Serial.printf("wavOpen %d\r\n", rc);
  if (rc != 0) {
    Serial.println("wavOpen failed");
    return;
  }
  Serial.printf("audioFormat %d\r\n", wProps.audioFormat);
  Serial.printf("numChannels %d\r\n", wProps.numChannels);
  Serial.printf("sampleRate %d\r\n", wProps.sampleRate);
  Serial.printf("byteRate %d\r\n", wProps.byteRate);
  Serial.printf("blockAlign %d\r\n", wProps.blockAlign);
  Serial.printf("bitsPerSample %d\r\n", wProps.bitsPerSample);

  i2s_set_rate(wProps.sampleRate);

  I2S_WAV.bufferlen = -1;
  I2S_WAV.buffer_index = 0;
  I2S_WAV.playing = true;
  while(I2S_WAV.playing){
    wav_loop();
  }
}

void startPlaying(const char *filename)
{
  char nowPlaying[80] = "nowPlaying";

  wav_startPlayingFile(filename);
  strncat(nowPlaying, filename, sizeof(nowPlaying)-strlen(nowPlaying)-1);
//  webSocket.broadcastTXT(nowPlaying);
}




//******************
