#include <stdint.h>
#include <stdlib.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include "vbus.h"
#include "TimerOne.h"

SoftwareSerial sbrio_out(5,6); //RX, TX
SoftwareSerial vbus_in(10,11); //RX, TX

int b1 = 0;
int b2 = 0;
boolean blinker=false;
uint8_t buf[15];
uint8_t c;
uint16_t temp;
uint16_t temp1;
uint16_t temp2;
volatile t_vbus_outdata vbus_outdatanew;

volatile t_vbus_head vbus_head;
t_vbus10_frame vbus_frame;

//Pruefsumme eines Pakets berechnen
uint8_t VBus_CalcCrc(const unsigned char *Buffer, uint8_t Offset, uint8_t len) {
  uint8_t crc = VBUS_CRC_MAGIC;
  while (len) {
    len--;
    crc -= Buffer[Offset++];
    crc &= VBUS_CRC_MAGIC;
  }
  return crc;
}

void Vbus_ProcessChar(uint8_t c) {
  if (vbus_head.update == 0) return;
  if (c == VBUS_SYNC) {
    //Wir haben Sync -> alle folgenden Daten werden decodiert
    vbus_head.sync = 1;
    //Offset im Head setzen
    vbus_head.offset = 1;
    //Das naechste empfangene Frame hat den Index 0
    vbus_head.currframe = 0;
    /* Aktuelle Daten als ungültig markieren
     * Geht theoretisch auch später, an dieser Stelle können aber CPU-Zyklen gespart werden ;)
     */
    vbus_head.valid = 0;

    //Serial.print("Sync ");

    //Wenn das Sync-Wort empfangen wurde, gibt es für das aktuelle Byte nichts mehr zu tun
    return;
  }

  //Daten nur erfassen/verarbeiten, wenn vorher ein sync erkannt wurde
  if (vbus_head.sync) {


    //Paket-Head in die vorgesehene Variable schreiben
    if (vbus_head.offset < VBUS_HEADER_LENGTH) {
      uint8_t *headptr = (uint8_t *)&vbus_head;
      headptr += vbus_head.offset - 1;
      *headptr = c;
    }

    if (vbus_head.offset == (VBUS_HEADER_LENGTH - 1)) {

      /* Nach dem 10. Byte (inkl. Startwort) ist der Header komplett
       * Jetzt kann er geprüft und ausgewertet werden
       */
      //VBUS_DEBUG_BREAK();
      /*
      Serial.write("D:");
      Serial.print(vbus_head.dest);
      Serial.write(" S:");
      Serial.print(vbus_head.source);
      Serial.write(" V:");
      Serial.print((vbus_head.version >> 4) + 48);
      Serial.write(" C:");
      Serial.print(vbus_head.command);
      Serial.write(" F:");
      Serial.print(vbus_head.frames);
      Serial.write("\n"); */
      //VBUS_DEBUG_S(" X:");

      /* Checksum Überprüfen
       * Ist sie inkorrekt können wir mit dem Head und somit auch
       * mit allen folgenden Daten nichts anfangen
       */
      if (VBus_CalcCrc((uint8_t *)&vbus_head, 0, 8) != vbus_head.checksum) {
        //Pruefsumme ist ungueltig, sync zuruecksetzen und beenden
        //Serial.print("nok");
        vbus_head.sync = 0;
        return;
      }
      //Serial.print("ok");

      //Protokollversion prüfen, aktuell wird nur 1.0 unterstuetzt
      if (vbus_head.version != VBUS_PROTO_10) {
        //Protokollversion ist ungleich 1.0, sync zuruecksetzen und beenden
        vbus_head.sync = 0;
        //VBUS_DEBUG_BREAK();
        //Serial.print("!V1.0->dropped \n");
        return;
      }

      //Prüfen, ob das Paket richtig adressiert ist
      if (vbus_head.source != VBUS_SRC || vbus_head.dest != VBUS_DST) {
        //Paket ist nicht für "mich", ignorieren
        //Serial.print("Wrong addr->dropped \n");
        //Serial.print(vbus_head.source);
        vbus_head.sync = 0;
        return;
      }

      /* Hier könnten wir noch überprüfen, ob die Frame-Länge und der Befehl stimmt
       * Ist aber nicht nötig, da in diesem Fall nur eine Paketart mit entsprechenden
       * Adressen versendet werden
       */
    }

    //Ab Offset 10 (VBUS_HEADER_LENGTH) sind wir im Datenbereich
    if (vbus_head.offset >= VBUS_HEADER_LENGTH) {
      /* Offset im Paket berechnen
       * kann man sicherlich auch komplett per Pointer machen,
       * ist mir an dieser Stelle aber ein bisschen zu unheimlich ;)
       * 10 (VBUS_HEADER_LENGTH) ist der Offset (VBUS-Header), 6 (VBUS_10_FRAME_LENGTH) die Framelänge
       */
      uint8_t packetoffset = (vbus_head.offset - VBUS_HEADER_LENGTH) % VBUS_10_FRAME_LENGTH;

      //Wert an der aktuellen Stelle in die Paketvariable schreiben
      volatile unsigned char *pos = (volatile unsigned char *)&vbus_frame;
      pos += packetoffset;
      *pos = c;

      //Frame ist fertig. Jetzt schauen, ob es gültig ist.
      //das -1 ergibt sich wegen der Basis 0!
      if (packetoffset == (VBUS_10_FRAME_LENGTH - 1)) {
        //VBUS_DEBUG_S(" F");
        //Serial.print(vbus_head.currframe);

        //Pointer aufs Ausgabestruct ermitteln
        volatile unsigned char *outptr = (volatile unsigned char *)vbus_head.od;
        outptr += vbus_head.currframe * VBUS_10_FRAME_PAYLOAD; //Frames sind 4 Byte lang, Position des Pointers setzen

        uint8_t i = VBUS_10_FRAME_PAYLOAD;
        if (VBus_CalcCrc((uint8_t *)&vbus_frame, 0, (VBUS_10_FRAME_PAYLOAD + 1)) != vbus_frame.checksum) {
          //Speicher beschreiben, wenn das Frame ungültig ist
          while (i--) {
            *outptr = VBUS_INVALID_FRAME;
            outptr++;
          }
          //VBUS_DEBUG_S("nok");
        } else {
          //Prüfsumme war korrekt -> Septett holen
          uint8_t sept = vbus_frame.septett;
          uint8_t *inptr = (uint8_t *)&vbus_frame;

          //Serial.print("ok");

          while (i--) {
            *outptr = *inptr; //Wert aus dem Frame in die Ausgabe übertragen
            //if(sept & 1) *outptr |= 0x80; //Septett hinzufuegen
            *outptr |= (sept & 1) << 7; //ein Stückchen kleiner als die obere Zeile ;-)

            //Wer den Debug ein bisschen detaillierter haben will:
            /*VBUS_DEBUG_H(*outptr);
            VBUS_DEBUG_C(' ');*/
            //Serial.print(*outptr);

            sept >>= 1; //Septett nach rechts schieben (nöchstes Bit an erste Stelle)
            outptr++;
            inptr++;
          }
        }
        vbus_head.currframe++;
        if (vbus_head.currframe == VBUS_FRAME_COUNT) {
          vbus_head.valid = 1;
          vbus_head.update = 0;
          //Serial.print("AFR\n");
        }
      }
    }
    vbus_head.offset++;
  }
}

