#include "sd_.h"            // Thư viện để làm việc với thẻ SD
#include "lcd.h"
#include "RTClib.h"         // Thư viện để làm việc với module RTC DS1307
#include <Adafruit_Fingerprint.h>  // Thư viện để làm việc với cảm biến vân tay
#include <WiFi.h>           // Thư viện WiFi
#include <NTPClient.h>      // Thư viện NTPClient cho đồng bộ thời gian
#include <DNSServer.h>      // Thư viện DNSServer để xử lý DNS trên AP
#include <AsyncTCP.h>       // Thư viện AsyncTCP cho ESPAsyncWebServer
#include "ESPAsyncWebServer.h"  // Thư viện ESPAsyncWebServer
#define MAX_LINE_LENGTH (64)  // Định nghĩa độ dài tối đa của một dòng trong file
#define mySerial Serial2     // Sử dụng Serial2 cho cảm biến vân tay

RTC_DS1307 rtc;  // Khai báo đối tượng RTC

SemaphoreHandle_t spiMutex;  // Semaphore để quản lý truy cập SPI

QueueHandle_t QueueHandle;  // Hàng đợi để truyền ID từ TaskFinger đến TaskSQL
const int QueueElementSize = 10;  // Số lượng phần tử tối đa trong hàng đợi
TaskHandle_t taskfinger;  // Handle của TaskFinger
TaskHandle_t tasklcd;     // Handle của TaskLCD
TaskHandle_t tasksql;     // Handle của TaskSQL
TaskHandle_t taskitn;     // Handle của TaskInternet

enum FingerMode {
    Scan_finger,
    Insert_finger,
    Correct_finger,
    Incorrect_finger
};

class message_lcd {
public:
    int mode = Scan_finger;
    char ip[20] = "Conect Wifi...";
    char noti[20];
};


User_if user;    // Đối tượng để chứa Thông tin ngươi quét vân tay
message_lcd message;  // Đối tượng để chứa thông điệp cho TaskLCD
void TaskFinger(void *pvParameters);  // Hàm của TaskFinger
DateTime TaskTime();  // Hàm để lấy thời gian từ module DS1307
void TaskLCD(void *pvParameters);  // Hàm của TaskLCD
void TaskSQL(void *pvParameters);  // Hàm của TaskSQL
void TaskInternet(void *pvParameters);  // Hàm của TaskInternet
int Finger_s();  // Hàm để đọc ID từ cảm biến vân tay
void Record();  // Hàm để ghi dữ liệu
// In Thoi Gian
void drawTime(U8G2_ST7567_JLX12864_F_4W_HW_SPI u8g2);  // Hàm để vẽ thời gian lên màn hình
// Ve File
void drawFile(u8g2_int_t x, u8g2_int_t y, const char *filename, U8G2_ST7567_JLX12864_F_4W_HW_SPI u8g2);  // Hàm để vẽ hình ảnh từ file lên màn hìn
void record(int id);  // Hàm để ghi dữ liệu vào cơ sở dữ liệu
int Finger_s(Adafruit_Fingerprint finger);  // Hàm để đọc ID từ cảm biến vân tay

