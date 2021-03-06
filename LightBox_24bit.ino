#include <FastLED.h>
#include <SD.h>

#define numRows 6
#define rowLength 10
#define brightness 150
#define color_order GRB

struct RGBColor {
  byte r;
  byte g;
  byte b;

  SetRGB(byte _r, byte _g, byte _b)
  {
    r = _r;
    g = _g;
    b = _b;
  }
};

CRGB rows[numRows][rowLength];

File root;
//File currentFile;

//parameters to control the speed of image transitions
float transitionSteps = 60.0; //the number of steps in a full transition cycle (only works when this is a float)
int transitionStepDelay = 50; //the amount of time taken between each step of the transition
int imageHoldTime = 1000; //how long each bmp image is displayed

/*SD Pins:

CS: 9
MOSI: 11
MISO: 12
SCK: 13
*/

void setup() {
  
  FastLED.setBrightness(brightness);
  //set the pins for each row of LEDs
  FastLED.addLeds<WS2812B, 2, color_order>(rows[0], rowLength);
  FastLED.addLeds<WS2812B, 3, color_order>(rows[1], rowLength);
  FastLED.addLeds<WS2812B, 4, color_order>(rows[2], rowLength);
  FastLED.addLeds<WS2812B, 5, color_order>(rows[3], rowLength);
  FastLED.addLeds<WS2812B, 6, color_order>(rows[4], rowLength);
  FastLED.addLeds<WS2812B, 7, color_order>(rows[5], rowLength);
  FastLED.clear();

  RGBColor black;
  black.SetRGB(0,0,0);
  for (int y = 0; y < numRows; y++)
  {
    for (int x = 0; x < rowLength; x++)
    {
      SetColorAtCoords(x, y, black);
    }
  }
  
  Serial.begin(9600);
  //Serial.println("begin");

  if (!SD.begin(9)) {
    while (1);
  }

  //Serial.println("SD initialized");

  root = SD.open("/", FILE_READ);
  //printDirectory(root, 0);

  //Serial.println("root open");

  ImageTransition(SelectNextBMP());
}

void SignalBlink(int duration, int repetitions, int endDelay)
{
  for (int i = 0; i < repetitions; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(duration);
    digitalWrite(LED_BUILTIN, LOW);
  }
  delay(endDelay);
}

File SelectNextBMP()  {
  Serial.println(F("SelectNextBMP()"));
  //printDirectory(root, 0);
  File currentFile;
  while (true)
  {
    currentFile = root.openNextFile();
    if (!currentFile) //if there is no next file, go back to the beginning of the directory
    {
      Serial.println(F("no next file"));
      currentFile.close();
      root.rewindDirectory();
      continue;
    }
    String fileName = currentFile.name();
    if (!isBMP(fileName)) //if this file is not a BMP, try the next file
    {
      //Serial.print(fileName);
      //Serial.println(" is not a bmp file");
      currentFile.close();
      continue;
    }
    Serial.print(F("selected "));
    Serial.println(fileName);
    //return currentFile;
    break;
  }
  return currentFile;
}

bool isBMP(String fileName) {
  //return true if the last 4 characters of the file name are ".bmp" or ".BMP"
  return (fileName.lastIndexOf(".bmp") == fileName.length() - 4 || fileName.lastIndexOf(".BMP") == fileName.length() - 4);
}

void ImageTransition(File imageFile)  {
  Serial.println("ImageTransition()");
  //Serial.println(currentFile.name());

  //Set the current colors array
  RGBColor currentColors[numRows][rowLength];
  for (int y = 0; y < numRows; y++)
  {
    for (int x = 0; x < rowLength; x++)
    {
      currentColors[y][x].SetRGB(rows[y][x].r, rows[y][x].g, rows[y][x].b);
    }
  }

  //get the pixel size, which should be either 24 bits or 32 bits
  int pixelSize = readNbytesInt(&imageFile, 0x1C, 2);

  //find the start of the pixel data, skipping the header info
  int32_t pixelDataOffset = readNbytesInt(&imageFile, 0x0A,4);
  imageFile.seek(pixelDataOffset);
  //Set the target colors array
  RGBColor targetColors[numRows][rowLength];
  for (int y = 0; y < numRows; y++)
  {
    for (int x = 0; x < rowLength; x++)
    {
      //for each LED, get the cooresponding rgb values in the image. Alpha will be ignored, but it still must be read
      byte r, g, b, a;
      b = imageFile.read();
      g = imageFile.read();
      r = imageFile.read();
      switch (pixelSize) {
        case 32:
          a = imageFile.read();
          break;
        case 24:
          break;
        default:
          break;
      }
      targetColors[y][x].SetRGB(r, g, b);
    }
    switch (pixelSize) {
      case 32:
        imageFile.seek(imageFile.position() + 1);
        break;
      case 24:
        imageFile.seek(imageFile.position() + 2);
        break;
      default:
        break;
    }
  }
  //interpolate between the current rgb values and the target rgb values
  for (int i = 1; i <= transitionSteps; i++)
  {
    for (int y = 0; y < numRows; y++)
    {
      for (int x = 0; x < rowLength; x++)
      {
        RGBColor transColor = lerpColor(currentColors[y][x], targetColors[y][x], i/transitionSteps);
        SetColorAtCoords(x, y, transColor);
      }
    }
    delay(transitionStepDelay);
  }
  imageFile.close();
}

void SetColorAtCoords(int x, int y, RGBColor c) {
  rows[y][x] = CRGB(c.r, c.g, c.b);
  FastLED.show();
}

RGBColor lerpColor(RGBColor A, RGBColor B, float percent) {
  RGBColor color;
  byte r = lerpByte(A.r, B.r, percent);
  byte g = lerpByte(A.g, B.g, percent);
  byte b = lerpByte(A.b, B.b, percent);
  color.SetRGB(r, g, b);
  return color;
}

byte lerpByte(byte A, byte B, float percent)
{
  return A + ((B - A) * percent);
}

int32_t readNbytesInt(File *p_file, int position, byte nBytes)  {
    if (nBytes > 4)
        return 0;

    p_file->seek(position);

    int32_t weight = 1;
    int32_t result = 0;
    for (; nBytes; nBytes--)
    {
        result += weight * p_file->read();
        weight <<= 8;
    }
    return result;
}

void loop() {
  delay(imageHoldTime);
  ImageTransition(SelectNextBMP());
}