//CRC fuer serial berechnen
uint8_t CRC_Set(uint8_t *message, uint8_t len){
  uint8_t CRC_temp;
  uint8_t CRC_i;
  CRC_temp = 0x00; // start bei Hex0 LSB
  for ( CRC_i = 0; CRC_i<len-1; CRC_i++){
      CRC_temp = (message[CRC_i])^CRC_temp;
  }
  return CRC_temp;
}

// callback for sending data
void sendData(){
  //Serial.write("sende Daten\n");
  buf[sizeof(buf)-1] = CRC_Set(buf,sizeof(buf));
  for(int i=0;i<sizeof(buf);i++){
    sbrio_out.write(buf[i]);
  }
}

void VBus_Show_Values() {
  if (blinker){
    Wire.requestFrom(0x78,2); // read 2 bytes
    temp1 = Wire.read(); // receive 1st byte
    temp2 = Wire.read(); // receive 2nd byte
    temp = (temp1 << 8 | temp2) &0x7FFE;
    digitalWrite(13, HIGH);    
    if (temp>0&&temp<65536){
      buf[0]=temp1;
      buf[1]=temp2;
    }
    blinker=false;
  }
  else{
    digitalWrite(13, LOW);
    blinker=true;
  }
  if (vbus_outdatanew.TemperatureSensor[0]>-300&&vbus_outdatanew.TemperatureSensor[0]<2000&&vbus_outdatanew.TemperatureSensor[1]>-300&&vbus_outdatanew.TemperatureSensor[1]<2000&&vbus_outdatanew.TemperatureSensor[2]==8888&&vbus_outdatanew.TemperatureSensor[3]==8888&&vbus_outdatanew.TemperatureSensor[4]==8888&&vbus_outdatanew.PWM[0]<101&&vbus_outdatanew.Pumpspeedrelay[0]<101){
  buf[2]=vbus_outdatanew.TemperatureSensor[0]>>8;
  buf[3]=vbus_outdatanew.TemperatureSensor[0];
  //Serial.print(vbus_outdatanew.TemperatureSensor[0]);
  //Serial.write(" ");
  buf[4]=vbus_outdatanew.TemperatureSensor[1]>>8;
  buf[5]=vbus_outdatanew.TemperatureSensor[1];
  //Serial.print(vbus_outdatanew.TemperatureSensor[1]);
  //Serial.write(" ");
  buf[6]=vbus_outdatanew.TemperatureSensor[2]>>8;
  buf[7]=vbus_outdatanew.TemperatureSensor[2];
  //Serial.write(" ");
  buf[8]=vbus_outdatanew.TemperatureSensor[3]>>8;
  buf[9]=vbus_outdatanew.TemperatureSensor[3];
  //Serial.write(" ");
  buf[10]=vbus_outdatanew.TemperatureSensor[4]>>8;
  buf[11]=vbus_outdatanew.TemperatureSensor[4];  
  //Serial.print(vbus_outdatanew.TemperatureSensor[2]);
  //Serial.write(" ");  
  buf[12]=vbus_outdatanew.PWM[0];
  //Serial.print(vbus_outdatanew.PWM[0]);
  //Serial.write(" ");
  buf[13]=vbus_outdatanew.Pumpspeedrelay[0];
  //Serial.print(vbus_outdatanew.Pumpspeedrelay[0]);
  //Serial.write("\n");
  //Serial.write("Daten empfangen\n");

  }
}

void setup()
{
  pinMode(13, OUTPUT);
  sbrio_out.begin(57600);
  vbus_in.begin(9600);
  //Serial.begin(9600);
  Wire.begin();
  Timer1.initialize(5*1000000);
  Timer1.attachInterrupt(sendData);
  vbus_head.update = 1;
  vbus_head.od = &vbus_outdatanew;
}

void loop()
{
  if (vbus_in.available()) {
    c = vbus_in.read();
    //Serial.print(c);
    Vbus_ProcessChar(c);
  }
  
  if (vbus_head.valid == 1) {
    vbus_head.valid = 0;
    VBus_Show_Values();
    vbus_head.update = 1;
  }  
  //memset(buf,0,sizeof(buf));   
  //Serial.println(((buf[0] << 8 | buf[1]) &0x7FFE) / 256.0 - 32.0);
  //delay(2000);
}
