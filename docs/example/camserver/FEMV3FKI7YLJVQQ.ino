#include <Adafruit_VC0706.h>
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <Ethernet.h>
#include <SoftwareSerial.h>         

// Arduino Ethernet shield: pin 4
#define chipSelect 4
// Using SoftwareSerial (Arduino 1.0+) or NewSoftSerial (Arduino 0023 & prior):
#if ARDUINO >= 100
// On Uno: camera TX connected to pin 2, camera RX to pin 3:
SoftwareSerial cameraconnection = SoftwareSerial(2, 3);
// On Mega: camera TX connected to pin 69 (A15), camera RX to pin 3:
//SoftwareSerial cameraconnection = SoftwareSerial(69, 3);
#else
NewSoftSerial cameraconnection = NewSoftSerial(2, 3);
#endif

Adafruit_VC0706 cam = Adafruit_VC0706(&cameraconnection);

/************ ETHERNET STUFF ************/
byte mac[] = {0x90,0xa2,0xda,0x0e,0xf7,0xfc};
byte ip[] = {192, 168, 0, 30};
char rootFileName[] = "index.htm"; 
EthernetServer server(80);

/************ SDCARD STUFF ************/
SdFat SD;
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

// store error strings in flash to save RAM
#define error(s) error_P(PSTR(s))
void error_P(const char* str) {
 PgmPrint("error: ");
 SerialPrintln_P(str);
 if (card.errorCode()) {
   PgmPrint("SD error: ");
   Serial.print(card.errorCode(), HEX);
   Serial.print(',');
   Serial.println(card.errorData(), HEX);
 }
 while(1);
}

/**********************SETUP()*********************/

void setup() {
 #if !defined(SOFTWARE_SPI)
 #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
   if(chipSelect != 53) pinMode(53, OUTPUT); // SS on Mega
 #else
   if(chipSelect != 10) pinMode(10, OUTPUT); // SS on Uno, etc.
 #endif
 #endif

 PgmPrint("Free RAM: ");
 Serial.println(FreeRam());  
 pinMode(10, OUTPUT);                       
 digitalWrite(10, HIGH);                    

 if (!card.init(SPI_HALF_SPEED, 4)) error("card.init failed!");
 if (!volume.init(&card)) error("vol.init failed!");

  PgmPrint("Volume is FAT");
  Serial.println(volume.fatType(),DEC);
  Serial.println();
 
  if (!root.openRoot(&volume)) error("openRoot failed");

  PgmPrintln("Files found in root:");
  root.ls(LS_DATE | LS_SIZE);
  Serial.println();
   
  PgmPrintln("Files found in all dirs:");
  root.ls(LS_R);
 
  Serial.println();
  PgmPrintln("Done");
  Serial.begin(9600);
  Serial.println("VC0706 Camera snapshot test");
  if (!SD.begin(chipSelect)) {
     Serial.println("Card failed, or not present");
     return;
   }  
   Ethernet.begin(mac, ip);
 server.begin();
}

#define BUFSIZ 100

/**********************LOOP()*********************/

void loop() {
  char clientline[BUFSIZ];
 char *filename;
 int index = 0;
 int image = 0;
 
 EthernetClient client = server.available();
 if (client) {
    boolean current_line_is_blank = true;
    index = 0;
  while (client.connected()) {
  if (client.available()) {
           char c = client.read();
   if (c != '\n' && c != '\r') {
           clientline[index] = c;
           index++;
           if (index >= BUFSIZ) 
             index = BUFSIZ -1;  
            continue;
   }
  clientline[index] = 0;
  filename = 0;
  Serial.println(clientline);
  if (strstr(clientline, "GET / ") != 0) { 
  filename = rootFileName;
         }
         if (strstr(clientline, "GET /") != 0) {        
            if (!filename) filename = clientline + 5; 
           (strstr(clientline, " HTTP"))[0] = 0; 
           Serial.println(filename);
           if (strstr(filename, "tttt") != 0)
              takingPicture();
          if (strstr(filename, "rrrr") != 0)
              removePicture();
                   if (! file.open(filename, O_READ)) {
    client.println("HTTP/1.1 404 Not Found");
              client.println("Content-Type: text/html");
              client.println();
              client.println("<h2>File Not Found!</h2>");
              break;
           }
         
            Serial.println("Open!");   
        client.println("HTTP/1.1 200 OK");
          if(strstr(filename, ".css") != 0)
               client.println("Content-Type: text/css");
         else  if (strstr(filename, ".htm") != 0)
               client.println("Content-Type: text/html");
          else if (strstr(filename, ".jpg") != 0)
               client.println("Content-Type: image/jpeg");                      
         client.println();
      
         int16_t c;
            while ((c = file.read()) >= 0) {
                Serial.print((char)c);
                client.print((char)c);
            }
          file.close(); 
         }else {
           // everything else is a 404
           client.println("HTTP/1.1 404 Not Found");
           client.println("Content-Type: text/html");
           client.println();
           client.println("<h2>File Not Found!</h2>");
           }
         break;
         }
       }
       delay(1000);
       client.stop();
    }
}

/**********************FUNCTION1()*********************/

void takingPicture(){
          if (cam.begin()) {
            Serial.println("Camera Found:");
          } else {
            Serial.println("No camera found?");
            return;
          }
          char *reply = cam.getVersion();
          if (reply == 0) {
            Serial.print("Failed to get version");
          } else {
            Serial.println("-----------------");
            Serial.print(reply);
   
            Serial.println("-----------------");
          }
          //cam.setImageSize(VC0706_640x480);        // biggest
          cam.setImageSize(VC0706_320x240);        // medium
          //cam.setImageSize(VC0706_160x120);          // small

          uint8_t imgsize = cam.getImageSize();
          Serial.print("Image size: ");
          if (imgsize == VC0706_640x480) Serial.println("640x480");
          if (imgsize == VC0706_320x240) Serial.println("320x240");
          if (imgsize == VC0706_160x120) Serial.println("160x120");

          Serial.println("Snap in 3 secs...");
          delay(3000);
            cam.takePicture();
            Serial.println("taking picture");
          
            file.open("IMAGE.jpg", O_RDWR | O_CREAT);
           
            uint16_t jpglen = cam.frameLength();  
            pinMode(8, OUTPUT);
            
            byte wCount = 0; 
            while (jpglen > 0) {
               uint8_t *buffer;
               uint8_t bytesToRead = min(32, jpglen); 
               buffer = cam.readPicture(bytesToRead);
              file.write(buffer, bytesToRead);
               if(++wCount >= 64) { 
                  Serial.print('.');
                  wCount = 0;
               }
              jpglen -= bytesToRead;
            }
            file.close();
            Serial.println("end of taking picture");
}
 /**********************FUNCTION2()*********************/
void removePicture(){
  file.open("IMAGE.jpg", O_READ | O_WRITE);
  file.remove();
}
