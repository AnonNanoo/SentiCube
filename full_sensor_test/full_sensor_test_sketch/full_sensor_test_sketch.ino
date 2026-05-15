#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "driver/i2s.h"
#include <Adafruit_AHTX0.h>
#include <Adafruit_VL53L0X.h>

//   PINS  
#define SDA_PIN 21
#define SCL_PIN 22

#define SD_CS 13

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32

#define MPU_ADDR 0x68
#define QMC_ADDR 0x2C   // change to 0x0D if scan shows it

//   FORWARD DECLARATIONS  
void initI2S();
void readMPU();
void readMAG();
void readAHT();
void readTOF();
void readMIC();
void logSD();
bool initQMC();
void calibrateMPU();

//   SYSTEM  
WebServer server(80);
Adafruit_AHTX0 aht;
Adafruit_VL53L0X lox;

//   STATUS  
bool mpuOK=false, ahtOK=false, tofOK=false, qmcOK=false, sdOK=false;

//   DATA  
float temp=0, hum=0, distance=0;
float ax=0, ay=0, az=0;
float gx=0, gy=0, gz=0;
float ax0=0, ay0=0, az0=0;
float gx0=0, gy0=0, gz0=0;
float heading=0;

String rmsValue="--";
String peakValue="--";

//   UI  
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SentiCube</title>

<style>
body{margin:0;font-family:Arial;background:#0b0f14;color:#fff;}
h1{text-align:center;padding:10px;}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;padding:10px;}
.card{background:rgba(255,255,255,0.07);padding:10px;border-radius:12px;}
.title{font-size:12px;opacity:0.6;}
.value{font-size:16px;margin-top:4px;}
</style>
</head>

<body>
<h1>SentiCube</h1>

<div class="grid">
<div class="card"><div class="title">TEMP/HUM</div><div class="value" id="env"></div></div>
<div class="card"><div class="title">DIST</div><div class="value" id="dist"></div></div>
<div class="card"><div class="title">IMU</div><div class="value" id="imu"></div></div>
<div class="card"><div class="title">HEADING</div><div class="value" id="mag"></div></div>
<div class="card"><div class="title">RMS</div><div class="value" id="rms"></div></div>
<div class="card"><div class="title">PEAK</div><div class="value" id="peak"></div></div>
</div>

<script>
async function u(){
 let r=await fetch('/data');
 let d=await r.json();

 document.getElementById('env').innerText=
 d.temperature+"C / "+d.humidity+"%";

 document.getElementById('dist').innerText=
 d.distance+" mm";

 document.getElementById('imu').innerText=
 "AX:"+d.ax+" AY:"+d.ay+" AZ:"+d.az;

 document.getElementById('mag').innerText=
 d.heading+"°";

 document.getElementById('rms').innerText=
 d.rms;

 document.getElementById('peak').innerText=
 d.peak;
}
setInterval(u,400);
u();
</script>
</body>
</html>
)rawliteral";

//   I2S (FIXED)  
void initI2S(){
  i2s_config_t cfg={
    .mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX),
    .sample_rate=16000,
    .bits_per_sample=I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format=I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format=I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags=0,
    .dma_buf_count=4,
    .dma_buf_len=256,
    .use_apll=false,
    .tx_desc_auto_clear=false,
    .fixed_mclk=0
  };

  i2s_pin_config_t pin={
    .bck_io_num=I2S_SCK,
    .ws_io_num=I2S_WS,
    .data_out_num=-1,
    .data_in_num=I2S_SD
  };

  i2s_driver_install(I2S_NUM_0,&cfg,0,NULL);
  i2s_set_pin(I2S_NUM_0,&pin);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

//   QMC  
bool initQMC(){
  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x0B);
  Wire.write(0x01);
  if(Wire.endTransmission()!=0) return false;

  delay(20);

  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x09);
  Wire.write(0x1D);
  if(Wire.endTransmission()!=0) return false;

  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x0A);
  Wire.write(0x00);
  Wire.endTransmission();

  return true;
}

//   MPU CAL  
void calibrateMPU(){
  Serial.println("MPU CALIBRATING...");

  long axS=0,ayS=0,azS=0,gxS=0,gyS=0,gzS=0;
  uint8_t d[14];

  for(int i=0;i<200;i++){
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR,14);

    for(int j=0;j<14;j++) d[j]=Wire.read();

    axS+=(int16_t)(d[0]<<8|d[1]);
    ayS+=(int16_t)(d[2]<<8|d[3]);
    azS+=(int16_t)(d[4]<<8|d[5]);

    gxS+=(int16_t)(d[8]<<8|d[9]);
    gyS+=(int16_t)(d[10]<<8|d[11]);
    gzS+=(int16_t)(d[12]<<8|d[13]);

    delay(5);
  }

  ax0=axS/200.0/16384.0;
  ay0=ayS/200.0/16384.0;
  az0=azS/200.0/16384.0;

  gx0=gxS/200.0/131.0;
  gy0=gyS/200.0/131.0;
  gz0=gzS/200.0/131.0;
}

