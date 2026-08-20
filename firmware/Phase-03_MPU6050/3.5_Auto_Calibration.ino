/*
 * Project : Smart Safety Vest
 * Phase   : Phase 03.5 - Automatic Reference Calibration
 * File    : Phase03_Auto_Calibration.ino
 * Author  : Ajoy Das Team
 */
#include <Wire.h>
#include <MPU6050.h>
#include <math.h>
MPU6050 mpu; int16_t ax,ay,az,gx,gy,gz; 
const int FILTER_SIZE=10,CAL_SAMPLES=100; 
long axBuf[FILTER_SIZE]={0},ayBuf[FILTER_SIZE]={0},azBuf[FILTER_SIZE]={0}; 
int idx=0; float axF,ayF,azF,pitch,roll,refPitch,refRoll,tiltPitch,tiltRoll; 

void updateFilter()
{mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz); 
axBuf[idx]=ax; 
ayBuf[idx]=ay; 
azBuf[idx]=az; 
idx=(idx+1)%FILTER_SIZE; long sx=0,sy=0,sz=0; 

for(int i=0;i<FILTER_SIZE;i++)
  {sx+=axBuf[i];
sy+=ayBuf[i];sz+=azBuf[i];} axF=sx/(float)FILTER_SIZE; 

ayF=sy/(float)FILTER_SIZE; 
azF=sz/(float)FILTER_SIZE; 

pitch=atan2(axF,sqrt(ayF*ayF+azF*azF))*180.0/PI; 
roll=atan2(ayF,sqrt(axF*axF+azF*azF))*180.0/PI;} 

void calibrate()
{Serial.println("Automatic Calibration"); 
for(int i=5;i>0;i--)
  {Serial.println(i);
  delay(1000);
  
  } 
  
float ps=0,rs=0; 
for(int i=0;i<CAL_SAMPLES;i++){updateFilter(); 
ps+=pitch; rs+=roll; 
delay(20);
} 

refPitch=ps/CAL_SAMPLES; 
refRoll=rs/CAL_SAMPLES;

Serial.print("Reference Pitch: ");
Serial.println(refPitch,2); 
Serial.print("Reference Roll: ");
Serial.println(refRoll,2);

} 

void setup(){Serial.begin(115200); 
Wire.begin(21,22); mpu.initialize(); 
calibrate();

} 
void loop(){updateFilter(); 
tiltPitch=pitch-refPitch; 
tiltRoll=roll-refRoll; 
Serial.print("TiltPitch: ");Serial.print(tiltPitch,2); Serial.print(" TiltRoll: ");Serial.println(tiltRoll,2); delay(200);}