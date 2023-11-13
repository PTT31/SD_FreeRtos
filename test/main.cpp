// simple project using Arduino UNO and 128x64 OLED Display, 
// created by upir, 2022
// youtube channel: https://www.youtube.com/upir_upir

// FULL TUTORIAL part1: https://youtu.be/NPfaLKKsf_Q 
// FULL TOTORIAL part2: https://youtu.be/sFGsYZ0Hszk

// Useful links:
// Big Black knob: https://s.click.aliexpress.com/e/_Aq1wGF
// Huge Aluminium Knob: https://s.click.aliexpress.com/e/_A4GlET
// Arduino UNO: https://s.click.aliexpress.com/e/_AXDw1h
// Normal OLED Display: https://s.click.aliexpress.com/e/_AWSVnt
// Transparent OLED Display: https://s.click.aliexpress.com/e/_ABpnu7
// Big OLED Display: https://s.click.aliexpress.com/e/_ADL0T9
// Arduino breadboard prototyping shield: https://s.click.aliexpress.com/e/_ApbCwx
// u8g fonts (fonts available for u8g library): https://nodemcu-build.com/u8g-fonts.php
// u8g documentation: https://github.com/olikraus/u8glib/wiki/userreference
// Wokwi starting project: https://wokwi.com/projects/327642884399432274
// Photopea (online Photoshop-like tool): https://www.photopea.com/
// image2cpp (convert images into C code): https://javl.github.io/image2cpp/
// SSD1306 documentation: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf



#include "U8glib.h"

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0 | U8G_I2C_OPT_NO_ACK | U8G_I2C_OPT_FAST); // Fast I2C / TWI
//U8GLIB_SSD1306_128X64 u8g(13, 11, 8, 9, 10);   // SPI communication: SCL = 13, SDA = 11, RES = 10, DC = 9, CS = 8

int potentiometer_value = 0; // value from the potentiometer
char buffer[20];       // helper buffer for converting values into C-style string (array of chars)
int string_width;      // helper value for string widths

float pixel_x = 0;     // x pos for pixel
float pixel_y = 0;     // y pos for pixel
float line_x = 0;      // x pos for line end
float line_y = 0;      // y pos for line end
float text_x = 0;      // x pos for text
float text_y = 0;      // y pos for text

int center_x = 64;     // x center of the knob 
int center_y = 108;    // y center of the knob (outside of the screen)
int radius_pixel = 92; // radius for pixel tickmarks
int radius_line = 87;  // radius for line end
int radius_text = 75;  // radius for text

int angle;             // angle for the individual tickmarks
int tick_value;        // numeric value for the individual tickmarks

byte precalculated_x_radius_pixel[180]; // lookup table to prevent expensive sin/cos calculations
byte precalculated_y_radius_pixel[180]; // lookup table to prevent expensive sin/cos calculations

unsigned long millis_time;       // fps
unsigned long millis_time_last;  // fps
int fps;                         // actual FPS value


const uint8_t upir_logo[] U8G_PROGMEM = {
B00010101, B11010111,     //  ░░░█░█░███░█░███
B00010101, B01000101,     //  ░░░█░█░█░█░░░█░█
B00010101, B10010110,     //  ░░░█░█░██░░█░██░
B00011001, B00010101      //  ░░░██░░█░░░█░█░█
};

int tick_pixel_array[50][2];    // pixels to be drawn, x,y
int tick_line_array[10][4];     // lines to be drawn, x,y,x,y
int tick_text_array[10][3];     // labels to be drawn, x,y,value

int tick_pixel_count;           // number of pixels to be drawn
int tick_line_count;            // number of lines to be drawn
int tick_text_count;            // number of labels to be drawn

int current_u8g_page;  // current page of the u8g drawing loop - there are 8 pages for 128x64 display


void setup() {
  
  u8g.setColorIndex(1);          // set color to white

  for (int i = 0; i < 180; i++) {    // pre-calculate x and y positions into the look-up tables
     precalculated_x_radius_pixel[i] =  sin(radians(i-90)) * radius_pixel + center_x; 
     precalculated_y_radius_pixel[i] = -cos(radians(i-90)) * radius_pixel + center_y;      
  }
}

