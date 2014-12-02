/***************************************************************************************
 
#include <xbee.h>
#include <Servo.h> 
#include <SPI.h>
#include <LiquidCrystal.h>

const int HLD_001 = 0;   //DO0(HOLD)
const int HLD_002 = 1;   //DO1(HOLD)
const int LCD_DB7 = 2;   //DO2(LCD用)
const int LCD_DB6 = 3;   //DO3(LCD用)
const int LCD_DB5 = 6;   //DO6(LCD用)
const int LCD_DB4 = 5;   //DO5(LCD用)
const int LCD_EBL = 7;   //DO7(LCD用)
const int LCD_RS_ = 8;   //DO8(LCD用)

// SERVO
const int SV_PAN = 9;
const int SV_TIT = 10;

// Must call this at least once every 50ms
Servo TiltServo; // Up & Down
Servo PanServo;  // Left & Right
byte  minAngle;
byte  maxAngle;

// Servo Adjustment
#define NEUTRAL 90
byte correction = 25; // degree
byte adjustment = 2;

// Data Frame Paramater
#define STX     0x02
#define ETX     0x03

struct PACKET_INFO {
  byte Stx;
  byte Cmd01;
  byte Pan;
  byte Cmd02;
  byte Tilt;
  byte Ext;
};
PACKET_INFO  Packet;

//LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
LiquidCrystal lcd(LCD_RS_, LCD_EBL, LCD_DB4, LCD_DB5, LCD_DB6, LCD_DB7);

void setup(){
  
    // Servo Setting
    LCDWritter("STATUS:","INITIALIZE SERVO...");
    InitializeServo();  
  
    // Initializeing the LCD
    lcd.begin(16, 2);                               // LCDの初期化
    lcd.clear(); lcd.print("SERVO CONTROLLER");
    xbee_init( 0x00 );                              // XBeeの初期化
    xbee_atcb( 4 );                                 // ネットワーク初期化
    xbee_atnj(0);                                   // 孫機からのジョインを制限
    lcd.setCursor(0, 1);                            // 液晶の２行目に移動
    lcd.print("COMMISSIONING...");                  // 文字を表示
    while( xbee_atai() > 0x01 ){                    // ネットワーク参加状況を確認
        delay(3000);
        xbee_atcb(1);                               // ネットワーク参加ボタンを押下
    }
    lcd.clear();
} 

void loop(){
    byte buffer[6];
    byte length = 0;
    XBEE_RESULT xbee_result;                        // 受信データ用
    
    memset(&buffer, 0, sizeof(PACKET_INFO));
    byte data = xbee_rx_call( &xbee_result );
    if( xbee_result.MODE == MODE_UART){             // UARTシリアルデータを受信
        for(int i = 0; i < data; i++){
          
            if (xbee_result.DATA[i] == STX) {
                length = 0;
            }
            
            buffer[length] = xbee_result.DATA[i];
            //LCDWritter("Received:"+String(length), String(buffer[length]));
            delay(9);
            length++;
            
            if( length >= 6 ) {
                if ( buffer[0] == STX && buffer[5] == ETX ){
                    memcpy(&Packet, &buffer, sizeof(PACKET_INFO));
                    executeCommand();
                    //LCDWritter("Execute:", String("Command"));
                }
                length = 0;
                memset(&buffer, 0, sizeof(PACKET_INFO));
            }
        }
    } else if( xbee_result.MODE ){                  // その他の受信時
        lcd.clear();
        lcd.print( "MODE="); lcd.print(xbee_result.MODE,  HEX);
        lcd.print(" STAT="); lcd.print(xbee_result.STATUS,HEX);
        lcd.setCursor(0,1);
        for(int i = 0; i < 8; i++){                           // 送信元のアドレスを表示
            lcd.print(xbee_result.FROM[i]>>4, HEX);
            lcd.print(xbee_result.FROM[i]%16, HEX);
        }
    }
}

void InitializeServo()
{ 
    // Attach Serve to the Arduino Pin
    minAngle = 0   + correction;
    maxAngle = 180 - correction; 
  
    PanServo.attach(SV_PAN);      // Right and Left
    TiltServo.attach(SV_TIT);    // Up and down
  
    TiltServo.write(NEUTRAL);
    delay(1000);
    PanServo.write(NEUTRAL);
    delay(1000);
}

void LCDWritter(String title, String value){
    // set the cursor to column 0, line 1
    lcd.begin(16, 2);
    lcd.print(title);
    lcd.setCursor(0, 1);
    lcd.print(value);
}

void executeCommand()
{
    PanServo.write(AngleAdjustment(Packet.Pan));
    byte angle = AngleAdjustment(Packet.Tilt);
    TiltServo.write((angle + 90)/adjustment);
}

byte AngleAdjustment(byte ang)
{
  byte angle = 0;
  if (ang < minAngle){
    angle = minAngle; 
  }else if (ang > maxAngle){
    angle = maxAngle;
  }else{
    angle = ang;
  }
  return angle;
}

