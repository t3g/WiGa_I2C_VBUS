#ifndef _VBUS_H
#define _VBUS_H

#define VBUS_CRC_MAGIC 0x7F

//Syncwort
#define VBUS_SYNC 0xAA

//Citrin Solar CS 2.5
#define VBUS_SRC 0x7421
//DFA (Datenfernanzeige)
#define VBUS_DST 0x0010

#define VBUS_FRAME_COUNT 15

//Wenn das Frame ungueltig ist, wird folgender Wert in die Ausgabe geschrieben
#define VBUS_INVALID_FRAME 0xFF

//Protokollversionen
//momentan wird nur Version 1.0 unterstuetzt
#define VBUS_PROTO_10 0x10
#define VBUS_PROTO_20 0x20
#define VBUS_PROTO_30 0x30

//Laenge des Headers
#define VBUS_HEADER_LENGTH 10

//Anzahl der Nutzdaten, die ein V1.0-Paket traegt
#define VBUS_10_FRAME_PAYLOAD 4

//Gesamtgroesse eines V1.0-Pakets (Nutzdaten + Septett + Checksum)
#define VBUS_10_FRAME_LENGTH (VBUS_10_FRAME_PAYLOAD + 2)


/* Frameinhalt eines VBus 1.0-Pakets
 * offset gehoert nicht zum Paketinhalt
 */
typedef struct {
	uint8_t data[4];
	uint8_t septett;
	uint8_t checksum;
	uint8_t offset;
} t_vbus10_frame;

/* Empfangene Daten vom Geraet
 * Hier: spezifisch fuer Citrin Solar CS 2.5
 * Die Infos bzgl. Laenge, Position, etc. können den XML-Dateien 
 * %ProgramFiles%\RESOL\ServiceCenterFull\eclipse\plugins\de.resol.servicecenter.vbus.<Device>\VBusSpecification<Device>.xml
 * entnommen werden.
 * Das Resol ServiceCenter (RSC) gibt's kostenlos unter http://www.resol.de
 */
typedef struct {
	int16_t  TemperatureSensor[5];     //Offset  0-9  interessant 0-4
	int16_t  TemperatureRPS;           //Offset 10-11
	uint16_t PressureRPS;              //Offset 12-13
	uint16_t TemperatureVFS;           //Offset 14-15
	uint16_t FlowrateVFS;              //Offset 16-17
	uint16_t FlowrateV40;              //Offset 18-19
	uint8_t  Unit;                     //Offset 20	
	uint8_t  Unknown1;                 //Offset 21
	uint8_t  PWM[2];                   //Offset 22-23 interessant 0
	uint8_t  Pumpspeedrelay[4];        //Offset 24-27 interessant 0
	uint32_t Operatingsecondsrelay[4]; //Offset 28-43
	uint16_t Errors;                   //Offset 44-45
	uint16_t Status;                   //Offset 46-47
	uint32_t Heatquantity;             //Offset 48-51
	uint16_t Version;                  //Offset 52-53
	uint16_t Systemtime;               //Offset 54-55
	uint32_t Date;                     //Offset 56-59
} t_vbus_outdata;

/* Kopfinformationen des VBus-Pakets
 * sync, offset und currframe gehoeren nicht zum Paket, 
 * sondern sind Informationen, die beim Decodieren benoetigt werden
 */
typedef struct {
	uint16_t dest;
	uint16_t source;
	uint8_t version; //eigentlich nur 4 bit
	uint16_t command;
	uint8_t frames;
	uint8_t checksum;
	uint8_t currframe;
	volatile t_vbus_outdata* od;
	unsigned sync:1;
	unsigned offset:7;
	unsigned update:1;
	unsigned valid:1;
} t_vbus_head;

extern volatile t_vbus_head vbus_head;
extern t_vbus10_frame vbus_frame;

extern void Vbus_ProcessChar(uint8_t);
extern uint8_t VBus_CalcCrc(const unsigned char*, uint8_t, uint8_t);

#endif
