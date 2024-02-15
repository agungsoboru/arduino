#include <SPFD5408_Adafruit_GFX.h>    // Core graphics  library
#include <SPFD5408_Adafruit_TFTLCD.h> // Hardware-specific library
#include  <SPI.h>
#include <SD.h>

#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
#define LCD_RESET A4

#define  SD_CS 10  //SD card pin on your shield

// Agar warna mudah dimengerti (Human Readable color):
#define TFT_BLACK   0x0000
#define TFT_CYAN    0x07FF
#define TFT_WHITE   0xFFFF

#define BUFFPIXEL 1           //Drawing  speed, 20 is meant to be the best but you can use 60 altough it takes a lot of uno's  RAM         

bool fillScreenExecuted = false;

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;    // Saved H, M, S x & y multipliers
float sdeg = 0, mdeg = 0, hdeg = 0;
uint16_t osx = 120, osy = 120, omx = 120, omy = 120, ohx = 120, ohy = 120; // Saved H, M, S x & y coords
int16_t x0 = 0, x1 = 0, yy0 = 0, yy1 = 0, x00 = 0, yy00 = 0;
uint32_t targetTime = 0;                    // for next 1 second timeout

uint16_t xpos; // x posisi jam
uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}
uint8_t hh = conv2d(__TIME__), mm = conv2d(__TIME__ + 3), ss = conv2d(__TIME__ + 6); // mengambil data waktu dari jam Compile-Upload
boolean initial = 1;
char d;


void setup(void) {
  tft.reset();  //perlu saat menggunakan lib.SPFD5408_Adafruit_TFTLCD.h
  tft.begin(0x9341); //perlu address ini saat menggunakan lib.SPFD5408_Adafruit_TFTLCD.h
  tft.setRotation(0);

  tft.setTextColor(TFT_WHITE);//warna text
  tft.fillScreen(TFT_BLACK);//warna latar

  // Draw clock face
  xpos = tft.width() / 2; // mencari titik koordinat tengah LCD




  targetTime = millis() + 1000;



  uint16_t identifier = tft.readID();
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  //tft.begin(identifier);
  if (!SD.begin(SD_CS))  {
  //progmemPrintln(PSTR("failed!"));
  return;
  }
}

 
void loop() {
  //bmpDraw("hutao.bmp", 70, 0);

  //jam();

 if (!fillScreenExecuted) {
  bmpDraw("hutao.bmp", 0, 0);
    //tft.fillScreen(TFT_BLACK);
    fillScreenExecuted = true; // Set the flag to true so it won't execute again
  }

  // Your other code, for example, bmpDraw
  
  jam();


}



void bmpDraw(char *filename,  int x, int y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   //  W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must  be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t  rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL];  // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel  out buffer (16-bit per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current  position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid  header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime  = millis();
  uint8_t  lcdidx = 0;
  boolean  first = true;

  if((x  >= tft.width()) || (y >= tft.height())) return;

  //Serial.println();
  //progmemPrint(PSTR("Loading  image '"));
  //Serial.print(filename);
  //Serial.println('\\');
  // Open  requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    //progmemPrintln(PSTR("File not found"));
    return;
  }

  //  Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    //progmemPrint(PSTR("File  size: ")); 
    (read32(bmpFile));
    (void)read32(bmpFile); // Read  & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image  data
    //progmemPrint(PSTR("Image Offset: ")); 
    (bmpImageoffset,  DEC);
    // Read DIB header
    //progmemPrint(PSTR("Header size: ")); 
    read32(bmpFile);
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile)  == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits  per pixel
      //progmemPrint(PSTR("Bit Depth: ")); //Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        //progmemPrint(PSTR("Image  size: "));
        // Serial.print(bmpWidth);
        // Serial.print('x');
        // Serial.println(bmpHeight);

        // BMP rows are padded (if needed)  to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        //  If bmpHeight is negative, image is in top-down order.
        // This is not  canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight  = -bmpHeight;
          flip      = false;
        }

        // Crop  area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1)  >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h  = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h;  row++) { // For each scanline...
          // Seek to start of scan line.  It  might seem labor-
          // intensive to be doing this on every line, but  this
          // method covers a lot of gritty details like cropping
          //  and scanline padding.  Also, the seek only takes
          // place if the file  position actually needs to change
          // (avoids a lot of cluster math  in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal  BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset  + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force  buffer reload
          }

          for (col=0; col<w; col++) { // For  each column...
            // Time to read more pixel data?
            if  (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to  the display first
              if(lcdidx > 0) {
                tft.pushColors(lcdbuffer,  lcdidx, first);
                lcdidx = 0;
                first  = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx  = 0; // Set index to beginning
            }

            // Convert pixel  from BMP to TFT format
            b = sdbuffer[buffidx++];
            g  = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++]  = tft.color565(r,g,b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if(lcdidx > 0) {
          tft.pushColors(lcdbuffer,  lcdidx, first);
        } 
        //progmemPrint(PSTR("Loaded in "));
        //Serial.print(millis() - startTime);
        //Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  
}


void jam() {

if (targetTime < millis()) {
    targetTime = millis() + 1000;
    ss++;              // Advance second

    if (ss == 60) {
      ss = 0;
      mm++;            // Advance minute

      if (mm > 59) {
        mm = 0;
        hh++;          // Advance hour

        if (hh > 23) {
          hh = 0;
        }
      }
    }

  tft.fillCircle(xpos, 131, 1, TFT_WHITE); //titik tengah : pada jam dan menit
  tft.fillCircle(xpos, 110, 1, TFT_WHITE);

  tft.drawCircle(xpos+35, 117, 17, TFT_CYAN);
  tft.drawCircle(xpos+35, 117, 16, TFT_CYAN);
  tft.fillRect(xpos+25, 111,22,15,TFT_BLACK); //erase

  if(mm<10){tft.setCursor(xpos+25, 111); tft.setTextSize(2);
  tft.print('0'); tft.setCursor(xpos+37, 111);}
  else{
  tft.setCursor(xpos+25, 111);}
  tft.println(mm);
  tft.drawCircle(xpos-35, 117, 17, TFT_CYAN);
  tft.drawCircle(xpos-35, 117, 16, TFT_CYAN);
  tft.fillRect(xpos-45, 111,22,15,TFT_BLACK); //erase

  if(hh<10){tft.setCursor(xpos-45, 111); tft.setTextSize(2);
  tft.print('0'); tft.setCursor(xpos-33, 111);}
  else{
  tft.setCursor(xpos-45, 111);}
  tft.setTextSize(2);
  tft.print(hh);

  if (hh>=0 && hh<12) d='A'; else {d='P';}
  tft.drawRoundRect(xpos-14,72,29,21,5,TFT_CYAN);
  //tft.fillRect(xpos-11, 75,23,15,TFT_BLACK); //erase
  tft.setCursor(xpos-11, 75);
  tft.setTextSize(2);
  tft.print(d);
  tft.println('M');
  }
}


// These read  16- and 32-bit types from the SD card file.
// BMP data is stored little-endian,  Arduino is little-endian too.
// May need to reverse subscript order if porting  elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t  *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); //  MSB
  return result;
}