// In Thoi Gian
void drawTime(U8G2_ST7567_JLX12864_F_4W_HW_SPI u8g2)
{
    DateTime now;
    u8g2.setCursor(0, 12);
    now = rtc.now();
    String dateTimeString = String(now.year(), DEC) + '/' +
                            String(now.month(), DEC) + '/' +
                            String(now.day(), DEC) + ' ' +
                            String(now.hour(), DEC) + ':' +
                            String(now.minute(), DEC) + ':' +
                            String(now.second(), DEC);

    u8g2.print(dateTimeString);
};
void setup()
{
    //Ham cấu hình
    Serial.begin(115200);
    while (!Serial)
    {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    SPI.begin();
    if (!SD.begin(5))
    {
        Serial.println("initialization failed!");
        while (1)
            ;
    }
    sqlite3_initialize();
    Serial.println("initialization done.");
    Serial.println("u8g2 Start");
    vTaskDelay(5);
    Wire.setPins(16, 17);
    if (!rtc.begin())
    {
        Serial.println("Couldn't find RTC");
        Serial.flush();
    }
    spiMutex = xSemaphoreCreateBinary();
    xSemaphoreGive(spiMutex);
    Serial.print("Semaphore Start");
    QueueHandle = xQueueCreate(QueueElementSize, sizeof(int));
    // Check if the queue was successfully created
    if (QueueHandle == NULL)
    {
        Serial.println("Queue could not be created. Halt.");
        while (1)
            delay(1000);
    }
    Serial.print("QueueStart");
    xTaskCreatePinnedToCore(TaskSQL, "TaskSQL", 73000, NULL, 4, &tasksql, 1);
    xTaskCreatePinnedToCore(TaskLCD, "TaskLCD", 8192, NULL, 5, &tasklcd, 1);
    xTaskCreatePinnedToCore(TaskFinger, "FindFinger", 2048, NULL, 5, &taskfinger, 1);
    xTaskCreatePinnedToCore(TaskInternet, "TaskInternet", 8192, NULL, 3, &taskitn, 0);
}

void loop()
{
    vTaskSuspend(NULL);
}
void record(int id)
{
}

int Finger_s(Adafruit_Fingerprint finger)
{
    //test nên viết ntn nào dùng cảm biến vân tay thì thay code
    if (Serial.available() > 0)
    {
        // Đọc số nguyên từ cổng Serial
        return Serial.parseInt();
    }
    return finger.fingerID;
}
void TaskFinger(void *pvParameters)
{
    Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
    finger.begin(57600);
    if (finger.verifyPassword())
    {
        Serial.println("Found fingerprint sensor!");
    }
    int finger_id;
    while (1)
    {
        finger_id = Finger_s(finger);
        if (finger_id != 0 && QueueHandle != NULL)
        { // Sanity check just to make sure the queue actually exists
            int ret = xQueueSend(QueueHandle, (void *)&finger_id, 0);
            if (ret == errQUEUE_FULL)
            {
                Serial.println("The `TaskReadFromSerial` was unable to send data into the Queue");
            } // Queue send check
        }
        // Serial.print("Found ID #");
        // Serial.print(user.id);
        // Serial.print(" with confidence of ");
        vTaskDelay(pdMS_TO_TICKS(500)); // Đợi 1 giây trước khi lặp lại nhiệm vụ
    }
}
void TaskLCD(void *pvParameters)
{

    LCD u8g2;
    u8g2.begin();
    u8g2.setFont(u8g2_font_helvR10_tr);
    u8g2.setContrast(30);
    int current_u8g_page;
    while (1)
    {

        xSemaphoreTake(spiMutex, portMAX_DELAY);
        u8g2.clearDisplay();
        u8g2.firstPage();
        do
        {
            // if (current_u8g_page == 0)
            // {
            //     drawTime(u8g2);
            // };
            drawTime(u8g2);
            switch (message.mode)
            {
            case Scan_finger:
                u8g2.drawFile(0, 40, "/bin/u8g2.bin");
                break;
            case Correct_finger:
                Serial.println(message.noti);
                u8g2.setCursor(0, 40); // Đặt vị trí để in tên
                u8g2.print(message.noti); // In tên lên màn hình
                u8g2.drawFile(0, 40, "/bin/Correct_finger.bin");
                vTaskDelay(pdMS_TO_TICKS(2000));
                memset(message.noti, 0, 20);
                message.mode = Scan_finger;
                break;
            case Insert_finger:
                u8g2.drawFile(0, 40, "/bin/Insert_finger.bin");
                break;
            case Incorrect_finger:
                u8g2.drawFile(0, 40, "/bin/Incorrect_finger.bin");
                break;
            default:
                break;
            }
        current_u8g_page++;
        } while (u8g2.nextPage());
        current_u8g_page = 0;
        Serial.println(xPortGetFreeHeapSize());
        xSemaphoreGive(spiMutex);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Đợi 2 giây trước khi lặp lại nhiệm vụ
    }
}
void TaskSQL(void *pvParameters)
{
    User_if user;
    int finger_id;
    while (1)
    {
        int ret = xQueueReceive(QueueHandle, &finger_id, portMAX_DELAY);
        if (ret == pdPASS)
        {
            Serial.printf("ID find %d:\"\n", finger_id);
            xSemaphoreTake(spiMutex, portMAX_DELAY);
            db_query(finger_id, &user);
            if(user.name != NULL){

                strcpy(message.noti, user.name);
                message.mode = Correct_finger;
                Serial.printf("Name: %s, Finger_id: %d\n", user.name, user.finger_id);
            }
            xSemaphoreGive(spiMutex);
            
        }
        else if (ret == pdFALSE)
        {
            Serial.println("The `TaskWriteToSerial` was unable to receive data from the Queue");
        }
    }
}
const char *ssid = "A";
const char *password = "tung3108";
AsyncWebServer server(80);
DNSServer dnsServer;
void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void TaskInternet(void *pvParameters)
{
    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    for (int count; count < 20 && WiFi.status() != WL_CONNECTED; count++)
    {
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("Wifi STA");

        WiFi.softAP("esp-captive");
    }
    else
    {
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        timeClient.begin();
        timeClient.update();
        rtc.adjust(DateTime(timeClient.getEpochTime()));
        // server.on("/", handleRoot);
        // server.on("/login", handleLogin);
        // server.on("/wifi", handleWifi);
        // server.on("/save", handleSave);

        server.begin();
    }
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}