void loop() {

    tick_pixel_count=0;   // reset number of pixels
    tick_line_count=0;    // reset number of lines
    tick_text_count=0;    // reset number of labels

    // calculate tickmarks
    for (int i=-48; i<=48; i=i+3) {                                // only try to calculate tickmarks that would end up be displayed
      angle = i + ((potentiometer_value*3)/10) % 3;                // final angle for the tickmark
      tick_value = round((potentiometer_value/10.0) + angle/3.0);  // get number value for each tickmark

      //pixel_x =  sin(radians(angle)) * radius_pixel + center_x;    // calculate the tickmark pixel x value
      //pixel_y = -cos(radians(angle)) * radius_pixel + center_y;    // calculate the tickmark pixel y value
      pixel_x = precalculated_x_radius_pixel[angle+90];              // get x value from lookup table
      pixel_y = precalculated_y_radius_pixel[angle+90];              // get y value from lookup table

      if (pixel_x > 0 && pixel_x < 128 && pixel_y > 0 && pixel_y < 64) {  // only draw inside of the screen

        if(tick_value >= 0 && tick_value <= 100) {  // only draw tickmarks between values 0-100%, could be removed when using rotary controller

          if (tick_value % 10 == 0) {                                // draw big tickmark == lines + text
            line_x =  sin(radians(angle)) * radius_line + center_x;  // calculate x pos for the line end
            line_y = -cos(radians(angle)) * radius_line + center_y;  // calculate y pos for the line end
            //u8g.drawLine(pixel_x, pixel_y, line_x, line_y);          // NOT draw the line
            tick_line_array[tick_line_count][0] = line_x;              // but instead...
            tick_line_array[tick_line_count][1] = line_y;              // store those values...
            tick_line_array[tick_line_count][2] = pixel_x;             // inside this...
            tick_line_array[tick_line_count][3] = pixel_y;             // array
            tick_line_count++;                                         // and increment the counter


            text_x =  sin(radians(angle)) * radius_text + center_x;  // calculate x pos for the text
            text_y = -cos(radians(angle)) * radius_text + center_y;  // calculate y pos for the text 
            //itoa(tick_value, buffer, 10);                            // convert integer to string
            //string_width = u8g.getStrWidth(buffer);                  // get string width
            //u8g.drawStr(text_x - string_width/2, text_y, buffer);    // NOT draw text - tickmark value
            tick_text_array[tick_text_count][0] = text_x;              // but instead, store the values...
            tick_text_array[tick_text_count][1] = text_y;              // inside this...
            tick_text_array[tick_text_count][2] = tick_value;          // array
            tick_text_count++;                                         // and increment the counter            
            
          } 
          else {                                                     // draw small tickmark == pixel tickmark
            //u8g.drawPixel(pixel_x, pixel_y);                       // NOT draw a single pixel
            tick_pixel_array[tick_pixel_count][0] = pixel_x;         // but instead, store the values...
            tick_pixel_array[tick_pixel_count][1] = pixel_y;         // inside this array
            tick_pixel_count++;                                      // and increment the counter        
          }      
  
        }
      }
    }



  u8g.firstPage();                // required for u8g library
  current_u8g_page=0;             // reset the u8g page number
  do {                            // 

    

    u8g.setColorIndex(1);          // set color to white
    u8g.setFont(u8g_font_6x10r);   // set smaller font for tickmarks   
 
    if (current_u8g_page>=2 && current_u8g_page<=4) {
      // draw pixels
      for (int i=0; i<tick_pixel_count; i++) { 
          u8g.drawPixel(tick_pixel_array[i][0], tick_pixel_array[i][1]);      // draw the pixel
      } 
    }

    if (current_u8g_page>=2 && current_u8g_page<=5) {
      // draw lines
      for (int i=0; i<tick_line_count; i++) { 
          u8g.drawLine(tick_line_array[i][0], tick_line_array[i][1],tick_line_array[i][2],tick_line_array[i][3]);          // draw the line
      } 
    }   

    if (current_u8g_page>=2 && current_u8g_page<=6) {
      // draw labels
      for (int i=0; i<tick_text_count; i++) { 
        itoa(tick_text_array[i][2] , buffer, 10);                // convert integer to string
        string_width = u8g.getStrWidth(buffer);                  // get string width
        u8g.drawStr(tick_text_array[i][0] - string_width/2, tick_text_array[i][1], buffer);    // draw text - tickmark value
      } 
    } 



 
    if (current_u8g_page<2) {
      // draw the big value on top
      u8g.setFont(u8g_font_8x13r);                      // set bigger font
      dtostrf(potentiometer_value/10.0, 1, 1, buffer);  // float to string, -- value, min. width, digits after decimal, buffer to store
      sprintf(buffer, "%s%s", buffer, "%");             // add some random ending character

      string_width = u8g.getStrWidth(buffer);           // calculate string width

      u8g.setColorIndex(1);                             // set color to white
      u8g.drawRBox(64-(string_width+4)/2, 0, string_width+4, 11, 2);  // draw background rounded rectangle
      u8g.drawTriangle( 64-3, 11,   64+4, 11,   64, 15);              // draw small arrow below the rectangle
      u8g.setColorIndex(0);                                           // set color to black 
      u8g.drawStr(64-string_width/2, 10, buffer);                     // draw the value on top of the display
    }

    if (current_u8g_page==0) {
      // draw upir logo
      u8g.setColorIndex(1);
      u8g.drawBitmapP(112, 0, 2, 4, upir_logo);  

      // display FPS, could be commented out for the final product
      u8g.setColorIndex(1);                                           // set color to white   
      u8g.setFont(u8g_font_5x7r);                                     // set very small font
      itoa(fps, buffer, 10);                                          // convert FPS number to string
      u8g.drawStr(0,8,buffer);                                       // draw the FPS number

    }

    current_u8g_page++;    // increment the current number counter

  } while ( u8g.nextPage() );    // required for u8g library


  potentiometer_value = map(analogRead(A0), 0, 1023, 1000, 0);     // read the potentiometer value, remap it to 0-1000

  millis_time_last = millis_time;                                  // store last millisecond value
  millis_time = millis();                                          // get millisecond value from the start of the program
  fps = round(1000.0/ (millis_time*1.0-millis_time_last));         // calculate FPS (frames per second) value


}