//   READ MPU  
void readMPU(){
  if(!mpuOK) return;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR,14);

  uint8_t d[14];
  for(int i=0;i<14;i++) d[i]=Wire.read();

  ax=(int16_t)(d[0]<<8|d[1])/16384.0 - ax0;
  ay=(int16_t)(d[2]<<8|d[3])/16384.0 - ay0;
  az=(int16_t)(d[4]<<8|d[5])/16384.0 - az0;

  gx=(int16_t)(d[8]<<8|d[9])/131.0 - gx0;
  gy=(int16_t)(d[10]<<8|d[11])/131.0 - gy0;
  gz=(int16_t)(d[12]<<8|d[13])/131.0 - gz0;
}

//   MAG  
void readMAG(){
  if(!qmcOK) return;

  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);

  if(Wire.requestFrom(QMC_ADDR,6)!=6){
    heading=-1;
    return;
  }

  int16_t x=Wire.read()|(Wire.read()<<8);
  int16_t y=Wire.read()|(Wire.read()<<8);

  if(x==0&&y==0){
    heading=-1;
    return;
  }

  float h=atan2((float)y,(float)x)*180/PI;
  if(h<0)h+=360;

  heading=h;
}

//   AHT  
void readAHT(){
  if(!ahtOK) return;
  sensors_event_t h,t;
  aht.getEvent(&h,&t);
  temp=t.temperature;
  hum=h.relative_humidity;
}

//   TOF  
void readTOF(){
  if(!tofOK) return;
  VL53L0X_RangingMeasurementData_t m;
  lox.rangingTest(&m,false);
  if(m.RangeStatus!=4) distance=m.RangeMilliMeter;
}

//   MIC  
void readMIC(){
  const int N=128;
  int32_t b[N];
  size_t r;

  i2s_read(I2S_NUM_0,b,sizeof(b),&r,portMAX_DELAY);
  if(r==0) return;

  int c=r/4;
  long long sum=0;
  int32_t peak=0;

  for(int i=0;i<c;i++){
    int32_t s=b[i]>>8;
    if(s<0)s=-s;
    if(s>peak)peak=s;
    sum+=s*s;
  }

  float rms=sqrt((float)sum/c);

  rmsValue=String(rms,1);
  peakValue=String(peak);
}

//   SD  
void logSD(){
  if(!sdOK) return;

  File f=SD.open("/log.json",FILE_APPEND);
  if(!f) return;

  f.println("{\"t\":"+String(temp)+
  ",\"h\":"+String(hum)+
  ",\"d\":"+String(distance)+
  ",\"hdeg\":"+String(heading)+
  ",\"rms\":"+rmsValue+
  ",\"peak\":"+peakValue+"}");

  f.close();
}

//   WEB  
void handleRoot(){server.send_P(200,"text/html",index_html);}

void handleData(){
  String j="{";
  j+="\"temperature\":"+String(temp)+",";
  j+="\"humidity\":"+String(hum)+",";
  j+="\"distance\":"+String(distance)+",";
  j+="\"ax\":"+String(ax)+",";
  j+="\"ay\":"+String(ay)+",";
  j+="\"az\":"+String(az)+",";
  j+="\"heading\":"+String(heading)+",";
  j+="\"rms\":"+rmsValue+",";
  j+="\"peak\":"+peakValue;
  j+="}";
  server.send(200,"application/json",j);
}

//   SETUP  
void setup(){
  Serial.begin(115200);
  Wire.begin(SDA_PIN,SCL_PIN);

  SPI.begin(18,19,23,SD_CS);
  sdOK=SD.begin(SD_CS);

  WiFiManager wm;
  wm.autoConnect("SentiCube");

  mpuOK=true;
  ahtOK=aht.begin(&Wire);
  tofOK=lox.begin();
  qmcOK=initQMC();

  initI2S();
  calibrateMPU();

  server.on("/",handleRoot);
  server.on("/data",handleData);
  server.begin();
}

// LOOP
void loop(){
  server.handleClient();

  static unsigned long t=0;
  if(millis()-t>200){
    t=millis();
    readMPU();
    readAHT();
    readTOF();
    readMAG();
    readMIC();
  }

  static unsigned long l=0;
  if(millis()-l>2000){
    l=millis();
    logSD();
  }
}